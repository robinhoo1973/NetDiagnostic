#include "nd_g2.h"
#include "../common/nd_diagnostic_base.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <dirent.h>
#include <netinet/in.h>

// ---------------------------------------------------------------------------
// Helper: read a /proc or /sys file into a buffer
// ---------------------------------------------------------------------------
static int readFile(const char* path, char* buf, int maxLen) {
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

// ===========================================================================
// NdNetProfileDiagLinux — Network Profile via /sys/class/net
// ===========================================================================
NdDiagnosticResult* NdNetProfileDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    DIR* netDir = opendir("/sys/class/net");
    if (netDir) {
        struct dirent* entry;
        while ((entry = readdir(netDir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            char operstate[32] = "unknown", mtu[32] = "unknown", flags[32] = "unknown";
            char path[256];

            snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", entry->d_name);
            FILE* f = fopen(path, "r"); if (f) { if (fgets(operstate, sizeof(operstate), f)) operstate[strcspn(operstate,"\n")]=0; fclose(f); }
            snprintf(path, sizeof(path), "/sys/class/net/%s/mtu", entry->d_name);
            f = fopen(path, "r"); if (f) { if (fgets(mtu, sizeof(mtu), f)) mtu[strcspn(mtu,"\n")]=0; fclose(f); }
            snprintf(path, sizeof(path), "/sys/class/net/%s/flags", entry->d_name);
            f = fopen(path, "r"); if (f) { if (fgets(flags, sizeof(flags), f)) flags[strcspn(flags,"\n")]=0; fclose(f); }

            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: operstate=%s mtu=%s flags=%s\n",
                           entry->d_name, operstate, mtu, flags);
            char val[128];
            snprintf(val, sizeof(val), "%s | MTU=%s | flags=%s", operstate, mtu, flags);
            addProperty(props, pc, entry->d_name, val);
        }
        closedir(netDir);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_NETWORK_PROFILE, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      pc > 0 ? "Network interfaces enumerated" : "No interfaces found",
                      "/sys/class/net enumeration (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdTcpDiagLinux — TCP settings via /proc/sys/net/ipv4 and /proc/sys/net/core
// ===========================================================================
NdDiagnosticResult* NdTcpDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    static const char* tcpParams[][2] = {
        {"tcp_congestion_control", "/proc/sys/net/ipv4/tcp_congestion_control"},
        {"tcp_fastopen",           "/proc/sys/net/ipv4/tcp_fastopen"},
        {"tcp_syncookies",         "/proc/sys/net/ipv4/tcp_syncookies"},
        {"tcp_window_scaling",     "/proc/sys/net/ipv4/tcp_window_scaling"},
        {"tcp_timestamps",         "/proc/sys/net/ipv4/tcp_timestamps"},
        {"tcp_sack",               "/proc/sys/net/ipv4/tcp_sack"},
        {"tcp_dsack",              "/proc/sys/net/ipv4/tcp_dsack"},
        {"tcp_fack",               "/proc/sys/net/ipv4/tcp_fack"},
        {"tcp_ecn",                "/proc/sys/net/ipv4/tcp_ecn"},
        {"tcp_mtu_probing",        "/proc/sys/net/ipv4/tcp_mtu_probing"},
        {"tcp_rmem",               "/proc/sys/net/ipv4/tcp_rmem"},
        {"tcp_wmem",               "/proc/sys/net/ipv4/tcp_wmem"},
        {"somaxconn",              "/proc/sys/net/core/somaxconn"},
        {"netdev_max_backlog",     "/proc/sys/net/core/netdev_max_backlog"},
        {nullptr, nullptr}
    };

    for (int i = 0; tcpParams[i][0]; i++) {
        char val[128] = "N/A";
        FILE* f = fopen(tcpParams[i][1], "r");
        if (f) {
            if (fgets(val, sizeof(val), f)) val[strcspn(val, "\n")] = 0;
            fclose(f);
        }
        rl += snprintf(raw + rl, sizeof(raw) - rl, "%s = %s\n", tcpParams[i][0], val);
        addProperty(props, pc, tcpParams[i][0], val);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_TCP_SETTINGS, 0, ND_STATUS_PASS,
                      "TCP kernel parameters retrieved",
                      "/proc/sys/net/* (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdGatewayDiagLinux — Default gateway via /proc/net/route
// ===========================================================================
NdDiagnosticResult* NdGatewayDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    // /proc/net/route format: Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\tMTU\tWindow\tIRTT
    FILE* f = fopen("/proc/net/route", "r");
    if (f) {
        char line[512];
        bool header = true;
        while (fgets(line, sizeof(line), f)) {
            if (header) { header = false; continue; } // skip header
            char iface[32]; unsigned int dest, gateway, flags;
            if (sscanf(line, "%31s %x %x %x", iface, &dest, &gateway, &flags) >= 4) {
                // Destination 0.0.0.0 = default route; flags & 0x0002 = RTF_UP
                if (dest == 0 && gateway != 0 && (flags & 0x0002)) {
                    struct in_addr gw;
                    gw.s_addr = gateway;
                    char gwStr[32];
                    snprintf(gwStr, sizeof(gwStr), "%d.%d.%d.%d",
                             gw.s_addr & 0xff, (gw.s_addr >> 8) & 0xff,
                             (gw.s_addr >> 16) & 0xff, (gw.s_addr >> 24) & 0xff);
                    rl += snprintf(raw + rl, sizeof(raw) - rl,
                                   "default via %s dev %s\n", gwStr, iface);
                    addProperty(props, pc, iface, gwStr);
                }
            }
        }
        fclose(f);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DEFAULT_GATEWAY, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      pc > 0 ? "Default gateway found" : "No default gateway found",
                      "/proc/net/route (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdRoutingDiagLinux — Routing table via /proc/net/route (IPv4) + /proc/net/ipv6_route
// ===========================================================================
NdDiagnosticResult* NdRoutingDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0;
    int count = 0;

    // Read IPv4 routes from /proc/net/route
    FILE* f = fopen("/proc/net/route", "r");
    if (f) {
        char line[512];
        bool header = true;
        while (fgets(line, sizeof(line), f)) {
            if (header) { header = false; continue; }
            char iface[32]; unsigned int dest, gateway, flags, metric;
            if (sscanf(line, "%31s %x %x %x %*d %*d %d", iface, &dest, &gateway, &flags, &metric) >= 4) {
                rl += snprintf(raw + rl, sizeof(raw) - rl, "%s", line);
                count++;
            }
        }
        fclose(f);
    }

    // Also attempt to read IPv6 routes
    f = fopen("/proc/net/ipv6_route", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s", line);
            count++;
        }
        fclose(f);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "Routing table contains %d routes", count);
    return makeResult(ND_DIAG_ROUTING_TABLE, 0,
                      count > 0 ? ND_STATUS_PASS : ND_STATUS_INFO, sum,
                      "/proc/net/route + /proc/net/ipv6_route (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdArpDiagLinux — ARP table via /proc/net/arp
// ===========================================================================
NdDiagnosticResult* NdArpDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    int count = 0;

    FILE* f = fopen("/proc/net/arp", "r");
    if (f) {
        char line[256];
        bool header = true;
        while (fgets(line, sizeof(line), f)) {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s", line);
            if (header) { header = false; continue; }
            // Format: IP address   HW type   Flags   HW address   Mask   Device
            char ip[64], hwType[16], flags[16], hwAddr[32], mask[16], dev[32];
            if (sscanf(line, "%63s %15s %15s %31s %15s %31s",
                       ip, hwType, flags, hwAddr, mask, dev) >= 4) {
                addProperty(props, pc, ip, hwAddr);
                count++;
            }
        }
        fclose(f);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "ARP table contains %d entries", count);
    return makeResult(ND_DIAG_ARP_TABLE, 0,
                      ND_STATUS_PASS, sum,
                      "/proc/net/arp (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdProxyDiagLinux — Proxy settings via environment variables
// ===========================================================================
NdDiagnosticResult* NdProxyDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    int found = 0;

    static const char* proxyVars[] = {
        "HTTP_PROXY", "HTTPS_PROXY", "FTP_PROXY", "ALL_PROXY",
        "NO_PROXY", "http_proxy", "https_proxy", "ftp_proxy",
        "all_proxy", "no_proxy", nullptr
    };

    for (int i = 0; proxyVars[i]; i++) {
        const char* val = getenv(proxyVars[i]);
        if (val && val[0]) {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s=%s\n", proxyVars[i], val);
            addProperty(props, pc, proxyVars[i], val);
            found++;
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), found > 0 ? "%d proxy variable(s) set" : "No proxy configured",
             found);
    return makeResult(ND_DIAG_PROXY_SETTINGS, 0,
                      found > 0 ? ND_STATUS_PASS : ND_STATUS_INFO, sum,
                      "environment variables (no CLI)", d, props, pc, raw);
}
