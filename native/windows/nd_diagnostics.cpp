// =============================================================================
// Windows native diagnostics — pure Win32 API (no CLI / PowerShell / _popen)
// =============================================================================
#include "../linux/nd_network_adapters.h"
#include "../linux/nd_nic_advanced.h"
#include "../linux/nd_wifi.h"
#include "../linux/nd_wired.h"
#include "../linux/nd_g2.h"
#include "../linux/nd_g3_g4.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#include <winhttp.h>
#include <tlhelp32.h>
#include <winreg.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool initWinsock() {
    static int state = -1; // -1=untried, 1=ok, 0=failed
    if (state < 0) {
        WSADATA wsa;
        state = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) ? 1 : 0;
    }
    return state == 1;
}

// GetAdaptersAddresses with dynamic retry on buffer overflow.
// Returns heap-allocated buffer (caller must free) or nullptr.
static PIP_ADAPTER_ADDRESSES getAdaptersSafe(ULONG family, ULONG flags) {
    ULONG len = 0;
    GetAdaptersAddresses(family, flags, nullptr, nullptr, &len);
    if (len == 0) return nullptr;
    auto* buf = (PIP_ADAPTER_ADDRESSES)malloc(len);
    if (!buf) return nullptr;
    if (GetAdaptersAddresses(family, flags, nullptr, buf, &len) == NO_ERROR)
        return buf;
    free(buf);
    return nullptr;
}

// WideCharToMultiByte with error checking — returns empty string on failure.
static bool wideToUtf8(LPCWSTR wstr, char* out, int outLen) {
    if (!wstr) { out[0] = 0; return false; }
    int n = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out, outLen, nullptr, nullptr);
    if (n <= 0) { out[0] = 0; return false; }
    return true;
}

// Read a registry DWORD value, return defaultValue on failure.
static DWORD regDword(HKEY base, const char* subkey, const char* name, DWORD defVal) {
    HKEY h;
    if (RegOpenKeyExA(base, subkey, 0, KEY_READ, &h) != ERROR_SUCCESS) return defVal;
    DWORD val = defVal, sz = sizeof(val);
    RegQueryValueExA(h, name, nullptr, nullptr, (LPBYTE)&val, &sz);
    RegCloseKey(h);
    return val;
}

// ===========================================================================
// G1.1 — Network Adapters (already pure Win32, kept as-is)
// ===========================================================================
NdDiagnosticResult* NdNetworkAdapterDiagnosticWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0, up = 0, total = 0;
    ULONG bufLen = 15000; char buf[15000];
    PIP_ADAPTER_ADDRESSES aa = (PIP_ADAPTER_ADDRESSES)buf;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, aa, &bufLen) == NO_ERROR) {
        for (; aa; aa = aa->Next) {
            total++;
            bool isUp = aa->OperStatus == IfOperStatusUp;
            if (isUp) up++;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                     aa->PhysicalAddress[0], aa->PhysicalAddress[1],
                     aa->PhysicalAddress[2], aa->PhysicalAddress[3],
                     aa->PhysicalAddress[4], aa->PhysicalAddress[5]);
            char line[256];
            snprintf(line, sizeof(line), "%-30S %-4s %-17s Speed=%I64u Mbps",
                     aa->FriendlyName, isUp ? "UP" : "DOWN", mac,
                     aa->TransmitLinkSpeed / 1000000);
            addProperty(p, pc, aa->AdapterName, line);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s\n", line);
        }
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d/%d adapter(s) up", up, total);
    return makeResult(ND_DIAG_NETWORK_ADAPTERS, 0,
                      up > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      sum, "GetAdaptersAddresses()", d, p, pc, raw);
}

