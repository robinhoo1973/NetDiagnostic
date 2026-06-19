// =============================================================================
// macOS native diagnostics — CoreFoundation / SystemConfiguration / IOKit
// Zero popen — all pure native frameworks.
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
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <netinet/if_ether.h>
#include <unistd.h>
#include <dirent.h>

#import <SystemConfiguration/SystemConfiguration.h>
#import <CoreWLAN/CoreWLAN.h>
#import <IOKit/IOKitLib.h>
#import <Foundation/Foundation.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Copy a CFString value from an SCDynamicStore dict.
static NSString* cfString(CFDictionaryRef dict, CFStringRef key) {
    CFStringRef s = (CFStringRef)CFDictionaryGetValue(dict, key);
    return (__bridge NSString*)s;
}

/// Add a CFString key-value property, converting to C strings.
static void addCF(NdProperty*& p, int32_t& c, CFStringRef k, CFStringRef v) {
    NdDiagnosticBase::addProperty(p, c,
        [(__bridge NSString*)k UTF8String],
        [(__bridge NSString*)v UTF8String]);
}

/// Get DNS servers from SCDynamicStore.
static NSArray<NSString*>* getDNSServers() {
    SCDynamicStoreRef store = SCDynamicStoreCreate(nullptr, CFSTR("nd"), nullptr, nullptr);
    if (!store) return nil;
    CFPropertyListRef plist = SCDynamicStoreCopyValue(store, CFSTR("State:/Network/Global/DNS"));
    if (store) CFRelease(store);
    if (!plist) return nil;
    NSDictionary* dict = (__bridge_transfer NSDictionary*)plist;
    return dict[@"ServerAddresses"];
}

/// TCP connect helper (returns true on success).
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

/// Read a sysctl string value.
static bool sysctlString(const char* name, char* out, size_t outLen) {
    size_t len = outLen;
    return sysctlbyname(name, out, &len, nullptr, 0) == 0;
}

// ===========================================================================
// G1 — Network Adapters / NIC Advanced / Wired / DHCP / IP Config / Connections
// ===========================================================================

NdDiagnosticResult* NdNetworkAdapterDiagnosticMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t c = 0;
    char raw[8192] = {}; int rl = 0, up = 0, total = 0;
    struct ifaddrs *ifa, *ifa2;
    if (getifaddrs(&ifa2) == 0) {
        for (ifa = ifa2; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK) continue;
            total++;
            struct ifreq ifr; memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            bool isUp = false;
            int mtu = 0;
            if (sock >= 0) {
                ioctl(sock, SIOCGIFFLAGS, &ifr);
                isUp = (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
                struct ifreq mtuIfr;
                memset(&mtuIfr, 0, sizeof(mtuIfr));
                strncpy(mtuIfr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
                if (ioctl(sock, SIOCGIFMTU, &mtuIfr) == 0) mtu = mtuIfr.ifr_mtu;
                close(sock);
            }
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
            snprintf(line, sizeof(line), "%-10s %-4s %-17s MTU=%d",
                     ifa->ifa_name, isUp ? "UP" : "DOWN", mac, mtu);
            addProperty(p, c, ifa->ifa_name, line);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s\n", line);
        }
        freeifaddrs(ifa2);
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d/%d adapter(s) up", up, total);
    return makeResult(ND_DIAG_NETWORK_ADAPTERS, 0,
                      up > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      sum, "getifaddrs()+ioctl(SIOCGIFMTU)", d, p, c, raw);
}

// NIC Advanced — use IOKit for network device info
NdDiagnosticResult* NdNicAdvancedDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0; int found = 0;

    @autoreleasepool {
        CFMutableDictionaryRef matching = IOServiceMatching("IOEthernetInterface");
        if (matching) {
            io_iterator_t iter = 0;
            if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) == KERN_SUCCESS) {
                io_object_t obj;
                while ((obj = IOIteratorNext(iter))) {
                    CFMutableDictionaryRef props = nullptr;
                    if (IORegistryEntryCreateCFProperties(obj, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS) {
                        NSDictionary* dict = (__bridge_transfer NSDictionary*)props;
                        NSString* name = dict[@"IOName"] ?: @"unknown";
                        NSString* location = dict[@"IOLocation"] ?: @"";
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: loc=%s\n",
                                       [name UTF8String], [location UTF8String]);
                        addProperty(p, pc, [name UTF8String], [location UTF8String]);
                        found++;
                    }
                    IOObjectRelease(obj);
                }
                IOObjectRelease(iter);
            }
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d network interface(s) via IOKit", found);
    return makeResult(ND_DIAG_NIC_ADVANCED, 0,
                      found > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      sum, "IOKit IOServiceGetMatchingServices()", d, p, pc, raw);
}

