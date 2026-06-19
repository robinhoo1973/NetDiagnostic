// =============================================================================
// iOS diagnostics — sandbox-aware, pure native frameworks only
// Zero popen — iOS sandbox prevents process execution anyway.
// =============================================================================
#include "../linux/nd_network_adapters.h"
#include "../linux/nd_nic_advanced.h"
#include "../linux/nd_wifi.h"
#include "../linux/nd_wired.h"
#include "../linux/nd_g3_g4.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#import <SystemConfiguration/SystemConfiguration.h>
#import <Network/Network.h>
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <CoreTelephony/CTCarrier.h>

#define IOS_SKIP(ID, DIAG, REASON) \
NdDiagnosticResult* DIAG::execute(const NdDiagnosticInput*) { \
    return makeResult(ID, ND_ERR_NOT_SUPPORTED, ND_STATUS_SKIPPED, \
                      REASON, "iOS sandbox restriction", 0, nullptr, 0, nullptr); }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
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

// ===========================================================================
// G1 — Network Adapters (getifaddrs works on iOS)
// ===========================================================================
NdDiagnosticResult* NdNetworkAdapterDiagnosticIOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t c = 0;
    char raw[4096] = {}; int rl = 0, up = 0, total = 0;

    struct ifaddrs *ifa, *ifa2;
    if (getifaddrs(&ifa2) == 0) {
        for (ifa = ifa2; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK) continue;
            total++;
            struct ifreq ifr; memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            bool isUp = false;
            if (sock >= 0) { ioctl(sock, SIOCGIFFLAGS, &ifr); isUp = ifr.ifr_flags & IFF_UP; close(sock); }
            if (isUp) up++;
            auto* sa = (struct sockaddr_dl*)ifa->ifa_addr;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                     (unsigned char)sa->sdl_data[sa->sdl_nlen + 0],
                     (unsigned char)sa->sdl_data[sa->sdl_nlen + 1],
                     (unsigned char)sa->sdl_data[sa->sdl_nlen + 2],
                     (unsigned char)sa->sdl_data[sa->sdl_nlen + 3],
                     (unsigned char)sa->sdl_data[sa->sdl_nlen + 4],
                     (unsigned char)sa->sdl_data[sa->sdl_nlen + 5]);
            char line[256];
            snprintf(line, sizeof(line), "%-10s %-4s %-17s", ifa->ifa_name, isUp ? "UP" : "DOWN", mac);
            addProperty(p, c, ifa->ifa_name, line);
            bool isCell = strstr(ifa->ifa_name, "pdp_ip") || strstr(ifa->ifa_name, "ipsec");
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s %s%s\n", line,
                           (ifa->ifa_flags & IFF_LOOPBACK) ? "LOOPBACK" : "",
                           isCell ? "CELLULAR" : "");
        }
        freeifaddrs(ifa2);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d/%d adapter(s) up", up, total);
    return makeResult(ND_DIAG_NETWORK_ADAPTERS, 0,
                      up > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, sum,
                      "getifaddrs() [iOS]", d, p, c, raw);
}

// ===========================================================================
// G1.3 — WiFi (CNCopyCurrentNetworkInfo, requires entitlement)
// ===========================================================================
const NdPermissionInfo _iosWifiPerms[] = {
    {"wifi.ssid", ND_PERM_UNKNOWN, "WiFi SSID",
     "com.apple.developer.networking.wifi-info entitlement"}
};
const NdPermissionInfo* NdWifiDiagIOS::requiredPermissions(int* c) const {
    *c = 1; return _iosWifiPerms;
}