// ===========================================================================
// G1.2 — NIC Advanced (registry + GetAdaptersAddresses)
// ===========================================================================
NdDiagnosticResult* NdNicAdvancedDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0; int found = 0;

    ULONG bufLen = 15000; char buf[15000];
    PIP_ADAPTER_ADDRESSES aa = (PIP_ADAPTER_ADDRESSES)buf;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, aa, &bufLen) == NO_ERROR) {
        for (; aa; aa = aa->Next) {
            char val[256];
            snprintf(val, sizeof(val), "%-20S  MTU=%lu  Speed=%I64u Mbps  DHCP=%s",
                     aa->FriendlyName, aa->Mtu, aa->TransmitLinkSpeed / 1000000,
                     aa->Flags & IP_ADAPTER_DHCP_ENABLED ? "yes" : "no");
            addProperty(p, pc, aa->AdapterName, val);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s\n", val);
            found++;
        }
    }
    // Supplement: TCP global parameters from registry
    DWORD rss = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "EnableRSS", 0);
    DWORD timestamps = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "Tcp1323Opts", 0);
    DWORD autoTuning = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "TcpAutotuning", 1);

    char adv[256];
    snprintf(adv, sizeof(adv), "RSS=%lu Timestamps=%lu AutoTuning=%lu", rss, timestamps & 3, autoTuning);
    addProperty(p, pc, "TCP Global", adv);
    rl += snprintf(raw + rl, sizeof(raw) - rl, "TCP Global: %s\n", adv);

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d NIC(s) + TCP registry", found);
    return makeResult(ND_DIAG_NIC_ADVANCED, 0,
                      found > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      sum, "GetAdaptersAddresses() + registry", d, p, pc, raw);
}

// ===========================================================================
// G1.3 — WiFi (WlanQueryInterface)
// ===========================================================================
NdDiagnosticResult* NdWifiDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    bool connected = false;

    HANDLE hWlan = nullptr;
    DWORD ver = 0;
    if (WlanOpenHandle(2, nullptr, &ver, &hWlan) == ERROR_SUCCESS) {
        PWLAN_INTERFACE_INFO_LIST ifList = nullptr;
        if (WlanEnumInterfaces(hWlan, nullptr, &ifList) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < ifList->dwNumberOfItems; i++) {
                WLAN_INTERFACE_INFO& info = ifList->InterfaceInfo[i];
                bool isUp = info.isState == wlan_interface_state_connected;
                if (isUp) connected = true;

                rl += snprintf(raw + rl, sizeof(raw) - rl,
                               "Interface[%lu]: %S  state=%lu\n",
                               i, info.strInterfaceDescription, info.isState);

                // Get connection attributes for connected interfaces
                if (isUp) {
                    PWLAN_CONNECTION_ATTRIBUTES conn = nullptr;
                    DWORD sz = sizeof(WLAN_CONNECTION_ATTRIBUTES);
                    WLAN_OPCODE_VALUE_TYPE opType;
                    if (WlanQueryInterface(hWlan, &info.InterfaceGuid,
                            wlan_intf_opcode_current_connection,
                            nullptr, &sz, (PVOID*)&conn, &opType) == ERROR_SUCCESS && conn) {
                        char ssid[64] = {};
                        memcpy(ssid, conn->wlanAssociationAttributes.dot11Ssid.ucSSID,
                               std::min((DWORD)sizeof(ssid) - 1,
                                        conn->wlanAssociationAttributes.dot11Ssid.uSSIDLength));
                        addProperty(p, pc, "SSID", ssid);
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "  SSID: %s\n", ssid);

                        char sig[32];
                        // wlanSignalQuality is 0-100 percentage, not dBm
                        snprintf(sig, sizeof(sig), "%lu%%",
                                 conn->wlanAssociationAttributes.wlanSignalQuality);
                        addProperty(p, pc, "Signal", sig);
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "  Signal: %s\n", sig);

                        char rate[32];
                        snprintf(rate, sizeof(rate), "%lu Mbps",
                                 conn->wlanAssociationAttributes.ulTxRate / 1000);
                        addProperty(p, pc, "TX Rate", rate);
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "  TX Rate: %s\n", rate);

                        WlanFreeMemory(conn);
                    }
                }
            }
            WlanFreeMemory(ifList);
        }
        WlanCloseHandle(hWlan, nullptr);
    } else {
        rl = snprintf(raw, sizeof(raw), "WLAN API unavailable\n");
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_WIFI, 0,
                      connected ? ND_STATUS_PASS : ND_STATUS_INFO,
                      connected ? "WiFi connected" : "No active WiFi",
                      "WlanQueryInterface()", d, p, pc, raw);
}