// Wired — use getifaddrs, skip loopback and known WiFi names
NdDiagnosticResult* NdWiredDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; bool found = false;

    struct ifaddrs *ifa, *ifa2;
    if (getifaddrs(&ifa2) == 0) {
        for (ifa = ifa2; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK) continue;
            const char* name = ifa->ifa_name;
            if (strcmp(name, "lo0") == 0) continue;
            // Skip known WiFi (awdl/bridge) interfaces
            if (strncmp(name, "awdl", 4) == 0 || strncmp(name, "llw", 3) == 0) continue;
            if (strstr(name, "bridge") || strstr(name, "utun")) continue;

            found = true;
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            bool isUp = false;
            int mtu = 0;
            char media[64] = "unknown";
            if (sock >= 0) {
                ioctl(sock, SIOCGIFFLAGS, &ifr);
                isUp = (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
                struct ifreq mtuIfr;
                memset(&mtuIfr, 0, sizeof(mtuIfr));
                strncpy(mtuIfr.ifr_name, name, IFNAMSIZ - 1);
                if (ioctl(sock, SIOCGIFMTU, &mtuIfr) == 0) mtu = mtuIfr.ifr_mtu;
                struct ifmediareq mr;
                memset(&mr, 0, sizeof(mr));
                strncpy(mr.ifm_name, name, sizeof(mr.ifm_name));
                if (ioctl(sock, SIOCGIFMEDIA, &mr) == 0)
                    snprintf(media, sizeof(media), "%d Mbps", mr.ifm_current & IFM_ETHER ? 1000 : 100);
                close(sock);
            }
            char val[256];
            snprintf(val, sizeof(val), "%s  MTU=%d  %s", isUp ? "UP" : "DOWN", mtu, media);
            addProperty(p, pc, name, val);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: %s\n", name, val);
        }
        freeifaddrs(ifa2);
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), found ? "Ethernet adapter(s) found" : "No wired adapter");
    return makeResult(ND_DIAG_WIRED, 0,
                      found ? ND_STATUS_PASS : ND_STATUS_INFO,
                      sum, "getifaddrs()+ioctl(SIOCGIFMEDIA)", d, p, pc, raw);
}

// DHCP — use SCDynamicStore
NdDiagnosticResult* NdDhcpDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; bool found = false;

    @autoreleasepool {
        SCDynamicStoreRef store = SCDynamicStoreCreate(nullptr, CFSTR("nd"), nullptr, nullptr);
        if (store) {
            CFStringRef pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(
                nullptr, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetDHCP);
            CFArrayRef keys = SCDynamicStoreCopyKeyList(store, pattern);
            if (keys) {
                for (CFIndex i = 0; i < CFArrayGetCount(keys); i++) {
                    CFStringRef key = (CFStringRef)CFArrayGetValueAtIndex(keys, i);
                    CFDictionaryRef dict = (CFDictionaryRef)SCDynamicStoreCopyValue(store, key);
                    if (dict) {
                        found = true;
                        NSString* ip = cfString(dict, (__bridge CFStringRef)@"dhcp_server_identifier") ?: @"N/A";
                        NSString* iface = cfString(dict, (__bridge CFStringRef)@"InterfaceName") ?: @"unknown";
                        addProperty(p, pc, [iface UTF8String], [ip UTF8String]);
                        rl += snprintf(raw + rl, sizeof(raw) - rl,
                                       "%s: DHCP server=%s\n", [iface UTF8String], [ip UTF8String]);
                        CFRelease(dict);
                    }
                }
                CFRelease(keys);
            }
            if (pattern) CFRelease(pattern);
            CFRelease(store);
        }
    }
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DHCP_STATUS, 0,
                      found ? ND_STATUS_PASS : ND_STATUS_INFO,
                      found ? "DHCP detected" : "No DHCP info (may be static)",
                      "SCDynamicStoreCopyValue()", d, p, pc, raw);
}