NdDiagnosticResult* NdWifiDiagIOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;

    @autoreleasepool {
        NSArray* ifaces = (__bridge_transfer NSArray*)CNCopySupportedInterfaces();
        if (ifaces && ifaces.count > 0) {
            NSDictionary* info = (__bridge_transfer NSDictionary*)
                CNCopyCurrentNetworkInfo((__bridge CFStringRef)ifaces[0]);
            if (info) {
                NSString* ssid = info[(NSString*)kCNNetworkInfoKeySSID];
                NSString* bssid = info[(NSString*)kCNNetworkInfoKeyBSSID];
                if (ssid) addProperty(p, pc, "SSID", [ssid UTF8String]);
                if (bssid) addProperty(p, pc, "BSSID", [bssid UTF8String]);
            }
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    bool has = pc > 0;
    auto* r = makeResult(ND_DIAG_WIFI, 0,
                         has ? ND_STATUS_PASS : ND_STATUS_WARNING,
                         has ? "WiFi info retrieved" : "SSID unavailable — check entitlement",
                         "CNCopyCurrentNetworkInfo", d, p, pc,
                         has ? "SSID retrieved" : nullptr);
    if (!has) markDegraded(r,
        "WiFi SSID requires com.apple.developer.networking.wifi-info entitlement");
    return r;
}

// ===========================================================================
// Cellular (CTTelephonyNetworkInfo)
// ===========================================================================
NdDiagnosticResult* NdCellularDiagnosticIOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;

    @autoreleasepool {
        CTTelephonyNetworkInfo* tel = [[CTTelephonyNetworkInfo alloc] init];
        if (tel.serviceSubscriberCellularProviders) {
            for (NSString* key in tel.serviceSubscriberCellularProviders) {
                CTCarrier* car = tel.serviceSubscriberCellularProviders[key];
                if (car.carrierName)
                    addProperty(p, pc, "Carrier", [car.carrierName UTF8String]);
                if (car.mobileCountryCode && car.mobileNetworkCode) {
                    char mccmnc[16];
                    snprintf(mccmnc, sizeof(mccmnc), "%s-%s",
                             [car.mobileCountryCode UTF8String],
                             [car.mobileNetworkCode UTF8String]);
                    addProperty(p, pc, "MCC-MNC", mccmnc);
                }
                if (car.isoCountryCode)
                    addProperty(p, pc, "Country", [car.isoCountryCode UTF8String]);
                if (car.allowsVOIP)
                    addProperty(p, pc, "VoIP", "yes");
            }
        }
        if (tel.currentRadioAccessTechnology) {
            for (NSString* key in tel.currentRadioAccessTechnology) {
                NSString* rat = tel.currentRadioAccessTechnology[key];
                NSString* shortRat = [rat stringByReplacingOccurrencesOfString:
                    @"CTRadioAccessTechnology" withString:@""];
                addProperty(p, pc, "Radio", [shortRat UTF8String]);
            }
        }
        [tel release];
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_CELLULAR_INFO, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_INFO,
                      pc > 0 ? "Cellular info retrieved" : "No cellular data",
                      "CTTelephonyNetworkInfo", d, p, pc, nullptr);
}

// ===========================================================================
// DNS Servers (SCDynamicStore — works on iOS)
// ===========================================================================
NdDiagnosticResult* NdDnsServersDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t c = 0;
    char raw[2048] = {}; int rl = 0;

    @autoreleasepool {
        SCDynamicStoreRef store = SCDynamicStoreCreate(nullptr, CFSTR("nd"), nullptr, nullptr);
        if (store) {
            CFPropertyListRef plist = SCDynamicStoreCopyValue(
                store, CFSTR("State:/Network/Global/DNS"));
            if (plist) {
                NSDictionary* dict = (__bridge_transfer NSDictionary*)plist;
                NSArray* servers = dict[@"ServerAddresses"];
                if (servers) {
                    for (NSString* s in servers) {
                        addProperty(p, c, "DNS", [s UTF8String]);
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "nameserver %s\n", [s UTF8String]);
                    }
                }
            }
            CFRelease(store);
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_SERVERS, 0,
                      c > 0 ? ND_STATUS_PASS : ND_STATUS_INFO,
                      c > 0 ? "DNS servers found" : "No DNS info",
                      "SCDynamicStore [iOS]", d, p, c, raw);
}

// ===========================================================================
// Internet Connectivity (TCP connect — no ping on iOS)
// ===========================================================================
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

// ===========================================================================
// DNS Resolution (getaddrinfo)
// ===========================================================================
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
// Skipped on iOS (iOS sandbox restrictions)
// ===========================================================================
IOS_SKIP(ND_DIAG_NIC_ADVANCED, NdNicAdvancedDiagIOS, "NIC advanced — not on iOS")
IOS_SKIP(ND_DIAG_DHCP_STATUS, NdDhcpDiagLinux, "DHCP — use system settings")
IOS_SKIP(ND_DIAG_IP_CONFIG, NdIpConfigDiagLinux, "IP config — use system settings")
IOS_SKIP(ND_DIAG_ACTIVE_CONNECTIONS, NdConnectionsDiagLinux, "Connections — not on iOS")
IOS_SKIP(ND_DIAG_WIRED, NdWiredDiagIOS, "No Ethernet on iOS")
IOS_SKIP(ND_DIAG_DNS_CACHE, NdDnsCacheDiagLinux, "DNS cache — not available")
IOS_SKIP(ND_DIAG_PING, NdPingDiagLinux, "Ping — raw socket blocked on iOS")
IOS_SKIP(ND_DIAG_TRACEROUTE, NdTracerouteDiagLinux, "Traceroute — blocked on iOS")
IOS_SKIP(ND_DIAG_MTU_DISCOVERY, NdMtuDiagLinux, "MTU — blocked on iOS")
IOS_SKIP(ND_DIAG_PORT_SCAN, NdPortScanDiagLinux, "Port scan — use Qt TCP connect")
IOS_SKIP(ND_DIAG_NETSKOPE_STATUS, NdNetskopeDiagLinux, "Netskope — not on iOS")
