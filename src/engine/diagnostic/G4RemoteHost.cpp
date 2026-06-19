#include "engine/diagnostic/G4RemoteHost.h"
#include "engine/PlatformCommand.h"
#include "engine/runner/NetworkProbe.h"
#include "util/PingParser.h"
#include "util/Logger.h"
#include <QHostInfo>
#include <QElapsedTimer>

namespace G4RemoteHost {

static ResultProperty prop(const QString& label, const QString& value) {
    return ResultProperty(label, value);
}

// ── DNS Resolution ────────────────────────────────────────────────────
DiagnosticResult dnsResolution(const QString& target, PlatformCommand* cmd) {
    DiagnosticResult r;
    r.id = TestId::G4DnsResolution;
    r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) {
        r.status = TestStatus::Skipped;
        r.summary = QStringLiteral("No target specified");
        return r;
    }
    QElapsedTimer t; t.start();
    QHostInfo info = QHostInfo::fromName(target);
    int ipv4Count = 0, ipv6Count = 0;
    QStringList ipList;
    for (const auto& addr : info.addresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol) ++ipv4Count;
        else if (addr.protocol() == QAbstractSocket::IPv6Protocol) ++ipv6Count;
        ipList.append(addr.toString());
    }
    QString digOutput;
    if (cmd) {
        ProcessResult pr;
#ifdef Q_OS_WIN
        pr = cmd->runAndWait(QStringLiteral("cmd.exe"), {QStringLiteral("/C"), QStringLiteral("nslookup %1").arg(target)}, 10000);
#else
        pr = cmd->runAndWait(cmd->shellExecutable(), cmd->shellArguments(QStringLiteral("dig +short %1 2>/dev/null || nslookup %1").arg(target)), 10000);
#endif
        digOutput = pr.stdoutStr;
    }
    r.durationMs = t.elapsed();
    r.properties.append(prop("Target", target));
    r.properties.append(prop("IpCount", QString::number(info.addresses().size())));
    r.properties.append(prop("Ipv4Count", QString::number(ipv4Count)));
    r.properties.append(prop("Ipv6Count", QString::number(ipv6Count)));
    r.properties.append(prop("IpList", ipList.join(", ")));
    if (info.addresses().isEmpty()) {
        r.status = TestStatus::Fail;
        r.summary = QStringLiteral("DNS resolution failed: %1").arg(info.errorString());
    } else {
        r.status = TestStatus::Pass;
        r.summary = QStringLiteral("Resolved to %1 address(es)").arg(info.addresses().size());
    }
    r.rawOutput = digOutput;
    return r;
}

// ── Ping ──────────────────────────────────────────────────────────────
DiagnosticResult ping(const QString& target, PlatformCommand* cmd) {
    DiagnosticResult r;
    r.id = TestId::G4Ping; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    if (!cmd) return DiagnosticResult::error(TestId::G4Ping, QStringLiteral("No PlatformCommand"));
    QElapsedTimer t; t.start();
    ProcessResult pr;
#ifdef Q_OS_WIN
    pr = cmd->runAndWait(QStringLiteral("ping"), {QStringLiteral("-n"),QStringLiteral("10"),QStringLiteral("-w"),QStringLiteral("3000"),target}, 60000);
#else
    pr = cmd->runAndWait(QStringLiteral("ping"), {QStringLiteral("-c"),QStringLiteral("10"),QStringLiteral("-W"),QStringLiteral("3"),target}, 60000);
#endif
    r.durationMs = t.elapsed();
    r.rawOutput = pr.stdoutStr;
    // Check for process timeout before parsing
    if (pr.timedOut) {
        r.status = TestStatus::Error;
        r.summary = QStringLiteral("Ping timed out after %1s").arg(r.durationMs / 1000);
        return r;
    }
    auto pr2 = PingParser::parse(pr.stdoutStr);
    r.properties.append(prop("Target", target));
    r.properties.append(prop("Sent", QString::number(pr2.sent > 0 ? pr2.sent : 10)));
    r.properties.append(prop("Received", QString::number(pr2.received)));
    r.properties.append(prop("LossPercent", QString::number(pr2.lossPercent, 'f', 1) + "%"));
    r.properties.append(prop("AvgMs", QString::number(pr2.avgMs, 'f', 1)));
    r.properties.append(prop("MinMs", QString::number(pr2.minMs, 'f', 1)));
    r.properties.append(prop("MaxMs", QString::number(pr2.maxMs, 'f', 1)));
    if (pr2.lossPercent >= 100.0) { r.status = TestStatus::Fail; r.summary = QStringLiteral("100% packet loss"); }
    else if (pr2.lossPercent >= 50.0) { r.status = TestStatus::Fail; r.summary = QStringLiteral("%1% loss").arg(pr2.lossPercent,0,'f',1); }
    else if (pr2.lossPercent > 0) { r.status = TestStatus::Warning; r.summary = QStringLiteral("%1% loss, avg %2ms").arg(pr2.lossPercent,0,'f',1).arg(pr2.avgMs,0,'f',1); }
    else { r.status = TestStatus::Pass; r.summary = QStringLiteral("0% loss, avg %1ms").arg(pr2.avgMs,0,'f',1); }
    return r;
}