// ===========================================================================
// G1.4 — Wired (GetIfTable2)
// ===========================================================================
NdDiagnosticResult* NdWiredDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; int found = 0;

    PMIB_IF_TABLE2 ifTable = nullptr;
    if (GetIfTable2(&ifTable) == NO_ERROR) {
        for (ULONG i = 0; i < ifTable->NumEntries; i++) {
            MIB_IF_ROW2& row = ifTable->Table[i];
            // Skip loopback, tunnel, WiFi (media types)
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (row.Type == IF_TYPE_TUNNEL) continue;
            // Ethernet-like interfaces
            if (row.PhysicalMediumType == NdisPhysicalMediumNative802_11 ||
                row.PhysicalMediumType == NdisPhysicalMediumWirelessLan) continue;

            const char* state = row.OperStatus == IfOperStatusUp ? "UP" : "DOWN";
            char val[256];
            snprintf(val, sizeof(val), "%s  MTU=%lu  Speed=%I64u Mbps",
                     state, row.Mtu, row.TransmitLinkSpeed / 1000000);
            char name[64];
            WideCharToMultiByte(CP_UTF8, 0, row.Alias, -1, name, sizeof(name), nullptr, nullptr);
            addProperty(p, pc, name, val);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: %s\n", name, val);
            found++;
        }
        FreeMibTable(ifTable);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), found ? "Wired adapter(s) detected" : "No wired adapter found");
    return makeResult(ND_DIAG_WIRED, 0,
                      found > 0 ? ND_STATUS_PASS : ND_STATUS_INFO,
                      sum, "GetIfTable2()", d, p, pc, raw);
}

// ===========================================================================
// G1 remaining — DHCP / IP Config / Active Connections
// ===========================================================================
NdDiagnosticResult* NdDhcpDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; bool anyDhcp = false;

    ULONG bufLen = 15000; char buf[15000];
    PIP_ADAPTER_ADDRESSES aa = (PIP_ADAPTER_ADDRESSES)buf;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, aa, &bufLen) == NO_ERROR) {
        for (; aa; aa = aa->Next) {
            bool dhcp = (aa->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;
            if (dhcp) anyDhcp = true;
            char val[64];
            snprintf(val, sizeof(val), "%s", dhcp ? "DHCP" : "Static");
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%-30S  %s\n", aa->FriendlyName, val);
            addProperty(p, pc, aa->AdapterName, val);
        }
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DHCP_STATUS, 0,
                      anyDhcp ? ND_STATUS_PASS : ND_STATUS_INFO,
                      anyDhcp ? "DHCP detected on some adapters" : "All static IPs",
                      "GetAdaptersAddresses()", d, p, pc, raw);
}

NdDiagnosticResult* NdIpConfigDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0;

    ULONG bufLen = 15000; char buf[15000];
    PIP_ADAPTER_ADDRESSES aa = (PIP_ADAPTER_ADDRESSES)buf;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, aa, &bufLen) == NO_ERROR) {
        for (; aa; aa = aa->Next) {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "=== %S ===\n", aa->FriendlyName);
            // IP addresses
            for (PIP_ADAPTER_UNICAST_ADDRESS ua = aa->FirstUnicastAddress; ua; ua = ua->Next) {
                char ip[INET6_ADDRSTRLEN] = {};
                if (ua->Address.lpSockaddr->sa_family == AF_INET)
                    inet_ntop(AF_INET, &((SOCKADDR_IN*)ua->Address.lpSockaddr)->sin_addr, ip, sizeof(ip));
                else if (ua->Address.lpSockaddr->sa_family == AF_INET6)
                    inet_ntop(AF_INET6, &((SOCKADDR_IN6*)ua->Address.lpSockaddr)->sin6_addr, ip, sizeof(ip));
                else continue;
                rl += snprintf(raw + rl, sizeof(raw) - rl, "  IP: %s\n", ip);
                addProperty(p, pc, aa->AdapterName, ip);
            }
            // DNS servers
            for (PIP_ADAPTER_DNS_SERVER_ADDRESS dns = aa->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[INET6_ADDRSTRLEN] = {};
                if (dns->Address.lpSockaddr->sa_family == AF_INET)
                    inet_ntop(AF_INET, &((SOCKADDR_IN*)dns->Address.lpSockaddr)->sin_addr, ip, sizeof(ip));
                else continue;
                rl += snprintf(raw + rl, sizeof(raw) - rl, "  DNS: %s\n", ip);
            }
            // Gateway
            for (PIP_ADAPTER_GATEWAY_ADDRESS_LH gw = aa->FirstGatewayAddress; gw; gw = gw->Next) {
                char ip[INET6_ADDRSTRLEN] = {};
                if (gw->Address.lpSockaddr->sa_family == AF_INET)
                    inet_ntop(AF_INET, &((SOCKADDR_IN*)gw->Address.lpSockaddr)->sin_addr, ip, sizeof(ip));
                else continue;
                rl += snprintf(raw + rl, sizeof(raw) - rl, "  Gateway: %s\n", ip);
            }
        }
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_IP_CONFIG, 0, ND_STATUS_PASS, "IP configuration",
                      "GetAdaptersAddresses()", d, p, pc, raw);
}

