// =============================================================================
// DiagnosticEngine.cpp
// =============================================================================
#include "engine/diagnostic/DiagnosticEngine.h"
#include "engine/PlatformCommand.h"
#include "engine/diagnostic/G4RemoteHost.h"
#include "engine/diagnostic/G5WebsiteUrl.h"
#include "engine/runner/NetworkProbe.h"
#include "app/NativeService.h"
#include "util/Logger.h"
#include "util/PingParser.h"
#include <QtConcurrent/QtConcurrent>

DiagnosticEngine::DiagnosticEngine(std::unique_ptr<PlatformCommand> cmd, QObject* parent)
    : QObject(parent)
    , m_cmd(std::move(cmd))
{
    // Initialize native plugin (non-fatal if unavailable)
    NativeService::instance().initialize();
}

DiagnosticEngine::~DiagnosticEngine() {
    m_destroying.store(true, std::memory_order_release);
}

QFuture<DiagnosticResult> DiagnosticEngine::runTest(TestId id, const QString& target,
                                                       int fromPort, int toPort, bool useCommonPorts) {
    // Capture a raw pointer — the atomic m_destroying flag guards against
    // use-after-free during shutdown. The engine is parented to AppState so
    // it outlives workers during normal operation.
    auto* engine = this;
    return QtConcurrent::run([engine, id, target, fromPort, toPort, useCommonPorts]() -> DiagnosticResult {
        fprintf(stderr, "[TRACE] runTest lambda id=%d ENTER\n", (int)id);
        if (engine->m_destroying.load(std::memory_order_acquire))
            return DiagnosticResult::error(id, QStringLiteral("Engine shutting down"));
        TestGroup group = testGroup(id);

        // Try native plugin first
        fprintf(stderr, "[TRACE] runTest id=%d calling tryNative\n", (int)id);
        auto nativeResult = engine->tryNative(id, target, fromPort, toPort);
        if (nativeResult.has_value()) {
            auto& r = nativeResult.value();
            r.id = id;
            r.group = group;
            return r;
        }

        // Fall back to Qt C++ implementation
        switch (group) {
            case TestGroup::G1: return engine->runG1(id, target);
            case TestGroup::G2: return engine->runG2(id, target);
            case TestGroup::G3: return engine->runG3(id, target);
            case TestGroup::G4: return engine->runG4(id, target, fromPort, toPort, useCommonPorts);
            case TestGroup::G5: return engine->runG5(id, target);
        }
        return DiagnosticResult::error(id, QStringLiteral("Unknown group"));
    });
}

std::optional<DiagnosticResult> DiagnosticEngine::tryNative(TestId id, const QString& target,
                                                             int fromPort, int toPort) {
    auto& ns = NativeService::instance();
    if (!ns.isAvailable() || !ns.isNativeCapable(id))
        return std::nullopt;
    return ns.runDiagnostic(id, target, fromPort, toPort);
}

// ── G1: System & Adapters ──────────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG1(TestId id, const QString& target) {
    Q_UNUSED(target)
    auto* cmd = m_cmd.get();
    QString exe;
    QStringList args;

    switch (id) {
        case TestId::G1NetworkAdapters: {
            // Fallback: ip link (Linux) — native plugin handles primary path
            exe = cmd->shellExecutable();
            args = cmd->shellArguments(QStringLiteral("ip -br link show || ifconfig"));
            break;
        }
        case TestId::G1WifiDiagnostics: {
            exe = cmd->shellExecutable();
            args = cmd->shellArguments(QStringLiteral("iw dev 2>/dev/null || nmcli device wifi list 2>/dev/null"));
            break;
        }
        case TestId::G1ActiveConnections: {
            exe = cmd->shellExecutable();
            args = cmd->shellArguments(QStringLiteral("ss -tan 2>/dev/null || netstat -an"));
            break;
        }
        case TestId::G1DhcpStatus: {
            exe = cmd->shellExecutable();
            args = cmd->shellArguments(QStringLiteral("nmcli device show 2>/dev/null || cat /var/lib/dhcp/dhclient.leases 2>/dev/null"));
            break;
        }
        case TestId::G1IpConfiguration: {
            exe = cmd->shellExecutable();
            args = cmd->shellArguments(QStringLiteral("ip addr show 2>/dev/null && ip route show default 2>/dev/null || ifconfig"));
            break;
        }
        default: {
            // NicAdvanced, Wired — native-only for now
            return DiagnosticResult::skipped(id, QStringLiteral("Runs via native plugin"));
        }
    }

    ProcessResult r = cmd->runAndWait(exe, args, testGroupTimeoutSec(TestGroup::G1) * 1000);
    DiagnosticResult result;
    result.id = id;
    result.group = TestGroup::G1;
    result.status = r.isSuccess() ? TestStatus::Pass : TestStatus::Warning;
    result.summary = r.isSuccess() ? QStringLiteral("OK") : QStringLiteral("Command failed");
    result.rawOutput = r.stdoutStr;
    result.errorOutput = r.stderrStr;
    result.timestamp = QDateTime::currentDateTime();
    return result;
}

