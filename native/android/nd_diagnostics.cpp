// =============================================================================
// Android diagnostics — /proc + getifaddrs + JNI system properties
// No popen for shell commands. Uses fopen for /proc files.
// dumpsys still required for WiFi/Telephony (Android system service, not shell)
// =============================================================================
#include "../linux/nd_network_adapters.h"
#include "../linux/nd_wifi.h"
#include "../linux/nd_wired.h"
#include "../linux/nd_g2.h"
#include "../linux/nd_g3_g4.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

// Android system property (libc internal)
extern "C" int __system_property_get(const char* name, char* value);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int readProcFile(const char* path, char* buf, int maxLen) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int total = 0;
    while (total < maxLen - 1) {
        size_t n = fread(buf + total, 1, maxLen - 1 - total, f);
        if (n == 0) break;
        total += (int)n;
    }
    buf[total] = 0;
    fclose(f);
    return total;
}

static bool tcpConnect(const char* host, int port, int timeoutSec) {
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0) return false;
    int sock = socket(res->ai_family, SOCK_STREAM, 0);
    if (sock < 0) { freeaddrinfo(res); return false; }
    struct timeval tv = {timeoutSec, 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    bool ok = connect(sock, res->ai_addr, res->ai_addrlen) == 0;
    close(sock);
    freeaddrinfo(res);
    return ok;
}

// Run dumpsys (Android-only system service, NOT a shell dependency)
// Uses popen internally since dumpsys must be called via /system/bin/dumpsys.
static FILE* dumpsys(const char* args) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/system/bin/dumpsys %s 2>/dev/null", args);
    return popen(cmd, "r");
}

// ===========================================================================
// G1 — Network Adapters via /proc/net/dev (no popen for cat)
// ===========================================================================
NdDiagnosticResult* NdNetworkAdapterDiagnosticAndroid::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t c = 0;
    char raw[4096] = {}; int rl = 0, up = 0, total = 0;

    // Read /proc/net/dev directly (no popen "cat")
    char buf[8192] = {};
    int n = readProcFile("/proc/net/dev", buf, sizeof(buf));
    if (n > 0) {
        char* line = buf;
        char* nextLine;
        bool header = true;
        while (line && *line) {
            nextLine = strchr(line, '\n');
            if (nextLine) *nextLine++ = 0;
            if (header) { header = false; line = nextLine; continue; } // skip header
            if (strlen(line) < 5) { line = nextLine; continue; }

            total++;
            char iface[32] = {};
            const char* p2 = line + strspn(line, " \t");
            const char* colon = strchr(p2, ':');
            if (colon) {
                size_t len = colon - p2;
                if (len >= sizeof(iface)) len = sizeof(iface) - 1;
                memcpy(iface, p2, len);
            }
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%-10s\n", iface);
            addProperty(p, c, iface, iface[0] ? "present" : "unknown");
            // Note: /proc/net/dev lists all interfaces regardless of carrier state.
            // We report all as "present"; actual UP/DOWN requires ioctl or netlink.
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d interface(s) found", total);
    return makeResult(ND_DIAG_NETWORK_ADAPTERS, 0,
                      total > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, sum,
                      "/proc/net/dev (no popen cat)", d, p, c, raw);
}

// ===========================================================================
// G1.3 — WiFi via dumpsys wifi (Android system service)
// ===========================================================================
const NdPermissionInfo _androidWifiPerms[] = {
    {"wifi.ssid", ND_PERM_UNKNOWN, "WiFi SSID",
     "ACCESS_WIFI_STATE permission"},
    {"wifi.signal", ND_PERM_UNKNOWN, "WiFi Signal",
     "ACCESS_WIFI_STATE + Location (Android 8+)"}
};
const NdPermissionInfo* NdWifiDiagAndroid::requiredPermissions(int* c) const {
    *c = 2; return _androidWifiPerms;
}

NdDiagnosticResult* NdWifiDiagAndroid::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[2048] = {}; int rl = 0;

    // dumpsys wifi — Android system service, the standard API for WiFi info
    FILE* fp = dumpsys("wifi");
    if (fp) {
        char ln[256];
        while (fgets(ln, sizeof(ln), fp)) {
            if (strstr(ln, "SSID") || strstr(ln, "RSSI") || strstr(ln, "BSSID") ||
                strstr(ln, "frequency") || strstr(ln, "link speed")) {
                rl += snprintf(raw + rl, sizeof(raw) - rl, "%s", ln);
                char* eq = strchr(ln, '=');
                if (!eq) eq = strchr(ln, ':');
                if (eq) {
                    *eq = 0;
                    char* k = ln;
                    while (*k == '"' || *k == ' ') k++;
                    char* v = eq + 1;
                    while (*v == '"' || *v == ' ') v++;
                    v[strcspn(v, "\r\n\"")] = 0;
                    if (strlen(k) > 0 && strlen(v) > 0) addProperty(p, pc, k, v);
                }
            }
        }
        pclose(fp);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    auto* r = makeResult(ND_DIAG_WIFI, 0,
                         pc > 0 ? ND_STATUS_PASS : ND_STATUS_INFO,
                         pc > 0 ? "WiFi data via dumpsys" : "dumpsys wifi blocked",
                         "Android dumpsys wifi", d, p, pc, raw);
    if (pc == 0) markDegraded(r, "WiFi info requires ACCESS_WIFI_STATE permission");
    return r;
}

// ===========================================================================
// Cellular via dumpsys telephony.registry
// ===========================================================================
const NdPermissionInfo _androidCellPerms[] = {
    {"cellular.signal", ND_PERM_UNKNOWN, "Cellular Signal",
     "READ_PHONE_STATE or ACCESS_NETWORK_STATE"}
};
const NdPermissionInfo* NdCellularDiagnosticAndroid::requiredPermissions(int* c) const {
    *c = 1; return _androidCellPerms;
}

NdDiagnosticResult* NdCellularDiagnosticAndroid::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[2048] = {}; int rl = 0;

    FILE* fp = dumpsys("telephony.registry");
    if (fp) {
        char ln[256];
        while (fgets(ln, sizeof(ln), fp)) {
            if (strstr(ln, "mSignalStrength") || strstr(ln, "mNetworkType") ||
                strstr(ln, "mDataState")) {
                ln[strcspn(ln, "\n")] = 0;
                char* eq = strchr(ln, '=');
                if (eq) { *eq = 0; addProperty(p, pc, ln, eq + 1); }
                rl += snprintf(raw + rl, sizeof(raw) - rl, "%s\n", ln);
            }
        }
        pclose(fp);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_CELLULAR_INFO, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_INFO,
                      pc > 0 ? "Cellular info via dumpsys" : "No cellular data",
                      "Android dumpsys telephony.registry", d, p, pc, raw);
}