// IP Config — getifaddrs() with all address families
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
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: %s (v%d)\n",
                           ifa->ifa_name, addr, family == AF_INET6 ? 6 : 4);
            char label[64];
            snprintf(label, sizeof(label), "%s (v%d)", ifa->ifa_name, family == AF_INET6 ? 6 : 4);
            addProperty(p, pc, label, addr);
        }
        freeifaddrs(ifa2);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_IP_CONFIG, 0, ND_STATUS_PASS, "IP configuration",
                      "getifaddrs()", d, p, pc, raw);
}

// Active Connections — getifaddrs for interfaces, simple TCP check
NdDiagnosticResult* NdConnectionsDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    // Enumerate active (UP+RUNNING) interfaces
    struct ifaddrs *ifa, *ifa2;
    if (getifaddrs(&ifa2) == 0) {
        for (ifa = ifa2; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK) continue;
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                ioctl(sock, SIOCGIFFLAGS, &ifr);
                bool active = (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
                close(sock);
                if (active) {
                    rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: ACTIVE\n", ifa->ifa_name);
                    addProperty(p, pc, ifa->ifa_name, "active");
                }
            }
        }
        freeifaddrs(ifa2);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_ACTIVE_CONNECTIONS, 0, ND_STATUS_PASS,
                      "Active connections", "getifaddrs()+ioctl(SIOCGIFFLAGS)", d, p, pc, raw);
}

// ===========================================================================
// G2 — Network Profile / TCP / Gateway / Routing / ARP / Proxy
// ===========================================================================

NdDiagnosticResult* NdNetProfileDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    @autoreleasepool {
        // DNS config via SCDynamicStore
        NSArray<NSString*>* dns = getDNSServers();
        if (dns) {
            for (NSString* s in dns) {
                rl += snprintf(raw + rl, sizeof(raw) - rl, "DNS: %s\n", [s UTF8String]);
            }
            addProperty(p, pc, "DNS Servers", [[dns componentsJoinedByString:@", "] UTF8String]);
        }

        // Network reachability
        SCNetworkReachabilityRef reach = SCNetworkReachabilityCreateWithName(
            nullptr, "www.apple.com");
        if (reach) {
            SCNetworkReachabilityFlags flags;
            if (SCNetworkReachabilityGetFlags(reach, &flags)) {
                bool reachable = (flags & kSCNetworkReachabilityFlagsReachable) != 0;
                addProperty(p, pc, "Internet", reachable ? "reachable" : "unreachable");
                rl += snprintf(raw + rl, sizeof(raw) - rl,
                               "Internet: %s\n", reachable ? "reachable" : "unreachable");
            }
            CFRelease(reach);
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_NETWORK_PROFILE, 0, ND_STATUS_PASS, "Network profile",
                      "SCDynamicStore+SCNetworkReachability", d, p, pc, raw);
}

NdDiagnosticResult* NdTcpDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    static const char* params[] = {
        "net.inet.tcp.slowstart_flightsize",
        "net.inet.tcp.recvspace", "net.inet.tcp.sendspace",
        "net.inet.tcp.sack", "net.inet.tcp.rfc1323",
        "net.inet.tcp.delayed_ack", "net.inet.tcp.mssdflt",
        nullptr
    };
    for (int i = 0; params[i]; i++) {
        int val = 0; size_t len = sizeof(val);
        if (sysctlbyname(params[i], &val, &len, nullptr, 0) == 0) {
            char v[32];
            snprintf(v, sizeof(v), "%d", val);
            addProperty(p, pc, params[i], v);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s = %d\n", params[i], val);
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_TCP_SETTINGS, 0, ND_STATUS_PASS, "TCP kernel parameters",
                      "sysctlbyname()", d, p, pc, raw);
}