NdDiagnosticResult* NdConnectionsDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0;
    if (!initWinsock()) { auto d = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t0).count(); return makeResult(ND_DIAG_ACTIVE_CONNECTIONS, ND_ERR_INTERNAL, ND_STATUS_ERROR, "Winsock init failed", "", d, nullptr, 0, nullptr); }

    // Use GetExtendedTcpTable for active TCP connections
    ULONG bufLen = 0;
    GetExtendedTcpTable(nullptr, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (bufLen > 0 && bufLen < 65536) {
        PMIB_TCPTABLE_OWNER_PID tcp = (PMIB_TCPTABLE_OWNER_PID)malloc(bufLen);
        if (tcp && GetExtendedTcpTable(tcp, &bufLen, FALSE, AF_INET,
                                        TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            for (DWORD i = 0; i < tcp->dwNumEntries; i++) {
                MIB_TCPROW_OWNER_PID& row = tcp->table[i];
                struct in_addr la, ra;
                la.S_un.S_addr = row.dwLocalAddr;
                ra.S_un.S_addr = row.dwRemoteAddr;
                const char* state = "UNKNOWN";
                switch (row.dwState) {
                    case MIB_TCP_STATE_ESTAB: state = "ESTABLISHED"; break;
                    case MIB_TCP_STATE_LISTEN: state = "LISTEN"; break;
                    case MIB_TCP_STATE_SYN_SENT: state = "SYN_SENT"; break;
                    case MIB_TCP_STATE_SYN_RCVD: state = "SYN_RCVD"; break;
                    case MIB_TCP_STATE_CLOSE_WAIT: state = "CLOSE_WAIT"; break;
                    case MIB_TCP_STATE_TIME_WAIT: state = "TIME_WAIT"; break;
                    default: break;
                }
                // inet_ntoa returns static buffer — copy each result immediately
                char localIp[INET_ADDRSTRLEN], remoteIp[INET_ADDRSTRLEN];
                strncpy(localIp, inet_ntoa(la), sizeof(localIp)-1);
                strncpy(remoteIp, inet_ntoa(ra), sizeof(remoteIp)-1);
                rl += snprintf(raw + rl, sizeof(raw) - rl,
                               "TCP %s:%d -> %s:%d  %s\n",
                               localIp, ntohs((u_short)row.dwLocalPort),
                               remoteIp, ntohs((u_short)row.dwRemotePort),
                               state);
            }
        }
        if (tcp) free(tcp);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_ACTIVE_CONNECTIONS, 0, ND_STATUS_PASS,
                      "TCP connections", "GetExtendedTcpTable()", d, p, pc, raw);
}

// ===========================================================================
// G2 — Network Profile / TCP / Gateway / Routing / ARP / Proxy
// ===========================================================================
NdDiagnosticResult* NdNetProfileDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    // Use GetIfTable2 for interface profiles
    PMIB_IF_TABLE2 ifTable = nullptr;
    if (GetIfTable2(&ifTable) == NO_ERROR) {
        for (ULONG i = 0; i < ifTable->NumEntries; i++) {
            MIB_IF_ROW2& row = ifTable->Table[i];
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            char name[64];
            WideCharToMultiByte(CP_UTF8, 0, row.Alias, -1, name, sizeof(name), nullptr, nullptr);
            const char* state = row.OperStatus == IfOperStatusUp ? "UP" : "DOWN";
            char val[128];
            snprintf(val, sizeof(val), "%s  MTU=%lu  Speed=%I64u Mbps",
                     state, row.Mtu, row.TransmitLinkSpeed / 1000000);
            addProperty(p, pc, name, val);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: %s\n", name, val);
        }
        FreeMibTable(ifTable);
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_NETWORK_PROFILE, 0, ND_STATUS_PASS,
                      "Network interfaces", "GetIfTable2()", d, p, pc, raw);
}