// ===========================================================================
// DHCP / IP Config / Connections — via /proc and getifaddrs
// ===========================================================================
NdDiagnosticResult* NdDhcpDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[2048] = {}; int rl = 0;

    // Check /proc/net/ for DHCP indicators, also getprop
    char dhcp1[PROP_VALUE_MAX] = {}, dhcp2[PROP_VALUE_MAX] = {};
    __system_property_get("dhcp.wlan0.ipaddress", dhcp1);
    __system_property_get("dhcp.wlan0.server", dhcp2);
    if (dhcp1[0]) {
        rl += snprintf(raw + rl, sizeof(raw) - rl, "DHCP IP: %s\n", dhcp1);
        addProperty(p, pc, "IP", dhcp1);
    }
    if (dhcp2[0]) {
        rl += snprintf(raw + rl, sizeof(raw) - rl, "DHCP Server: %s\n", dhcp2);
        addProperty(p, pc, "DHCP Server", dhcp2);
    }
    if (!dhcp1[0] && !dhcp2[0])
        rl = snprintf(raw, sizeof(raw), "DHCP info not available\n");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DHCP_STATUS, 0,
                      (dhcp1[0] || dhcp2[0]) ? ND_STATUS_PASS : ND_STATUS_INFO,
                      "DHCP status", "getprop (no CLI)", d, p, pc, raw);
}

NdDiagnosticResult* NdIpConfigDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0;

    struct ifaddrs *ifa, *ifa2;
    if (getifaddrs(&ifa2) == 0) {
        for (ifa = ifa2; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            char addr[INET6_ADDRSTRLEN] = {};
            int family = ifa->ifa_addr->sa_family;
            if (family == AF_INET)
                inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, addr, sizeof(addr));
            else if (family == AF_INET6)
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr, addr, sizeof(addr));
            else continue;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: %s\n", ifa->ifa_name, addr);
            addProperty(p, pc, ifa->ifa_name, addr);
        }
        freeifaddrs(ifa2);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_IP_CONFIG, 0, ND_STATUS_PASS, "IP config",
                      "getifaddrs() (no CLI)", d, p, pc, raw);
}

NdDiagnosticResult* NdConnectionsDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0;

    static const char* procFiles[] = {
        "/proc/net/tcp", "/proc/net/tcp6", "/proc/net/udp", "/proc/net/udp6", nullptr
    };
    for (int i = 0; procFiles[i]; i++) {
        char buf[4096] = {};
        int n = readProcFile(procFiles[i], buf, sizeof(buf));
        if (n > 0) rl += snprintf(raw + rl, sizeof(raw) - rl, "=== %s ===\n%s", procFiles[i], buf);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_ACTIVE_CONNECTIONS, 0, ND_STATUS_PASS, "Connections",
                      "/proc/net/* (no CLI)", d, p, pc, raw);
}

// ===========================================================================
// G2 — Routing / ARP via /proc + getprop
// ===========================================================================
NdDiagnosticResult* NdRoutingDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    rl = readProcFile("/proc/net/route", raw, sizeof(raw));
    if (rl == 0) rl = snprintf(raw, sizeof(raw), "No routing data\n");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_ROUTING_TABLE, 0,
                      rl > 0 ? ND_STATUS_PASS : ND_STATUS_INFO, "Routing table",
                      "/proc/net/route (no CLI)", d, p, pc, raw);
}