NdDiagnosticResult* NdGatewayDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[2048] = {}; int rl = 0;

    // Read routing table via sysctl for default route
    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0};
    size_t len = 0;
    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) == 0 && len > 0) {
        char* buf = (char*)malloc(len);
        if (buf && sysctl(mib, 6, buf, &len, nullptr, 0) == 0) {
            struct rt_msghdr* rtm;
            for (size_t i = 0; i < len; i += rtm->rtm_msglen) {
                rtm = (struct rt_msghdr*)(buf + i);
                if (rtm->rtm_msglen < sizeof(struct rt_msghdr)) break; // corrupt
                // macOS: check if destination is 0.0.0.0 via rtm_addrs bitmask
                if (!(rtm->rtm_addrs & RTA_DST)) continue;
                struct sockaddr_in* dstCheck = (struct sockaddr_in*)(rtm + 1);
                if (dstCheck->sin_family != AF_INET || dstCheck->sin_addr.s_addr != INADDR_ANY) continue;
                // Walk sockaddr chain: DST, GATEWAY, NETMASK, ...
                struct sockaddr* sa = (struct sockaddr*)(rtm + 1);
                for (int j = 0; j < RTAX_MAX; j++) {
                    if (!(rtm->rtm_addrs & (1 << j))) continue;
                    if (j == RTAX_GATEWAY && sa->sa_family == AF_INET) {
                        struct sockaddr_in* sin = (struct sockaddr_in*)sa;
                        char gw[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &sin->sin_addr, gw, sizeof(gw));
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "default via %s\n", gw);
                        addProperty(p, pc, "Gateway", gw);
                        break;
                    }
                    if (sa->sa_len == 0) break;
                    sa = (struct sockaddr*)((char*)sa + sa->sa_len);
                }
            }
        }
        if (buf) free(buf);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DEFAULT_GATEWAY, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      pc > 0 ? "Default gateway found" : "No default gateway",
                      "sysctl NET_RT_DUMP", d, p, pc, raw);
}

NdDiagnosticResult* NdRoutingDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0; int count = 0;

    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0};
    size_t len = 0;
    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) == 0 && len > 0) {
        char* buf = (char*)malloc(len);
        if (buf && sysctl(mib, 6, buf, &len, nullptr, 0) == 0) {
            for (size_t i = 0; i < len && count < 50;) {
                struct rt_msghdr* rtm = (struct rt_msghdr*)(buf + i);
                if (rtm->rtm_msglen < sizeof(struct rt_msghdr)) break;
                struct sockaddr_in* dst = (struct sockaddr_in*)(rtm + 1);
                if (dst->sin_family == AF_INET) {
                    char dstIP[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &dst->sin_addr, dstIP, sizeof(dstIP));
                    rl += snprintf(raw + rl, sizeof(raw) - rl,
                                   "%s\n", dstIP);
                    count++;
                }
                i += rtm->rtm_msglen;
            }
        }
        if (buf) free(buf);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d route(s)", count);
    return makeResult(ND_DIAG_ROUTING_TABLE, 0, ND_STATUS_PASS, sum,
                      "sysctl NET_RT_DUMP", d, p, pc, raw);
}

