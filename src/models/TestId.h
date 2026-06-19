// =============================================================================
// TestId.h — Diagnostic test identifiers and enums
//
// Central registry of all diagnostic tests, groups, statuses, and metadata.
// =============================================================================
#pragma once

#include <QString>
#include <QVector>
#include <QMap>

// ── Test Group ──────────────────────────────────────────────────────────────
enum class TestGroup {
    G1, G2, G3, G4, G5
};

inline QString testGroupLabel(TestGroup g) {
    switch (g) {
        case TestGroup::G1: return QStringLiteral("System & Adapters");
        case TestGroup::G2: return QStringLiteral("Connectivity & Security");
        case TestGroup::G3: return QStringLiteral("Internet & DNS");
        case TestGroup::G4: return QStringLiteral("Remote Host");
        case TestGroup::G5: return QStringLiteral("Website / URL");
    }
    return {};
}

inline int testGroupTimeoutSec(TestGroup g) {
    switch (g) {
        case TestGroup::G1: return 120;
        case TestGroup::G2: return 120;
        case TestGroup::G3: return 120;
        case TestGroup::G4: return 600;
        case TestGroup::G5: return 120;
    }
    return 120;
}

// ── Test Status ─────────────────────────────────────────────────────────────
enum class TestStatus {
    Pass, Warning, Fail, Skipped, Error, Info
};

inline QString testStatusIcon(TestStatus s) {
    switch (s) {
        case TestStatus::Pass:    return QStringLiteral("✔");  // ✓
        case TestStatus::Warning: return QStringLiteral("⚠");  // ⚠
        case TestStatus::Fail:    return QStringLiteral("✘");  // ✘
        case TestStatus::Skipped: return QStringLiteral("○");  // ○
        case TestStatus::Error:   return QStringLiteral("E");
        case TestStatus::Info:    return QStringLiteral("ⓘ");  // ⓘ
    }
    return {};
}

// ── Test ID (38 values) ─────────────────────────────────────────────────────
enum class TestId {
    // G1 — System & Adapters (7)
    G1NetworkAdapters,
    G1NicAdvanced,
    G1WifiDiagnostics,
    G1WiredDiagnostics,
    G1DhcpStatus,
    G1IpConfiguration,
    G1ActiveConnections,

    // G2 — Connectivity & Security (6)
    G2NetworkProfile,
    G2TcpSettings,
    G2DefaultGateway,
    G2RoutingTable,
    G2ArpTable,
    G2ProxySettings,

    // G3 — Internet & DNS (6)
    G3NetskopeStatus,
    G3DnsServers,
    G3DnsCache,
    G3DnsPollution,
    G3InternetConnectivity,
    G3InternetSpeedTest,

    // G4 — Remote Host (6)
    G4DnsResolution,
    G4Ping,
    G4Traceroute,
    G4PathPing,
    G4MtuDiscovery,
    G4PortScan,

    // G5 — Website / URL (13)
    G5UrlParsing,
    G5TcpConnect,
    G5ServiceBanner,
    G5CurlVerbose,
    G5HttpHeaders,
    G5SecurityHeaders,
    G5SslCertificate,
    G5HttpRedirect,
    G5HttpCompression,
    G5HttpTiming,
    G5FtpDiagnostics,
    G5SshDiagnostics,
    G5EmailDiagnostics,
};

// ── TestId metadata ─────────────────────────────────────────────────────────
inline TestGroup testGroup(TestId id) {
    switch (id) {
        case TestId::G1NetworkAdapters:
        case TestId::G1NicAdvanced:
        case TestId::G1WifiDiagnostics:
        case TestId::G1WiredDiagnostics:
        case TestId::G1DhcpStatus:
        case TestId::G1IpConfiguration:
        case TestId::G1ActiveConnections:
            return TestGroup::G1;
        case TestId::G2NetworkProfile:
        case TestId::G2TcpSettings:
        case TestId::G2DefaultGateway:
        case TestId::G2RoutingTable:
        case TestId::G2ArpTable:
        case TestId::G2ProxySettings:
            return TestGroup::G2;
        case TestId::G3NetskopeStatus:
        case TestId::G3DnsServers:
        case TestId::G3DnsCache:
        case TestId::G3DnsPollution:
        case TestId::G3InternetConnectivity:
        case TestId::G3InternetSpeedTest:
            return TestGroup::G3;
        case TestId::G4DnsResolution:
        case TestId::G4Ping:
        case TestId::G4Traceroute:
        case TestId::G4PathPing:
        case TestId::G4MtuDiscovery:
        case TestId::G4PortScan:
            return TestGroup::G4;
        case TestId::G5UrlParsing:
        case TestId::G5TcpConnect:
        case TestId::G5ServiceBanner:
        case TestId::G5CurlVerbose:
        case TestId::G5HttpHeaders:
        case TestId::G5SecurityHeaders:
        case TestId::G5SslCertificate:
        case TestId::G5HttpRedirect:
        case TestId::G5HttpCompression:
        case TestId::G5HttpTiming:
        case TestId::G5FtpDiagnostics:
        case TestId::G5SshDiagnostics:
        case TestId::G5EmailDiagnostics:
            return TestGroup::G5;
    }
    return TestGroup::G1;
}