// ── Traceroute ────────────────────────────────────────────────────────
DiagnosticResult traceroute(const QString& target, PlatformCommand* cmd) {
    DiagnosticResult r;
    r.id = TestId::G4Traceroute; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    if (!cmd) return DiagnosticResult::error(TestId::G4Traceroute, QStringLiteral("No PlatformCommand"));
    QElapsedTimer t; t.start();
    ProcessResult pr;
#ifdef Q_OS_WIN
    pr = cmd->runAndWait(QStringLiteral("tracert"), {QStringLiteral("-d"),QStringLiteral("-h"),QStringLiteral("30"),QStringLiteral("-w"),QStringLiteral("3000"),target}, 180000);
#else
    QString exe = cmd->findExecutable("traceroute"); if (exe.isEmpty()) exe = QStringLiteral("traceroute");
    pr = cmd->runAndWait(exe, {QStringLiteral("-I"),QStringLiteral("-m"),QStringLiteral("30"),QStringLiteral("-w"),QStringLiteral("3"),target}, 180000);
#endif
    r.durationMs = t.elapsed(); r.rawOutput = pr.stdoutStr;
    auto tr = PingParser::parseTraceroute(pr.stdoutStr);
    ResultProperty hopList("Hops", "");
    for (const auto& hop : tr.hops) {
        auto fmtRtt = [](double ms) -> QString {
            if (ms < 0) return QStringLiteral("*");
            if (ms < 1.0) return QStringLiteral("<1");
            return QString::number(ms, 'f', 1);
        };
        QString val = hop.timedOut ? QStringLiteral("* * *")
            : QStringLiteral("%1 (%2/%3/%4 ms)").arg(hop.ip, fmtRtt(hop.rtt1Ms), fmtRtt(hop.rtt2Ms), fmtRtt(hop.rtt3Ms));
        hopList.children.append(ResultProperty(QStringLiteral("Hop %1").arg(hop.hop), val));
    }
    r.properties.append(prop("Target", target));
    r.properties.append(prop("HopCount", QString::number(tr.hopCount)));
    r.properties.append(prop("TimeoutHops", QString::number(tr.timeoutHops)));
    r.properties.append(prop("ReachedTarget", tr.reachedTarget ? "Yes" : "No"));
    r.properties.append(hopList);
    if (tr.reachedTarget) { r.status = TestStatus::Pass; r.summary = QStringLiteral("Target reached in %1 hops").arg(tr.hopCount); }
    else if (tr.hopCount > 0) { r.status = TestStatus::Warning; r.summary = QStringLiteral("Partial path (%1 hops)").arg(tr.hopCount); }
    else { r.status = TestStatus::Fail; r.summary = QStringLiteral("No hops discovered"); }
    return r;
}