NdDiagnosticResult* NdArpDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; int count = 0;

    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
    size_t len = 0;
    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) == 0 && len > 0) {
        char* buf = (char*)malloc(len);
        if (buf && sysctl(mib, 6, buf, &len, nullptr, 0) == 0) {
            for (size_t i = 0; i < len && count < 50;) {
                struct rt_msghdr* rtm = (struct rt_msghdr*)(buf + i);
                if (rtm->rtm_msglen < sizeof(struct rt_msghdr)) break;
                struct sockaddr_inarp* sin = (struct sockaddr_inarp*)(rtm + 1);
                struct sockaddr_dl* sdl = (struct sockaddr_dl*)((char*)sin + sizeof(*sin));
                if (sin->sin_family == AF_INET && sdl->sdl_alen > 0 && count < 50) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
                    char mac[32] = "incomplete";
                    if (sdl->sdl_alen >= 6)
                        snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                                 (unsigned char)sdl->sdl_data[sdl->sdl_nlen],
                                 (unsigned char)sdl->sdl_data[sdl->sdl_nlen + 1],
                                 (unsigned char)sdl->sdl_data[sdl->sdl_nlen + 2],
                                 (unsigned char)sdl->sdl_data[sdl->sdl_nlen + 3],
                                 (unsigned char)sdl->sdl_data[sdl->sdl_nlen + 4],
                                 (unsigned char)sdl->sdl_data[sdl->sdl_nlen + 5]);
                    rl += snprintf(raw + rl, sizeof(raw) - rl, "%s => %s\n", ip, mac);
                    addProperty(p, pc, ip, mac);
                    count++;
                }
                i += rtm->rtm_msglen;
            }
        }
        if (buf) free(buf);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d ARP entries", count);
    return makeResult(ND_DIAG_ARP_TABLE, 0, ND_STATUS_PASS, sum,
                      "sysctl NET_RT_FLAGS RTF_LLINFO", d, p, pc, raw);
}

NdDiagnosticResult* NdProxyDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[2048] = {}; int rl = 0;

    @autoreleasepool {
        CFDictionaryRef proxies = SCDynamicStoreCopyProxies(nullptr);
        if (proxies) {
            NSDictionary* dict = (__bridge NSDictionary*)proxies;
            // HTTP proxy
            if ([dict[@"HTTPEnable"] boolValue]) {
                NSString* host = dict[@"HTTPProxy"] ?: @"";
                NSNumber* port = dict[@"HTTPPort"];
                char v[256];
                snprintf(v, sizeof(v), "%s:%d", [host UTF8String], [port intValue]);
                addProperty(p, pc, "HTTP Proxy", v);
                rl += snprintf(raw + rl, sizeof(raw) - rl, "HTTP Proxy: %s\n", v);
            }
            // HTTPS proxy
            if ([dict[@"HTTPSEnable"] boolValue]) {
                NSString* host = dict[@"HTTPSProxy"] ?: @"";
                NSNumber* port = dict[@"HTTPSPort"];
                char v[256];
                snprintf(v, sizeof(v), "%s:%d", [host UTF8String], [port intValue]);
                addProperty(p, pc, "HTTPS Proxy", v);
                rl += snprintf(raw + rl, sizeof(raw) - rl, "HTTPS Proxy: %s\n", v);
            }
            // SOCKS proxy
            if ([dict[@"SOCKSEnable"] boolValue]) {
                NSString* host = dict[@"SOCKSProxy"] ?: @"";
                addProperty(p, pc, "SOCKS Proxy", [host UTF8String]);
                rl += snprintf(raw + rl, sizeof(raw) - rl, "SOCKS Proxy: %s\n", [host UTF8String]);
            }
            // Exceptions
            NSArray* bypass = dict[@"ExceptionsList"];
            if (bypass && bypass.count > 0) {
                NSString* s = [bypass componentsJoinedByString:@", "];
                addProperty(p, pc, "Bypass", [s UTF8String]);
                rl += snprintf(raw + rl, sizeof(raw) - rl, "Bypass: %s\n", [s UTF8String]);
            }
            if (pc == 0) {
                rl = snprintf(raw, sizeof(raw), "No proxy configured\n");
                addProperty(p, pc, "Proxy", "none");
            }
            CFRelease(proxies);
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_PROXY_SETTINGS, 0, ND_STATUS_PASS, "Proxy configuration",
                      "SCDynamicStoreCopyProxies()", d, p, pc, raw);
}

// ===========================================================================
// G1.3 — WiFi (CoreWLAN — already native)
// ===========================================================================
const NdPermissionInfo _macWifiPerms[] = {
    {"wifi.ssid", ND_PERM_UNKNOWN, "WiFi SSID (CoreWLAN)",
     "System Preferences > Privacy > WiFi"}
};
const NdPermissionInfo* NdWifiDiagMacOS::requiredPermissions(int* c) const {
    *c = 1; return _macWifiPerms;
}