NdDiagnosticResult* NdTcpDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    DWORD autoTuning = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "TcpAutotuning", 1);
    DWORD timestamps = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "Tcp1323Opts", 0);
    DWORD rss = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "EnableRSS", 0);
    DWORD sack = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "SackOpts", 1);
    DWORD ecn = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "EnableCongestionProvider", 0);
    DWORD mtuProbe = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", "EnablePMTUDiscovery", 1);

    static const char* labels[] = {"AutoTuning","Timestamps","RSS","SACK","ECN","MTUDiscovery"};
    DWORD vals[] = {autoTuning, timestamps&3, rss, sack, ecn, mtuProbe};
    for (int i = 0; i < 6; i++) {
        char v[32]; snprintf(v, sizeof(v), "%lu", vals[i]);
        addProperty(p, pc, labels[i], v);
        rl += snprintf(raw + rl, sizeof(raw) - rl, "%s = %lu\n", labels[i], vals[i]);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_TCP_SETTINGS, 0, ND_STATUS_PASS,
                      "TCP kernel parameters", "registry", d, p, pc, raw);
}

NdDiagnosticResult* NdGatewayDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    initWinsock();

    PMIB_IPFORWARD_TABLE2 routes = nullptr;
    if (GetIpForwardTable2(AF_INET, &routes) == NO_ERROR) {
        for (ULONG i = 0; i < routes->NumEntries; i++) {
            MIB_IPFORWARD_ROW2& r = routes->Table[i];
            if (r.DestinationPrefix.PrefixLength == 0) { // 0.0.0.0/0
                char gw[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &r.NextHop.Ipv4.sin_addr, gw, sizeof(gw));
                rl += snprintf(raw + rl, sizeof(raw) - rl, "default via %s (if=%lu metric=%lu)\n",
                               gw, r.InterfaceIndex, r.Metric);
                addProperty(p, pc, "Gateway", gw);
            }
        }
        FreeMibTable(routes);
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DEFAULT_GATEWAY, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      pc > 0 ? "Default gateway found" : "No default gateway",
                      "GetIpForwardTable2()", d, p, pc, raw);
}

NdDiagnosticResult* NdRoutingDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0; int count = 0;
    initWinsock();

    PMIB_IPFORWARD_TABLE2 routes = nullptr;
    if (GetIpForwardTable2(AF_INET, &routes) == NO_ERROR) {
        for (ULONG i = 0; i < routes->NumEntries && count < 50; i++) {
            MIB_IPFORWARD_ROW2& r = routes->Table[i];
            char dest[INET_ADDRSTRLEN], gw[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &r.DestinationPrefix.Prefix.Ipv4.sin_addr, dest, sizeof(dest));
            inet_ntop(AF_INET, &r.NextHop.Ipv4.sin_addr, gw, sizeof(gw));
            rl += snprintf(raw + rl, sizeof(raw) - rl,
                           "%s/%lu via %s metric=%lu\n",
                           dest, r.DestinationPrefix.PrefixLength, gw, r.Metric);
            count++;
        }
        FreeMibTable(routes);
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "Routing table: %d routes shown", count);
    return makeResult(ND_DIAG_ROUTING_TABLE, 0, ND_STATUS_PASS, sum,
                      "GetIpForwardTable2()", d, p, pc, raw);
}

NdDiagnosticResult* NdArpDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; int count = 0;

    PMIB_IPNET_TABLE2 arp = nullptr;
    if (GetIpNetTable2(AF_INET, &arp) == NO_ERROR) {
        for (ULONG i = 0; i < arp->NumEntries && count < 50; i++) {
            MIB_IPNET_ROW2& row = arp->Table[i];
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &row.Address.Ipv4.sin_addr, ip, sizeof(ip));
            char mac[32] = "incomplete";
            if (row.PhysicalAddressLength >= 6)
                snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                         row.PhysicalAddress[0], row.PhysicalAddress[1],
                         row.PhysicalAddress[2], row.PhysicalAddress[3],
                         row.PhysicalAddress[4], row.PhysicalAddress[5]);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s => %s\n", ip, mac);
            addProperty(p, pc, ip, mac);
            count++;
        }
        FreeMibTable(arp);
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "ARP table: %d entries", count);
    return makeResult(ND_DIAG_ARP_TABLE, 0, ND_STATUS_PASS, sum,
                      "GetIpNetTable2()", d, p, pc, raw);
}