NdDiagnosticResult* NdArpDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    rl = readProcFile("/proc/net/arp", raw, sizeof(raw));
    if (rl == 0) rl = snprintf(raw, sizeof(raw), "No ARP data\n");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_ARP_TABLE, 0,
                      rl > 0 ? ND_STATUS_PASS : ND_STATUS_INFO, "ARP table",
                      "/proc/net/arp (no CLI)", d, p, pc, raw);
}

// ===========================================================================
// G3 — DNS / Internet / DNS Resolution
// ===========================================================================
NdDiagnosticResult* NdDnsServersDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    char dns1[PROP_VALUE_MAX] = {}, dns2[PROP_VALUE_MAX] = {};
    __system_property_get("net.dns1", dns1);
    __system_property_get("net.dns2", dns2);
    if (dns1[0]) { addProperty(p, pc, "DNS 1", dns1); rl += snprintf(raw + rl, sizeof(raw) - rl, "nameserver %s\n", dns1); }
    if (dns2[0]) { addProperty(p, pc, "DNS 2", dns2); rl += snprintf(raw + rl, sizeof(raw) - rl, "nameserver %s\n", dns2); }
    if (!dns1[0] && !dns2[0]) rl = snprintf(raw, sizeof(raw), "No DNS info\n");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_SERVERS, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, "DNS servers",
                      "getprop (no CLI)", d, p, pc, raw);
}

NdDiagnosticResult* NdInternetConnDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[512] = {};

    const char* target = input->target ? input->target : "223.5.5.5";
    bool ok = tcpConnect(target, 53, 5);

    snprintf(raw, sizeof(raw), "TCP %s:53 => %s\n", target, ok ? "OK" : "FAILED");
    addProperty(p, pc, ok ? "Status" : "Error", ok ? "Connected" : "Failed");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_INTERNET_CONNECTIVITY, 0,
                      ok ? ND_STATUS_PASS : ND_STATUS_FAIL,
                      ok ? "Internet OK" : "No internet", "TCP connect", d, p, pc, raw);
}

NdDiagnosticResult* NdDnsResolveDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    const char* hostname = "aliyun.com";
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int ret = getaddrinfo(hostname, nullptr, &hints, &res);

    if (ret == 0) {
        for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {
            char addr[INET6_ADDRSTRLEN] = {};
            if (ai->ai_family == AF_INET)
                inet_ntop(AF_INET, &((struct sockaddr_in*)ai->ai_addr)->sin_addr, addr, sizeof(addr));
            else continue;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s => %s\n", hostname, addr);
            addProperty(p, pc, hostname, addr);
        }
        freeaddrinfo(res);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_RESOLUTION, 0,
                      ret == 0 ? ND_STATUS_PASS : ND_STATUS_FAIL,
                      ret == 0 ? "DNS OK" : "DNS failed", "getaddrinfo()", d, p, pc, raw);
}

// ===========================================================================
// Skipped on Android (not available, or delegated to Qt C++ layer)
// ===========================================================================
#define ANDROID_SKIP(ID, DIAG, REASON) \
NdDiagnosticResult* DIAG::execute(const NdDiagnosticInput*) { \
    return makeResult(ID, ND_ERR_NOT_SUPPORTED, ND_STATUS_SKIPPED, \
                      REASON, "Android — not available", 0, nullptr, 0, nullptr); }

ANDROID_SKIP(ND_DIAG_NIC_ADVANCED, NdNicAdvancedDiagAndroid, "NIC advanced — requires root")
ANDROID_SKIP(ND_DIAG_WIRED, NdWiredDiagAndroid, "Wired diagnostics — not on Android")
ANDROID_SKIP(ND_DIAG_NETWORK_PROFILE, NdNetProfileDiagLinux, "Network profile — use system settings")
ANDROID_SKIP(ND_DIAG_TCP_SETTINGS, NdTcpDiagLinux, "TCP settings — requires root")
ANDROID_SKIP(ND_DIAG_DEFAULT_GATEWAY, NdGatewayDiagLinux, "Gateway — use system settings")
ANDROID_SKIP(ND_DIAG_PROXY_SETTINGS, NdProxyDiagLinux, "Proxy — use system settings")
ANDROID_SKIP(ND_DIAG_NETSKOPE_STATUS, NdNetskopeDiagLinux, "Netskope — not on Android")
ANDROID_SKIP(ND_DIAG_DNS_CACHE, NdDnsCacheDiagLinux, "DNS cache — not available")
ANDROID_SKIP(ND_DIAG_PING, NdPingDiagLinux, "Ping — raw socket blocked (use TCP connect)")
ANDROID_SKIP(ND_DIAG_TRACEROUTE, NdTracerouteDiagLinux, "Traceroute — blocked")
ANDROID_SKIP(ND_DIAG_MTU_DISCOVERY, NdMtuDiagLinux, "MTU — blocked")
ANDROID_SKIP(ND_DIAG_PORT_SCAN, NdPortScanDiagLinux, "Port scan — use Qt TCP connect")