NdDiagnosticResult* NdWifiDiagMacOS::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[2048] = {}; int rl = 0;

    @autoreleasepool {
        CWInterface* wifi = [[CWWiFiClient sharedWiFiClient] interface];
        if (wifi) {
            NSString* ssid = wifi.ssid;
            NSString* bssid = wifi.bssid;
            long rssi = wifi.rssiValue;
            long channel = wifi.wlanChannel.channelNumber;
            if (ssid) {
                rl += snprintf(raw + rl, sizeof(raw) - rl, "SSID: %s\n", [ssid UTF8String]);
                addProperty(p, pc, "SSID", [ssid UTF8String]);
            }
            if (bssid) {
                rl += snprintf(raw + rl, sizeof(raw) - rl, "BSSID: %s\n", [bssid UTF8String]);
                addProperty(p, pc, "BSSID", [bssid UTF8String]);
            }
            rl += snprintf(raw + rl, sizeof(raw) - rl, "RSSI: %ld dBm\nChannel: %ld\n", rssi, channel);
            char buf[32];
            snprintf(buf, sizeof(buf), "%ld dBm", rssi);
            addProperty(p, pc, "Signal", buf);
            snprintf(buf, sizeof(buf), "%ld", channel);
            addProperty(p, pc, "Channel", buf);
        } else {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "No active WiFi interface\n");
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    bool has = rl > 0 && !strstr(raw, "No active");
    auto* r = makeResult(ND_DIAG_WIFI, 0,
                         has ? ND_STATUS_PASS : ND_STATUS_INFO,
                         has ? "WiFi info retrieved" : "No WiFi",
                         has ? "CoreWLAN" : "", d, p, pc, raw);
    if (!has) markDegraded(r, "No WiFi interface or permission denied");
    return r;
}

// ===========================================================================
// G3 — Netskope / DNS / Internet / DNS Resolution
// ===========================================================================

NdDiagnosticResult* NdNetskopeDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[256] = {};

    // Enumerate processes via sysctl KERN_PROC
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t len = 0;
    bool found = false;
    if (sysctl(mib, 3, nullptr, &len, nullptr, 0) == 0 && len > 0) {
        struct kinfo_proc* procs = (struct kinfo_proc*)malloc(len);
        if (procs && sysctl(mib, 3, procs, &len, nullptr, 0) == 0) {
            int count = (int)(len / sizeof(struct kinfo_proc));
            for (int i = 0; i < count; i++) {
                if (strstr(procs[i].kp_proc.p_comm, "nsproxy")) {
                    found = true;
                    break;
                }
            }
        }
        if (procs) free(procs);
    }

    snprintf(raw, sizeof(raw), "Netskope nsproxy: %s\n", found ? "detected" : "not detected");
    addProperty(p, pc, "Netskope", found ? "installed" : "not installed");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_NETSKOPE_STATUS, 0, ND_STATUS_PASS, "Netskope status",
                      "sysctl KERN_PROC", d, p, pc, raw);
}

NdDiagnosticResult* NdDnsServersDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0; int nsCount = 0;

    @autoreleasepool {
        // Try SCDynamicStore first
        SCDynamicStoreRef store = SCDynamicStoreCreate(nullptr, CFSTR("nd"), nullptr, nullptr);
        if (store) {
            CFPropertyListRef plist = SCDynamicStoreCopyValue(store, CFSTR("State:/Network/Global/DNS"));
            if (plist) {
                NSDictionary* dict = (__bridge_transfer NSDictionary*)plist;
                NSArray* servers = dict[@"ServerAddresses"];
                if (servers) {
                    for (NSString* s in servers) {
                        nsCount++;
                        char label[32];
                        snprintf(label, sizeof(label), "DNS %d", nsCount);
                        addProperty(p, pc, label, [s UTF8String]);
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "nameserver %s\n", [s UTF8String]);
                    }
                }
            }
            CFRelease(store);
        }
        // Fallback: /etc/resolv.conf
        if (nsCount == 0) {
            FILE* f = fopen("/etc/resolv.conf", "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, "nameserver", 10) == 0) {
                        nsCount++;
                        char* ns = line + 10;
                        while (*ns == ' ' || *ns == '\t') ns++;
                        ns[strcspn(ns, "\n")] = 0;
                        char label[32];
                        snprintf(label, sizeof(label), "DNS %d", nsCount);
                        addProperty(p, pc, label, ns);
                        rl += snprintf(raw + rl, sizeof(raw) - rl, "nameserver %s\n", ns);
                    }
                }
                fclose(f);
            }
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d DNS server(s)", nsCount);
    return makeResult(ND_DIAG_DNS_SERVERS, 0,
                      nsCount > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, sum,
                      "SCDynamicStore+resolv.conf", d, p, pc, raw);
}