NdDiagnosticResult* NdProxyDiagWindows::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG cfg = {};
    if (WinHttpGetIEProxyConfigForCurrentUser(&cfg)) {
        if (cfg.lpszProxy) {
            char proxy[512];
            if (wideToUtf8(cfg.lpszProxy, proxy, sizeof(proxy))) {
                addProperty(p, pc, "Proxy", proxy);
                rl += snprintf(raw, sizeof(raw), "Proxy: %s\n", proxy);
            }
            GlobalFree(cfg.lpszProxy);
        }
        if (cfg.lpszProxyBypass) {
            char bypass[512];
            if (wideToUtf8(cfg.lpszProxyBypass, bypass, sizeof(bypass))) {
                addProperty(p, pc, "Bypass", bypass);
                rl += snprintf(raw + rl, sizeof(raw) - rl, "Bypass: %s\n", bypass);
            }
            GlobalFree(cfg.lpszProxyBypass);
        }
        if (cfg.lpszAutoConfigUrl) {
            char autoUrl[512];
            if (wideToUtf8(cfg.lpszAutoConfigUrl, autoUrl, sizeof(autoUrl))) {
                addProperty(p, pc, "PAC", autoUrl);
                rl += snprintf(raw + rl, sizeof(raw) - rl, "PAC: %s\n", autoUrl);
            }
            GlobalFree(cfg.lpszAutoConfigUrl);
        }
        if (!cfg.lpszProxy && !cfg.lpszAutoConfigUrl) {
            rl = snprintf(raw, sizeof(raw), "No proxy configured\n");
            addProperty(p, pc, "Proxy", "none");
        }
    } else {
        rl = snprintf(raw, sizeof(raw), "Could not read proxy config\n");
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_PROXY_SETTINGS, 0, ND_STATUS_PASS,
                      "Proxy configuration", "WinHttpGetIEProxyConfigForCurrentUser()",
                      d, p, pc, raw);
}

// ===========================================================================
// G3 — Netskope / DNS / Internet / Speed
// ===========================================================================
NdDiagnosticResult* NdNetskopeDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[256] = {};

    bool found = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (wcsstr(pe.szExeFile, L"nsproxy")) {
                    found = true;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    snprintf(raw, sizeof(raw), "Netskope nsproxy: %s\n", found ? "detected" : "not detected");
    addProperty(p, pc, "Netskope", found ? "installed" : "not installed");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_NETSKOPE_STATUS, 0, ND_STATUS_PASS, "Netskope status",
                      "CreateToolhelp32Snapshot()", d, p, pc, raw);
}

NdDiagnosticResult* NdDnsServersDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; int nsCount = 0;

    ULONG bufLen = 15000; char buf[15000];
    PIP_ADAPTER_ADDRESSES aa = (PIP_ADAPTER_ADDRESSES)buf;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, aa, &bufLen) == NO_ERROR) {
        for (; aa; aa = aa->Next) {
            for (PIP_ADAPTER_DNS_SERVER_ADDRESS dns = aa->FirstDnsServerAddress; dns; dns = dns->Next) {
                if (dns->Address.lpSockaddr->sa_family == AF_INET) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &((SOCKADDR_IN*)dns->Address.lpSockaddr)->sin_addr, ip, sizeof(ip));
                    nsCount++;
                    char label[32];
                    snprintf(label, sizeof(label), "DNS %d", nsCount);
                    addProperty(p, pc, label, ip);
                    rl += snprintf(raw + rl, sizeof(raw) - rl, "nameserver %s\n", ip);
                }
            }
        }
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d DNS server(s)", nsCount);
    return makeResult(ND_DIAG_DNS_SERVERS, 0,
                      nsCount > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, sum,
                      "GetAdaptersAddresses() DNS", d, p, pc, raw);
}

NdDiagnosticResult* NdDnsCacheDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[512] = {};

    // Windows DNS cache service status via registry
    DWORD cachingEnabled = regDword(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters", "MaxCacheTtl", 0);
    snprintf(raw, sizeof(raw), "DNS cache: %s (MaxCacheTtl=%lu)\n",
             cachingEnabled > 0 ? "active" : "unknown", cachingEnabled);
    addProperty(p, pc, "Cache", cachingEnabled > 0 ? "active" : "unknown");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_CACHE, 0,
                      cachingEnabled > 0 ? ND_STATUS_PASS : ND_STATUS_INFO,
                      cachingEnabled > 0 ? "DNS cache active" : "DNS cache status unknown",
                      "registry", d, p, pc, raw);
}