// ── G2: Connectivity & Security ────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG2(TestId id, const QString& target) {
    Q_UNUSED(target)
    auto* cmd = m_cmd.get();

    switch (id) {
        case TestId::G2RoutingTable: {
            ProcessResult r = cmd->runAndWait(cmd->shellExecutable(),
                cmd->shellArguments(QStringLiteral("ip route show 2>/dev/null || route print")),
                testGroupTimeoutSec(TestGroup::G2) * 1000);
            DiagnosticResult res;
            res.id = id;
            res.group = TestGroup::G2;
            res.status = r.isSuccess() ? TestStatus::Pass : TestStatus::Warning;
            res.summary = r.isSuccess() ? QStringLiteral("Route table read") : QStringLiteral("Failed");
            res.rawOutput = r.stdoutStr;
            res.timestamp = QDateTime::currentDateTime();
            return res;
        }
        case TestId::G2ArpTable: {
            ProcessResult r = cmd->runAndWait(cmd->shellExecutable(),
                cmd->shellArguments(QStringLiteral("ip neigh show 2>/dev/null || arp -a")),
                testGroupTimeoutSec(TestGroup::G2) * 1000);
            DiagnosticResult res;
            res.id = id;
            res.group = TestGroup::G2;
            res.status = r.isSuccess() ? TestStatus::Pass : TestStatus::Warning;
            res.summary = r.isSuccess() ? QStringLiteral("ARP table read") : QStringLiteral("Failed");
            res.rawOutput = r.stdoutStr;
            res.timestamp = QDateTime::currentDateTime();
            return res;
        }
        default: {
            // NetworkProfile, TcpSettings, DefaultGateway, ProxySettings — native plugin
            return DiagnosticResult::skipped(id, QStringLiteral("Runs via native plugin"));
        }
    }
}

// ── G3: Internet & DNS ─────────────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG3(TestId id, const QString& target) {
    Q_UNUSED(target)
    auto* cmd = m_cmd.get();

    switch (id) {
        case TestId::G3NetskopeStatus: {
            ProcessResult r = cmd->runAndWait(cmd->shellExecutable(),
                cmd->shellArguments(QStringLiteral("pgrep nsproxy 2>/dev/null || tasklist /FI \"IMAGENAME eq nsproxy.exe\" 2>/dev/null")),
                5000);
            DiagnosticResult res;
            res.id = id;
            res.group = TestGroup::G3;
            bool found = r.exitCode == 0 || r.stdoutStr.contains("nsproxy", Qt::CaseInsensitive);
            res.status = found ? TestStatus::Pass : TestStatus::Warning;
            res.summary = found ? QStringLiteral("netskope client running") : QStringLiteral("netskope not detected");
            res.timestamp = QDateTime::currentDateTime();
            return res;
        }
        case TestId::G3DnsServers: {
            ProcessResult r = cmd->runAndWait(cmd->shellExecutable(),
                cmd->shellArguments(QStringLiteral("cat /etc/resolv.conf 2>/dev/null || ipconfig /all")),
                testGroupTimeoutSec(TestGroup::G3) * 1000);
            DiagnosticResult res;
            res.id = id;
            res.group = TestGroup::G3;
            res.status = r.isSuccess() ? TestStatus::Pass : TestStatus::Warning;
            res.summary = QStringLiteral("DNS servers enumerated");
            res.rawOutput = r.stdoutStr;
            res.timestamp = QDateTime::currentDateTime();
            return res;
        }
        case TestId::G3InternetConnectivity: {
            // Ping 223.5.5.5 (Alibaba DNS) as connectivity check
            ProcessResult r = cmd->runAndWait(cmd->shellExecutable(),
                cmd->shellArguments(QStringLiteral("ping -c 4 -W 3 223.5.5.5 2>/dev/null || ping -n 4 -w 3000 223.5.5.5")),
                15000);
            DiagnosticResult res;
            res.id = id;
            res.group = TestGroup::G3;
            auto pingResult = PingParser::parse(r.stdoutStr);
            res.status = (r.isSuccess() && pingResult.lossPercent < 5.0) ? TestStatus::Pass : TestStatus::Warning;
            res.summary = pingResult.lossPercent < 5.0
                ? QStringLiteral("Internet reachable (loss %1%)").arg(pingResult.lossPercent, 0, 'f', 1)
                : QStringLiteral("Packet loss %1%").arg(pingResult.lossPercent, 0, 'f', 1);
            res.rawOutput = r.stdoutStr;
            res.timestamp = QDateTime::currentDateTime();
            return res;
        }
        default: {
            // DnsCache, DnsPollution, SpeedTest — native plugin or G5-like HTTP (Phase 4)
            return DiagnosticResult::skipped(id, QStringLiteral("Runs via native plugin or G5 handler"));
        }
    }
}