// ── PathPing ──────────────────────────────────────────────────────────
DiagnosticResult pathPing(const QString& target, PlatformCommand* cmd) {
    DiagnosticResult r;
    r.id = TestId::G4PathPing; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    if (!cmd) return DiagnosticResult::error(TestId::G4PathPing, QStringLiteral("No PlatformCommand"));
    QElapsedTimer t; t.start();
    auto tr = traceroute(target, cmd);
    int hopCount = 0;
    // Extract hop count from traceroute's HopList property
    for (const auto& p : tr.properties) {
        if (p.label == "HopCount") hopCount = p.value.toInt();
    }
    QVector<ResultProperty> perHopStats;
    double totalLoss = 0.0; int pingedHops = 0;
    // Find hop IPs from the HopList
    for (const auto& p : tr.properties) {
        if (p.label == "Hops") {
            for (const auto& hop : p.children) {
                // Bail out if total elapsed exceeds G4 group timeout
                if (t.elapsed() > testGroupTimeoutSec(TestGroup::G4) * 1000) {
                    r.status = TestStatus::Warning;
                    r.summary = QStringLiteral("PathPing timeout after %1s").arg(t.elapsed()/1000);
                    return r;
                }
                QString ip = hop.value.split("(").value(0).trimmed();
                if (!ip.isEmpty() && !ip.contains("*")) {
                    auto pr2 = ping(ip, cmd);
                    double loss = 0;
                    for (const auto& pp : pr2.properties) {
                        if (pp.label == "LossPercent") {
                            QString v = pp.value; v.replace("%", ""); loss = v.toDouble();
                            break;
                        }
                    }
                    totalLoss += loss; ++pingedHops;
                    perHopStats.append(ResultProperty(ip, QString::number(loss,'f',1) + "% loss"));
                }
            }
        }
    }
    r.durationMs = t.elapsed();
    double avgLoss = pingedHops > 0 ? totalLoss / pingedHops : 100.0;
    r.properties.append(prop("Target", target));
    r.properties.append(prop("HopCount", QString::number(hopCount)));
    r.properties.append(prop("AverageLossPercent", QString::number(avgLoss,'f',1) + "%"));
    ResultProperty stats("PerHopStats", ""); stats.children = perHopStats; r.properties.append(stats);
    if (avgLoss <= 10.0) { r.status = TestStatus::Pass; r.summary = QStringLiteral("Avg loss %1%%").arg(avgLoss,0,'f',1); }
    else if (pingedHops > 0) { r.status = TestStatus::Warning; r.summary = QStringLiteral("High avg loss %1%%").arg(avgLoss,0,'f',1); }
    else { r.status = TestStatus::Fail; r.summary = QStringLiteral("No hops"); }
    return r;
}

// ── MTU Discovery ─────────────────────────────────────────────────────
DiagnosticResult mtuDiscovery(const QString& target, PlatformCommand* cmd) {
    DiagnosticResult r;
    r.id = TestId::G4MtuDiscovery; r.group = TestGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) { r.status = TestStatus::Skipped; r.summary = QStringLiteral("No target"); return r; }
    if (!cmd) return DiagnosticResult::error(TestId::G4MtuDiscovery, QStringLiteral("No PlatformCommand"));
    QElapsedTimer t; t.start();
    int low = 472, high = 1472, lastSuccess = 472, probes = 0;
    while (low <= high) {
        int mid = (low + high) / 2; ++probes;
        ProcessResult pr;
#ifdef Q_OS_WIN
        pr = cmd->runAndWait(QStringLiteral("ping"), {QStringLiteral("-n"),QStringLiteral("1"),QStringLiteral("-f"),QStringLiteral("-l"),QString::number(mid),target}, 5000);
#elif defined(Q_OS_MACOS)
        pr = cmd->runAndWait(QStringLiteral("ping"), {QStringLiteral("-c"),QStringLiteral("1"),QStringLiteral("-D"),QStringLiteral("-s"),QString::number(mid),target}, 5000);
#else
        pr = cmd->runAndWait(QStringLiteral("ping"), {QStringLiteral("-c"),QStringLiteral("1"),QStringLiteral("-M"),QStringLiteral("do"),QStringLiteral("-s"),QString::number(mid),target}, 5000);
#endif
        if (pr.exitCode == 0) { lastSuccess = mid; low = mid + 1; } else { high = mid - 1; }
    }
    int mtu = lastSuccess + 28;
    r.durationMs = t.elapsed();
    r.properties.append(prop("Target", target));
    r.properties.append(prop("MtuValue", QString::number(mtu)));
    r.properties.append(prop("TestPacketsSent", QString::number(probes)));
    if (mtu >= 1500) { r.status = TestStatus::Pass; r.summary = QStringLiteral("MTU %1 (standard)").arg(mtu); }
    else if (mtu >= 1280) { r.status = TestStatus::Warning; r.summary = QStringLiteral("MTU %1 (below 1500)").arg(mtu); }
    else if (mtu > 500) { r.status = TestStatus::Warning; r.summary = QStringLiteral("Low MTU: %1").arg(mtu); }
    else { r.status = TestStatus::Skipped; r.summary = QStringLiteral("Cannot determine MTU"); }
    return r;
}

} // namespace G4RemoteHost