NdDiagnosticResult* NdInternetConnDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[512] = {};
    initWinsock();

    const char* target = input->target ? input->target : "223.5.5.5";
    bool ok = false;

    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(target, "53", &hints, &res) == 0) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s != INVALID_SOCKET) {
            DWORD timeout = 5000;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
            ok = connect(s, res->ai_addr, (int)res->ai_addrlen) == 0;
            closesocket(s);
        }
        freeaddrinfo(res);
    }

    snprintf(raw, sizeof(raw), "TCP %s:53 => %s\n", target, ok ? "OK" : "FAILED");
    addProperty(p, pc, ok ? "Status" : "Error", ok ? "Connected" : "Failed");
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_INTERNET_CONNECTIVITY, 0,
                      ok ? ND_STATUS_PASS : ND_STATUS_FAIL,
                      ok ? "Internet connectivity confirmed" : "No internet connectivity",
                      "TCP connect", d, p, pc, raw);
}

// ===========================================================================
// G4 — DNS Resolution / Ping / Traceroute / MTU
// ===========================================================================
NdDiagnosticResult* NdDnsResolveDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    initWinsock();

    const char* hostname = "aliyun.com";
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int ret = getaddrinfo(hostname, nullptr, &hints, &res);

    if (ret == 0) {
        for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {
            char addr[INET6_ADDRSTRLEN] = {};
            if (ai->ai_family == AF_INET)
                inet_ntop(AF_INET, &((SOCKADDR_IN*)ai->ai_addr)->sin_addr, addr, sizeof(addr));
            else continue;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s => %s\n", hostname, addr);
            addProperty(p, pc, hostname, addr);
        }
        freeaddrinfo(res);
    } else {
        rl = snprintf(raw, sizeof(raw), "DNS resolution failed for %s\n", hostname);
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_RESOLUTION, 0,
                      ret == 0 ? ND_STATUS_PASS : ND_STATUS_FAIL,
                      ret == 0 ? "DNS resolution OK" : "DNS resolution failed",
                      "getaddrinfo()", d, p, pc, raw);
}

NdDiagnosticResult* NdPingDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    initWinsock();

    const char* targetStr = input->target ? input->target : "223.5.5.5";
    ULONG targetIp = inet_addr(targetStr);
    if (targetIp == INADDR_NONE) {
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET;
        if (getaddrinfo(targetStr, nullptr, &hints, &res) == 0) {
            targetIp = ((SOCKADDR_IN*)res->ai_addr)->sin_addr.S_un.S_addr;
            freeaddrinfo(res);
        }
    }

    if (targetIp == INADDR_NONE) {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        rl = snprintf(raw, sizeof(raw), "Could not resolve %s\n", targetStr);
        return makeResult(ND_DIAG_PING, ND_ERR_NETWORK, ND_STATUS_FAIL,
                          "DNS resolution failed", raw, d, nullptr, 0, raw);
    }

    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        rl = snprintf(raw, sizeof(raw), "IcmpCreateFile failed (need admin?)\n");
        return makeResult(ND_DIAG_PING, ND_ERR_PERMISSION, ND_STATUS_WARNING,
                          "ICMP unavailable", "IcmpCreateFile may require admin",
                          d, nullptr, 0, raw);
    }

    int pass = 0, fail = 0;
    char sendBuf[32] = "NetDiagnostic";
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + 32];

    for (int i = 0; i < 4; i++) {
        DWORD ret = IcmpSendEcho(hIcmp, targetIp, sendBuf, (WORD)strlen(sendBuf),
                                  nullptr, replyBuf, sizeof(replyBuf), 3000);
        PICMP_ECHO_REPLY reply = (PICMP_ECHO_REPLY)replyBuf;
        if (ret > 0 && reply->Status == IP_SUCCESS) {
            pass++;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "Reply from %s: time=%lums\n",
                           targetStr, reply->RoundTripTime);
        } else {
            fail++;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "Request timed out.\n");
        }
    }
    IcmpCloseHandle(hIcmp);

    int loss = fail * 100 / 4;
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[256];
    snprintf(sum, sizeof(sum), "%d/%d responded, %d%% loss", pass, 4, loss);
    return makeResult(ND_DIAG_PING, 0,
                      pass > 0 ? ND_STATUS_PASS : ND_STATUS_FAIL, sum,
                      "IcmpSendEcho()", d, p, pc, raw);
}