// ── G4: Remote Host ────────────────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG4(TestId id, const QString& target,
                                            int fromPort, int toPort, bool useCommonPorts) {
    auto* cmd = m_cmd.get();
    switch (id) {
        case TestId::G4DnsResolution: return G4RemoteHost::dnsResolution(target, cmd);
        case TestId::G4Ping:          return G4RemoteHost::ping(target, cmd);
        case TestId::G4Traceroute:    return G4RemoteHost::traceroute(target, cmd);
        case TestId::G4PathPing:      return G4RemoteHost::pathPing(target, cmd);
        case TestId::G4MtuDiscovery:  return G4RemoteHost::mtuDiscovery(target, cmd);
        case TestId::G4PortScan: {
            auto commonPorts = useCommonPorts
                ? NetworkProbe::commonDiagnosticPorts()
                : QVector<int>{};
            QElapsedTimer t; t.start();
            auto results = NetworkProbe::portScan(target, commonPorts, fromPort, toPort, 2000, 20);
            DiagnosticResult r;
            r.id = id; r.group = TestGroup::G4;
            r.durationMs = t.elapsed(); r.timestamp = QDateTime::currentDateTime();
            int openCount = 0;
            for (const auto& e : results) {
                if (e.open) {
                    ++openCount;
                    r.properties.append({QStringLiteral("Port %1").arg(e.port), "OPEN"});
                }
            }
            r.status = openCount > 0 ? TestStatus::Pass : TestStatus::Info;
            r.summary = QStringLiteral("%1 ports open").arg(openCount);
            return r;
        }
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("Unknown G4 test"));
    }
}

// ── G5: Website / URL ──────────────────────────────────────────────────────

DiagnosticResult DiagnosticEngine::runG5(TestId id, const QString& target) {
    switch (id) {
        case TestId::G5UrlParsing:      return G5WebsiteUrl::urlParsing(target);
        case TestId::G5TcpConnect:      return G5WebsiteUrl::tcpConnect(target);
        case TestId::G5ServiceBanner:   return G5WebsiteUrl::serviceBanner(target);
        case TestId::G5CurlVerbose:     return G5WebsiteUrl::curlVerbose(target);
        case TestId::G5HttpHeaders:     return G5WebsiteUrl::httpHeaders(target);
        case TestId::G5SecurityHeaders: return G5WebsiteUrl::securityHeaders(target);
        case TestId::G5SslCertificate:  return G5WebsiteUrl::sslCertificate(target);
        case TestId::G5HttpRedirect:    return G5WebsiteUrl::httpRedirect(target);
        case TestId::G5HttpCompression: return G5WebsiteUrl::httpCompression(target);
        case TestId::G5HttpTiming:      return G5WebsiteUrl::httpTiming(target);
        case TestId::G5FtpDiagnostics:  return G5WebsiteUrl::ftpDiagnostics(target);
        case TestId::G5SshDiagnostics:  return G5WebsiteUrl::sshDiagnostics(target);
        case TestId::G5EmailDiagnostics:return G5WebsiteUrl::emailDiagnostics(target);
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("Unknown G5 test"));
    }
}