inline QString testIdLabelKey(TestId id) {
    switch (id) {
        case TestId::G1NetworkAdapters:     return QStringLiteral("test_g1_adapters");
        case TestId::G1NicAdvanced:         return QStringLiteral("test_g1_nic_advanced");
        case TestId::G1WifiDiagnostics:     return QStringLiteral("test_g1_wifi");
        case TestId::G1WiredDiagnostics:    return QStringLiteral("test_g1_wired");
        case TestId::G1DhcpStatus:          return QStringLiteral("test_g1_dhcp");
        case TestId::G1IpConfiguration:     return QStringLiteral("test_g1_ipconfig");
        case TestId::G1ActiveConnections:   return QStringLiteral("test_g1_connections");
        case TestId::G2NetworkProfile:      return QStringLiteral("test_g2_profile");
        case TestId::G2TcpSettings:         return QStringLiteral("test_g2_tcp");
        case TestId::G2DefaultGateway:      return QStringLiteral("test_g2_gateway");
        case TestId::G2RoutingTable:        return QStringLiteral("test_g2_routing");
        case TestId::G2ArpTable:            return QStringLiteral("test_g2_arp");
        case TestId::G2ProxySettings:       return QStringLiteral("test_g2_proxy");
        case TestId::G3NetskopeStatus:      return QStringLiteral("test_g3_netskope");
        case TestId::G3DnsServers:          return QStringLiteral("test_g3_dns_servers");
        case TestId::G3DnsCache:            return QStringLiteral("test_g3_dns_cache");
        case TestId::G3DnsPollution:        return QStringLiteral("test_g3_dns_pollution");
        case TestId::G3InternetConnectivity: return QStringLiteral("test_g3_internet");
        case TestId::G3InternetSpeedTest:   return QStringLiteral("test_g3_speed");
        case TestId::G4DnsResolution:       return QStringLiteral("test_g4_dns");
        case TestId::G4Ping:                return QStringLiteral("test_g4_ping");
        case TestId::G4Traceroute:          return QStringLiteral("test_g4_traceroute");
        case TestId::G4PathPing:            return QStringLiteral("test_g4_pathping");
        case TestId::G4MtuDiscovery:        return QStringLiteral("test_g4_mtu");
        case TestId::G4PortScan:            return QStringLiteral("test_g4_portscan");
        case TestId::G5UrlParsing:          return QStringLiteral("test_g5_url_parse");
        case TestId::G5TcpConnect:          return QStringLiteral("test_g5_tcp_connect");
        case TestId::G5ServiceBanner:       return QStringLiteral("test_g5_banner");
        case TestId::G5CurlVerbose:         return QStringLiteral("test_g5_curl");
        case TestId::G5HttpHeaders:         return QStringLiteral("test_g5_headers");
        case TestId::G5SecurityHeaders:     return QStringLiteral("test_g5_security");
        case TestId::G5SslCertificate:      return QStringLiteral("test_g5_ssl");
        case TestId::G5HttpRedirect:        return QStringLiteral("test_g5_redirect");
        case TestId::G5HttpCompression:     return QStringLiteral("test_g5_compression");
        case TestId::G5HttpTiming:          return QStringLiteral("test_g5_timing");
        case TestId::G5FtpDiagnostics:      return QStringLiteral("test_g5_ftp");
        case TestId::G5SshDiagnostics:      return QStringLiteral("test_g5_ssh");
        case TestId::G5EmailDiagnostics:    return QStringLiteral("test_g5_email");
    }
    return {};
}