NdDiagnosticResult* NdTracerouteDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    initWinsock();

    const char* targetStr = input->target ? input->target : "223.5.5.5";
    ULONG targetIp = inet_addr(targetStr);
    if (targetIp == INADDR_NONE) {
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET;
        if (getaddrinfo(targetStr, nullptr, &hints, &res) == 0) {
            targetIp = ((SOCKADDR_IN*)res->ai_addr)->sin_addr.S_un.S_addr;
            freeaddrinfo(res);
        }
    }

    if (targetIp == INADDR_NONE) {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        rl = snprintf(raw, sizeof(raw), "Could not resolve %s\n", targetStr);
        return makeResult(ND_DIAG_TRACEROUTE, ND_ERR_NETWORK, ND_STATUS_FAIL,
                          "DNS resolution failed", raw, d, nullptr, 0, raw);
    }

    // Windows traceroute: send ICMP with incrementing TTL
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        return makeResult(ND_DIAG_TRACEROUTE, ND_ERR_PERMISSION, ND_STATUS_WARNING,
                          "ICMP unavailable", "IcmpCreateFile may require admin",
                          d, nullptr, 0, nullptr);
    }

    IP_OPTION_INFORMATION ipOpts = {};
    ipOpts.Ttl = 1;
    char sendBuf[32] = "NetDiagnostic";
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + 32];
    bool reached = false;

    for (int ttl = 1; ttl <= 15 && !reached; ttl++) {
        ipOpts.Ttl = (UCHAR)ttl;
        DWORD ret = IcmpSendEcho2(hIcmp, nullptr, nullptr, nullptr,
                                   targetIp, sendBuf, (WORD)strlen(sendBuf),
                                   &ipOpts, replyBuf, sizeof(replyBuf), 3000);
        PICMP_ECHO_REPLY reply = (PICMP_ECHO_REPLY)replyBuf;
        if (ret > 0) {
            struct in_addr addr;
            addr.S_un.S_addr = reply->Address;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%2d: %s (%lums)\n",
                           ttl, inet_ntoa(addr), reply->RoundTripTime);
            if (reply->Address == targetIp) reached = true;
        } else {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%2d: * * *\n", ttl);
        }
    }
    IcmpCloseHandle(hIcmp);

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_TRACEROUTE, 0,
                      reached ? ND_STATUS_PASS : ND_STATUS_INFO,
                      reached ? "Traceroute reached target" : "Traceroute may be incomplete",
                      "IcmpSendEcho2()", d, p, pc, raw);
}

NdDiagnosticResult* NdMtuDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    PMIB_IF_TABLE2 ifTable = nullptr;
    if (GetIfTable2(&ifTable) == NO_ERROR) {
        for (ULONG i = 0; i < ifTable->NumEntries; i++) {
            MIB_IF_ROW2& row = ifTable->Table[i];
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            char name[64];
            WideCharToMultiByte(CP_UTF8, 0, row.Alias, -1, name, sizeof(name), nullptr, nullptr);
            char mtu[32];
            snprintf(mtu, sizeof(mtu), "%lu", row.Mtu);
            addProperty(p, pc, name, mtu);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: MTU=%lu\n", name, row.Mtu);
        }
        FreeMibTable(ifTable);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_MTU_DISCOVERY, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      "MTU from GetIfTable2", "GetIfTable2()", d, p, pc, raw);
}

// ===========================================================================
// Stubs — handled in Qt C++ layer or not applicable on Windows
// ===========================================================================
NdDiagnosticResult* NdPortScanDiagLinux::execute(const NdDiagnosticInput*) {
    return makeResult(ND_DIAG_PORT_SCAN, 0, ND_STATUS_INFO,
                      "Port scan via Qt", "QTcpSocket::connect", 0, nullptr, 0, nullptr);
}
NdDiagnosticResult* NdCellularDiagLinux::execute(const NdDiagnosticInput*) {
    return makeResult(ND_DIAG_CELLULAR_INFO, ND_ERR_NOT_SUPPORTED, ND_STATUS_SKIPPED,
                      "Desktop — no cellular", "", 0, nullptr, 0, nullptr);
}