NdDiagnosticResult* NdDnsCacheDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[512] = {};

    // Check if mDNSResponder (DNS caching daemon) is running
    bool cacheActive = false;
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t len = 0;
    if (sysctl(mib, 3, nullptr, &len, nullptr, 0) == 0 && len > 0) {
        struct kinfo_proc* procs = (struct kinfo_proc*)malloc(len);
        if (procs && sysctl(mib, 3, procs, &len, nullptr, 0) == 0) {
            int count = (int)(len / sizeof(struct kinfo_proc));
            for (int i = 0; i < count; i++) {
                if (strstr(procs[i].kp_proc.p_comm, "mDNSResponder")) {
                    cacheActive = true;
                    break;
                }
            }
        }
        if (procs) free(procs);
    }

    addProperty(p, pc, "DNS Cache", cacheActive ? "active (mDNSResponder)" : "unknown");
    snprintf(raw, sizeof(raw), "DNS cache: %s\n",
             cacheActive ? "active (mDNSResponder)" : "status unknown");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_CACHE, 0,
                      cacheActive ? ND_STATUS_PASS : ND_STATUS_INFO,
                      cacheActive ? "DNS cache active" : "DNS cache status unknown",
                      "sysctl KERN_PROC (mDNSResponder)", d, p, pc, raw);
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
                      ok ? "Internet connectivity confirmed" : "No internet connectivity",
                      "TCP connect", d, p, pc, raw);
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
            else if (ai->ai_family == AF_INET6)
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)ai->ai_addr)->sin6_addr, addr, sizeof(addr));
            else continue;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s => %s\n", hostname, addr);
            addProperty(p, pc, hostname, addr);
        }
        freeaddrinfo(res);
    } else {
        rl = snprintf(raw, sizeof(raw), "DNS resolution failed for %s: %s\n",
                      hostname, gai_strerror(ret));
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_RESOLUTION, 0,
                      ret == 0 ? ND_STATUS_PASS : ND_STATUS_FAIL,
                      ret == 0 ? "DNS resolution OK" : "DNS resolution failed",
                      "getaddrinfo()", d, p, pc, raw);
}

// ===========================================================================
// G4 — Ping / Traceroute / MTU
// ===========================================================================

NdDiagnosticResult* NdPingDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    const char* target = input->target ? input->target : "223.5.5.5";
    int pass = 0, fail = 0;

    for (int i = 0; i < 4; i++) {
        auto probeStart = std::chrono::steady_clock::now();
        bool ok = tcpConnect(target, 53, 3);
        auto probeEnd = std::chrono::steady_clock::now();
        long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            probeEnd - probeStart).count();
        rl += snprintf(raw + rl, sizeof(raw) - rl, "Probe %d: %s (%ld ms)\n",
                       i + 1, ok ? "OK" : "FAIL", ms);
        if (ok) pass++; else fail++;
    }

    int loss = fail * 100 / 4;
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[256];
    snprintf(sum, sizeof(sum), "%d/%d responded, %d%% loss", pass, 4, loss);
    return makeResult(ND_DIAG_PING, 0,
                      pass > 0 ? ND_STATUS_PASS : ND_STATUS_FAIL, sum,
                      "TCP connect ping (no CLI, no root)", d, p, pc, raw);
}