// Convert our TestId to the native plugin's NdDiagnosticId (0-30).
// Returns -1 for tests that have no native plugin implementation.
inline int testIdToNativeId(TestId id) {
    switch (id) {
        case TestId::G1NetworkAdapters:     return 0;
        case TestId::G1NicAdvanced:         return 1;
        case TestId::G1WifiDiagnostics:     return 2;
        case TestId::G1WiredDiagnostics:    return 3;
        case TestId::G1DhcpStatus:          return 4;
        case TestId::G1IpConfiguration:     return 5;
        case TestId::G1ActiveConnections:   return 6;
        case TestId::G2NetworkProfile:      return 7;
        case TestId::G2TcpSettings:         return 8;
        case TestId::G2DefaultGateway:      return 9;
        case TestId::G2RoutingTable:        return 10;
        case TestId::G2ArpTable:            return 11;
        case TestId::G2ProxySettings:       return 12;
        case TestId::G3NetskopeStatus:      return 13;
        case TestId::G3DnsServers:          return 14;
        case TestId::G3DnsCache:            return 15;
        case TestId::G3DnsPollution:        return 16;
        case TestId::G3InternetConnectivity: return 17;
        case TestId::G3InternetSpeedTest:   return 18;
        case TestId::G4DnsResolution:       return 19;
        case TestId::G4Ping:                return 20;
        case TestId::G4Traceroute:          return 21;
        case TestId::G4PathPing:            return 22;
        case TestId::G4MtuDiscovery:        return 23;
        case TestId::G4PortScan:            return 24;
        case TestId::G5TcpConnect:          return 25;
        case TestId::G5ServiceBanner:       return 26;
        case TestId::G5FtpDiagnostics:      return 27;
        case TestId::G5SshDiagnostics:      return 28;
        case TestId::G5EmailDiagnostics:    return 29;
        // G5 HTTP tests — no native implementation
        case TestId::G5UrlParsing:
        case TestId::G5CurlVerbose:
        case TestId::G5HttpHeaders:
        case TestId::G5SecurityHeaders:
        case TestId::G5SslCertificate:
        case TestId::G5HttpRedirect:
        case TestId::G5HttpCompression:
        case TestId::G5HttpTiming:
            return -1;
    }
    return -1;
}

// ── Utility: all test IDs ───────────────────────────────────────────────────
inline const QVector<TestId>& allTestIds() {
    static const QVector<TestId> ids = {
        TestId::G1NetworkAdapters, TestId::G1NicAdvanced, TestId::G1WifiDiagnostics,
        TestId::G1WiredDiagnostics, TestId::G1DhcpStatus, TestId::G1IpConfiguration,
        TestId::G1ActiveConnections,
        TestId::G2NetworkProfile, TestId::G2TcpSettings, TestId::G2DefaultGateway,
        TestId::G2RoutingTable, TestId::G2ArpTable, TestId::G2ProxySettings,
        TestId::G3NetskopeStatus, TestId::G3DnsServers, TestId::G3DnsCache,
        TestId::G3DnsPollution, TestId::G3InternetConnectivity, TestId::G3InternetSpeedTest,
        TestId::G4DnsResolution, TestId::G4Ping, TestId::G4Traceroute,
        TestId::G4PathPing, TestId::G4MtuDiscovery, TestId::G4PortScan,
        TestId::G5UrlParsing, TestId::G5TcpConnect, TestId::G5ServiceBanner,
        TestId::G5CurlVerbose, TestId::G5HttpHeaders, TestId::G5SecurityHeaders,
        TestId::G5SslCertificate, TestId::G5HttpRedirect, TestId::G5HttpCompression,
        TestId::G5HttpTiming, TestId::G5FtpDiagnostics, TestId::G5SshDiagnostics,
        TestId::G5EmailDiagnostics,
    };
    return ids;
}

inline const QVector<TestId>& testIdsForGroup(TestGroup g) {
    static const QMap<TestGroup, QVector<TestId>> cache = []() {
        QMap<TestGroup, QVector<TestId>> m;
        for (auto id : allTestIds())
            m[testGroup(id)].append(id);
        return m;
    }();
    static const QVector<TestId> empty;
    auto it = cache.find(g);
    return (it != cache.end()) ? *it : empty;
}