NdDiagnosticResult* NdTracerouteDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    const char* target = input->target ? input->target : "223.5.5.5";
    int maxHops = 15;
    bool reached = false;

    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(target, "33434", &hints, &res) != 0) {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        return makeResult(ND_DIAG_TRACEROUTE, ND_ERR_NETWORK, ND_STATUS_FAIL,
                          "Could not resolve target", "", d, nullptr, 0, nullptr);
    }

    struct sockaddr_in targetAddr;
    memcpy(&targetAddr, res->ai_addr, sizeof(targetAddr));
    freeaddrinfo(res);

    for (int ttl = 1; ttl <= maxHops && !reached; ttl++) {
        int sendSock = socket(AF_INET, SOCK_DGRAM, 0);
        int recvSock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sendSock < 0 || recvSock < 0) {
            if (sendSock >= 0) close(sendSock);
            if (recvSock >= 0) close(recvSock);
            rl += snprintf(raw + rl, sizeof(raw) - rl,
                           "%2d: raw socket failed (need root)\n", ttl);
            break;
        }
        setsockopt(sendSock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
        struct sockaddr_in dest = {};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(33434 + ttl);
        dest.sin_addr = targetAddr.sin_addr;
        sendto(sendSock, "", 1, 0, (struct sockaddr*)&dest, sizeof(dest));

        struct timeval tv = {2, 0};
        setsockopt(recvSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        char buf[512];
        ssize_t n = recvfrom(recvSock, buf, sizeof(buf), 0,
                             (struct sockaddr*)&from, &fromLen);
        if (n > 0) {
            char fromIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, fromIP, sizeof(fromIP));
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%2d: %s\n", ttl, fromIP);
            if (from.sin_addr.s_addr == targetAddr.sin_addr.s_addr) reached = true;
        } else {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%2d: * * *\n", ttl);
        }
        close(sendSock);
        close(recvSock);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_TRACEROUTE, 0,
                      reached ? ND_STATUS_PASS : ND_STATUS_INFO,
                      reached ? "Traceroute reached target" : "Traceroute may be incomplete",
                      "UDP+ICMP raw socket (no CLI)", d, p, pc, raw);
}

NdDiagnosticResult* NdMtuDiagLinux::execute(const NdDiagnosticInput*) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* p = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    struct ifaddrs *ifa, *ifa2;
    if (getifaddrs(&ifa2) == 0) {
        for (ifa = ifa2; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK) continue;
            struct ifreq mtuIfr;
            memset(&mtuIfr, 0, sizeof(mtuIfr));
            strncpy(mtuIfr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                if (ioctl(sock, SIOCGIFMTU, &mtuIfr) == 0) {
                    char mtu[32];
                    snprintf(mtu, sizeof(mtu), "%d", mtuIfr.ifr_mtu);
                    addProperty(p, pc, ifa->ifa_name, mtu);
                    rl += snprintf(raw + rl, sizeof(raw) - rl,
                                   "%s: MTU=%d\n", ifa->ifa_name, mtuIfr.ifr_mtu);
                }
                close(sock);
            }
        }
        freeifaddrs(ifa2);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_MTU_DISCOVERY, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      "MTU from ioctl(SIOCGIFMTU)", "ioctl(SIOCGIFMTU)", d, p, pc, raw);
}

// ===========================================================================
// Stubs
// ===========================================================================
NdDiagnosticResult* NdPortScanDiagLinux::execute(const NdDiagnosticInput*) {
    return makeResult(ND_DIAG_PORT_SCAN, 0, ND_STATUS_INFO,
                      "Port scan via Qt", "QTcpSocket", 0, nullptr, 0, nullptr);
}
NdDiagnosticResult* NdCellularDiagLinux::execute(const NdDiagnosticInput*) {
    return makeResult(ND_DIAG_CELLULAR_INFO, ND_ERR_NOT_SUPPORTED, ND_STATUS_SKIPPED,
                      "Not available on macOS", "", 0, nullptr, 0, nullptr);
}
