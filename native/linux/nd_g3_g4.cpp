#include "nd_g3_g4.h"
#include "../common/nd_diagnostic_base.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Helpers
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

// Check if a process with given name is running (scan /proc)
static bool processRunning(const char* name) {
    DIR* proc = opendir("/proc");
    if (!proc) return false;
    struct dirent* de;
    while ((de = readdir(proc)) != nullptr) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        char path[256], comm[128] = {};
        snprintf(path, sizeof(path), "/proc/%s/comm", de->d_name);
        FILE* f = fopen(path, "r");
        if (f) {
            if (fgets(comm, sizeof(comm), f)) { comm[strcspn(comm, "\n")] = 0; fclose(f); }
            else { fclose(f); continue; }
        } else continue;
        if (strstr(comm, name)) { closedir(proc); return true; }
    }
    closedir(proc);
    return false;
}

// TCP connect to host:port, return true on success
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
// NdDhcpDiagLinux — DHCP by scanning /var/lib/dhcp/ and NetworkManager leases
// ===========================================================================
NdDiagnosticResult* NdDhcpDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    // Check common DHCP lease locations
    static const char* leaseDirs[] = {
        "/var/lib/dhcp/", "/var/lib/dhclient/", "/var/lib/NetworkManager/",
        "/run/NetworkManager/devices/", nullptr
    };
    for (int i = 0; leaseDirs[i]; i++) {
        DIR* d = opendir(leaseDirs[i]);
        if (d) {
            struct dirent* de;
            while ((de = readdir(d)) != nullptr) {
                if (de->d_name[0] == '.') continue;
                char full[512];
                snprintf(full, sizeof(full), "%s%s", leaseDirs[i], de->d_name);
                struct stat st;
                if (stat(full, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
                    rl += snprintf(raw + rl, sizeof(raw) - rl, "Lease: %s (%ld bytes)\n",
                                   full, (long)st.st_size);
                    addProperty(props, pc, "lease", full);
                }
            }
            closedir(d);
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DHCP_STATUS, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_INFO,
                      pc > 0 ? "DHCP lease file(s) found" : "No DHCP lease files found (may be static IP)",
                      "DHCP lease directory scan (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdIpConfigDiagLinux — IP config via getifaddrs() + /proc/net/route
// ===========================================================================
#include <ifaddrs.h>
#include <net/if.h>

NdDiagnosticResult* NdIpConfigDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0;

    struct ifaddrs* ifa;
    if (getifaddrs(&ifa) == 0) {
        for (struct ifaddrs* p = ifa; p; p = p->ifa_next) {
            if (!p->ifa_addr) continue;
            char addr[INET6_ADDRSTRLEN] = {};
            int family = p->ifa_addr->sa_family;
            if (family == AF_INET) {
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ifa_addr)->sin_addr, addr, sizeof(addr));
            } else if (family == AF_INET6) {
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)p->ifa_addr)->sin6_addr, addr, sizeof(addr));
            } else continue;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: %s family=%d\n",
                           p->ifa_name, addr, family);
            char key[64];
            snprintf(key, sizeof(key), "%s (v%d)", p->ifa_name, family == AF_INET ? 4 : 6);
            addProperty(props, pc, key, addr);
        }
        freeifaddrs(ifa);
    }

    // Also read /proc/net/route for routing context
    char routeBuf[4096] = {};
    if (readFile("/proc/net/route", routeBuf, sizeof(routeBuf)) > 0) {
        rl += snprintf(raw + rl, sizeof(raw) - rl, "\n--- Routing ---\n%s", routeBuf);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_IP_CONFIG, 0, ND_STATUS_PASS,
                      "IP configuration retrieved",
                      "getifaddrs() + /proc/net/route (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdConnectionsDiagLinux — Active connections via /proc/net/tcp + /proc/net/udp
// ===========================================================================
NdDiagnosticResult* NdConnectionsDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[8192] = {}; int rl = 0;

    static const char* connFiles[] = {
        "/proc/net/tcp", "/proc/net/tcp6", "/proc/net/udp", "/proc/net/udp6", nullptr
    };
    for (int i = 0; connFiles[i]; i++) {
        char buf[4096] = {};
        int n = readFile(connFiles[i], buf, sizeof(buf));
        if (n > 0) {
            rl += snprintf(raw + rl, sizeof(raw) - rl, "=== %s ===\n%s", connFiles[i], buf);
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_ACTIVE_CONNECTIONS, 0, ND_STATUS_PASS,
                      "Active connections retrieved",
                      "/proc/net/tcp + udp (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdNetskopeDiagLinux — Netskope detection via /proc scan
// ===========================================================================
NdDiagnosticResult* NdNetskopeDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[512] = {}; int rl = 0;

    bool found = processRunning("nsproxy");
    if (found) {
        rl = snprintf(raw, sizeof(raw), "Netskope nsproxy process detected\n");
        addProperty(props, pc, "Netskope", "installed");
    } else {
        rl = snprintf(raw, sizeof(raw), "Netskope nsproxy not detected\n");
        addProperty(props, pc, "Netskope", "not installed");
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_NETSKOPE_STATUS, 0, ND_STATUS_PASS, "Netskope status",
                      "/proc scan (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdDnsServersDiagLinux — DNS servers via /etc/resolv.conf
// ===========================================================================
NdDiagnosticResult* NdDnsServersDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    // Primary: /etc/resolv.conf
    rl = readFile("/etc/resolv.conf", raw, sizeof(raw));

    // Parse nameserver lines
    char* copy = strdup(raw);
    char* save = nullptr;
    int nsCount = 0;
    for (char* line = strtok_r(copy, "\n", &save); line; line = strtok_r(nullptr, "\n", &save)) {
        if (strncmp(line, "nameserver", 10) == 0) {
            char* ns = line + 10;
            while (*ns == ' ' || *ns == '\t') ns++;
            nsCount++;
            char label[32];
            snprintf(label, sizeof(label), "DNS %d", nsCount);
            addProperty(props, pc, label, ns);
        }
    }
    free(copy);

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d DNS server(s) found in resolv.conf", nsCount);
    return makeResult(ND_DIAG_DNS_SERVERS, 0,
                      nsCount > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, sum,
                      "/etc/resolv.conf (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdDnsCacheDiagLinux — DNS cache info via systemd-resolved
// ===========================================================================
NdDiagnosticResult* NdDnsCacheDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    // Try systemd-resolved cache statistics via /run/systemd/resolve/
    // The D-Bus API is preferred but complex; read available stat files
    static const char* resolvedPaths[] = {
        "/run/systemd/resolve/resolv.conf",
        "/run/systemd/resolve/stub-resolv.conf",
        nullptr
    };
    bool hasResolved = false;
    for (int i = 0; resolvedPaths[i]; i++) {
        FILE* f = fopen(resolvedPaths[i], "r");
        if (f) {
            hasResolved = true;
            char line[256];
            while (fgets(line, sizeof(line), f))
                rl += snprintf(raw + rl, sizeof(raw) - rl, "%s", line);
            fclose(f);
        }
    }

    addProperty(props, pc, "Cache backend", hasResolved ? "systemd-resolved" : "unknown");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_DNS_CACHE, 0,
                      hasResolved ? ND_STATUS_PASS : ND_STATUS_INFO,
                      hasResolved ? "systemd-resolved active" : "DNS cache status unknown",
                      "systemd-resolved files (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdInternetConnDiagLinux — Internet connectivity via TCP connect
// ===========================================================================
NdDiagnosticResult* NdInternetConnDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[512] = {}; int rl = 0;

    // Use TCP port 53 (DNS) as connectivity probe — works through most firewalls
    const char* target = input->target ? input->target : "223.5.5.5";
    int port = 53;
    bool ok = tcpConnect(target, port, 5);

    rl = snprintf(raw, sizeof(raw), "TCP connect to %s:%d => %s\n",
                  target, port, ok ? "SUCCESS" : "FAILED");

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    addProperty(props, pc, ok ? "Status" : "Error", ok ? "Connected" : "Connection failed");
    return makeResult(ND_DIAG_INTERNET_CONNECTIVITY, 0,
                      ok ? ND_STATUS_PASS : ND_STATUS_FAIL,
                      ok ? "Internet connectivity confirmed" : "No internet connectivity",
                      "TCP connect (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdDnsResolveDiagLinux — DNS resolution via getaddrinfo()
// ===========================================================================
NdDiagnosticResult* NdDnsResolveDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    const char* hostname = "aliyun.com";
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_UNSPEC; // both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;
    int ret = getaddrinfo(hostname, nullptr, &hints, &res);

    if (ret == 0) {
        for (struct addrinfo* p = res; p; p = p->ai_next) {
            char addr[INET6_ADDRSTRLEN] = {};
            if (p->ai_family == AF_INET) {
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ai_addr)->sin_addr, addr, sizeof(addr));
            } else if (p->ai_family == AF_INET6) {
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, addr, sizeof(addr));
            } else continue;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s => %s (v%d)\n",
                           hostname, addr, p->ai_family == AF_INET6 ? 6 : 4);
            char label[32];
            snprintf(label, sizeof(label), "%s (v%d)", hostname, p->ai_family == AF_INET6 ? 6 : 4);
            addProperty(props, pc, label, addr);
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
                      ret == 0 ? "DNS resolution succeeded" : "DNS resolution failed",
                      "getaddrinfo() (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdPingDiagLinux — Ping via TCP connect (ICMP-free, works without root)
// ===========================================================================
NdDiagnosticResult* NdPingDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    const char* target = input->target ? input->target : "223.5.5.5";
    int passCount = 0, failCount = 0;
    long totalMs = 0, minMs = 999999, maxMs = 0;

    // TCP-connect ping: 4 probes to port 53
    for (int i = 0; i < 4; i++) {
        auto probeStart = std::chrono::steady_clock::now();
        bool ok = tcpConnect(target, 53, 3);
        auto probeEnd = std::chrono::steady_clock::now();
        long ms = std::chrono::duration_cast<std::chrono::milliseconds>(probeEnd - probeStart).count();

        rl += snprintf(raw + rl, sizeof(raw) - rl,
                       "Probe %d: %s (%ld ms)\n", i + 1, ok ? "OK" : "FAIL", ms);
        if (ok) {
            passCount++;
            totalMs += ms;
            if (ms < minMs) minMs = ms;
            if (ms > maxMs) maxMs = ms;
        } else {
            failCount++;
        }
    }

    int loss = failCount * 100 / 4;
    char avgStr[32] = "N/A";
    if (passCount > 0) {
        snprintf(avgStr, sizeof(avgStr), "%ld ms", totalMs / passCount);
    }

    char sum[256];
    snprintf(sum, sizeof(sum), "%d/%d responded, %d%% loss, avg %s",
             passCount, 4, loss, avgStr);

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_PING, 0,
                      passCount > 0 ? ND_STATUS_PASS : ND_STATUS_FAIL, sum,
                      "TCP connect ping (no CLI, no root)", d, props, pc, raw);
}

// ===========================================================================
// NdTracerouteDiagLinux — Basic UDP traceroute
// ===========================================================================
NdDiagnosticResult* NdTracerouteDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    const char* target = input->target ? input->target : "223.5.5.5";
    int maxHops = 15; // reduced for speed
    bool reached = false;

    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(target, "33434", &hints, &res) != 0) {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        rl = snprintf(raw, sizeof(raw), "DNS resolution failed for %s\n", target);
        return makeResult(ND_DIAG_TRACEROUTE, ND_ERR_NETWORK, ND_STATUS_FAIL,
                          "Could not resolve target", "getaddrinfo failed", d, nullptr, 0, raw);
    }

    struct sockaddr_in targetAddr;
    memcpy(&targetAddr, res->ai_addr, sizeof(targetAddr));
    freeaddrinfo(res);

    for (int ttl = 1; ttl <= maxHops && !reached; ttl++) {
        int sendSock = socket(AF_INET, SOCK_DGRAM, 0);
        int recvSock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sendSock < 0 || recvSock < 0) {
            rl += snprintf(raw + rl, sizeof(raw) - rl,
                           "%2d: socket failed (need root for raw ICMP)\n", ttl);
            if (sendSock >= 0) close(sendSock);
            if (recvSock >= 0) close(recvSock);
            // Fall back to basic hop count report
            addProperty(props, pc, "Note", "Raw socket unavailable — try TCP traceroute via Qt");
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
        ssize_t n = recvfrom(recvSock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);

        if (n > 0) {
            char fromIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, fromIP, sizeof(fromIP));
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%2d: %s\n", ttl, fromIP);
            addProperty(props, pc, "Hop", fromIP);

            // Check if we reached the target
            if (from.sin_addr.s_addr == targetAddr.sin_addr.s_addr) {
                reached = true;
            }
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
                      reached ? "Traceroute reached target" : "Traceroute completed (may be incomplete)",
                      "UDP + raw ICMP traceroute (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdMtuDiagLinux — MTU from /sys/class/net
// ===========================================================================
NdDiagnosticResult* NdMtuDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    DIR* netDir = opendir("/sys/class/net");
    if (netDir) {
        struct dirent* entry;
        while ((entry = readdir(netDir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            char mtu[32] = {}, path[256];
            snprintf(path, sizeof(path), "/sys/class/net/%s/mtu", entry->d_name);
            FILE* f = fopen(path, "r");
            if (f) {
                if (fgets(mtu, sizeof(mtu), f)) { mtu[strcspn(mtu, "\n")] = 0; fclose(f); }
                else { fclose(f); continue; }
            } else continue;
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: MTU=%s\n", entry->d_name, mtu);
            addProperty(props, pc, entry->d_name, mtu);
        }
        closedir(netDir);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return makeResult(ND_DIAG_MTU_DISCOVERY, 0,
                      pc > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING,
                      pc > 0 ? "MTU values retrieved" : "No interfaces found",
                      "/sys/class/net/<iface>/mtu (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdPortScanDiagLinux — port scan via Qt C++ (QTcpSocket)
// ===========================================================================
NdDiagnosticResult* NdPortScanDiagLinux::execute(const NdDiagnosticInput*) {
    return makeResult(ND_DIAG_PORT_SCAN, 0, ND_STATUS_INFO,
                      "Port scan via Qt", "QTcpSocket (no CLI)", 0, nullptr, 0, nullptr);
}

// ===========================================================================
// NdCellularDiagLinux — desktop: no cellular
// ===========================================================================
NdDiagnosticResult* NdCellularDiagLinux::execute(const NdDiagnosticInput*) {
    return makeResult(ND_DIAG_CELLULAR_INFO, ND_ERR_NOT_SUPPORTED, ND_STATUS_SKIPPED,
                      "Desktop — no cellular", "", 0, nullptr, 0, nullptr);
}

// ===========================================================================
// NdDnsPollutionDiagLinux — DNS pollution detection via trusted DNS reference
// ===========================================================================
NdDiagnosticResult* NdDnsPollutionDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;

    static const char* domains[] = {"aliyun.com", "qq.com", "baidu.com", nullptr};
    int mismatched = 0, tested = 0;

    for (int i = 0; domains[i]; i++) {
        char sysIP[INET_ADDRSTRLEN] = {};
        // System DNS
        struct addrinfo hints = {}, *resSys;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        int retSys = getaddrinfo(domains[i], nullptr, &hints, &resSys);
        if (retSys != 0) {
            rl += snprintf(raw + rl, sizeof(raw) - rl,
                           "%s: system DNS failed (%s)\n", domains[i], gai_strerror(retSys));
            continue;
        }
        inet_ntop(AF_INET, &((struct sockaddr_in*)resSys->ai_addr)->sin_addr, sysIP, sizeof(sysIP));
        freeaddrinfo(resSys);

        // Trusted DNS: send real DNS query to 223.5.5.5:53
        char trustedIP[INET_ADDRSTRLEN] = {};
        int udp = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp >= 0) {
            // Build minimal DNS query for A record
            unsigned char qbuf[512] = {};
            qbuf[0] = 0x12; qbuf[1] = 0x34;           // TXID
            qbuf[2] = 0x01; qbuf[3] = 0x00;           // RD=1
            qbuf[5] = 0x01;                            // QDCOUNT=1
            int qpos = 12;
            // Encode domain name as labels
            char tmp[128]; snprintf(tmp, sizeof(tmp), "%s", domains[i]);
            char* tok = strtok(tmp, ".");
            while (tok) {
                int len = (int)strlen(tok);
                qbuf[qpos++] = (unsigned char)len;
                memcpy(qbuf + qpos, tok, len); qpos += len;
                tok = strtok(nullptr, ".");
            }
            qbuf[qpos++] = 0x00;                       // terminating zero-length label
            qbuf[qpos++] = 0x00; qbuf[qpos++] = 0x01;  // QTYPE=A
            qbuf[qpos++] = 0x00; qbuf[qpos++] = 0x01;  // QCLASS=IN

            struct sockaddr_in trustAddr = {};
            trustAddr.sin_family = AF_INET;
            trustAddr.sin_port = htons(53);
            // 223.5.5.5 is a valid dotted-quad literal; inet_addr never fails here.
            // Guard kept for future configurable-address support.
            trustAddr.sin_addr.s_addr = inet_addr("223.5.5.5");
            if (sendto(udp, (const char*)qbuf, qpos, 0, (struct sockaddr*)&trustAddr, sizeof(trustAddr)) >= 0) {
                struct timeval tv = {2, 0};
                setsockopt(udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                unsigned char rbuf[512] = {};
                socklen_t fromLen = sizeof(trustAddr);
                int n = (int)recvfrom(udp, (char*)rbuf, sizeof(rbuf), 0, (struct sockaddr*)&trustAddr, &fromLen);
                if (n >= 12) {
                // Check TC (truncation) flag — ignore truncated responses
                if (!(rbuf[2] & 0x02)) {
                int anc = (rbuf[4] << 8) | rbuf[5];    // ANCOUNT
                int rpos = 12;
                // Skip question section (handle both labels and compression pointers)
                bool qCompressed = false;
                while (rpos < n && rbuf[rpos] != 0) {
                    if ((rbuf[rpos] & 0xc0) == 0xc0) { rpos += 2; qCompressed = true; break; }
                    rpos += rbuf[rpos] + 1;
                    if (rpos >= n) break;
                }
                if (!qCompressed && rpos < n && rbuf[rpos] == 0) rpos++; // skip terminating null label (only if not compressed)
                rpos += 4; // QTYPE(2) + QCLASS(2)
                // Parse answers
                for (int a = 0; a < anc && rpos + 10 <= n; a++) {
                    // Skip NAME (may be compressed or label sequence)
                    if ((rbuf[rpos] & 0xc0) == 0xc0) { rpos += 2; }
                    else {
                        bool aCompressed = false;
                        while (rpos < n && rbuf[rpos] != 0) {
                            if ((rbuf[rpos] & 0xc0) == 0xc0) { rpos += 2; aCompressed = true; break; }
                            rpos += rbuf[rpos] + 1;
                            if (rpos >= n) break;
                        }
                        if (!aCompressed && rpos < n && rbuf[rpos] == 0) rpos++;
                    }
                    if (rpos + 10 > n) break;
                    rpos += 8; // TYPE(2) + CLASS(2) + TTL(4)
                    int rdlen = (rbuf[rpos] << 8) | rbuf[rpos + 1]; rpos += 2;
                    if (rdlen == 4 && rpos + 4 <= n) {
                        snprintf(trustedIP, sizeof(trustedIP), "%d.%d.%d.%d",
                                 rbuf[rpos], rbuf[rpos+1], rbuf[rpos+2], rbuf[rpos+3]);
                        break;
                    }
                    rpos += rdlen;
                }
                } // if (!(rbuf[2] & 0x02)) — TC check
            }
            close(udp);
            } // if (sendto >= 0)
        }

        rl += snprintf(raw + rl, sizeof(raw) - rl,
                       "%s => sys=%s trust=%s\n", domains[i], sysIP,
                       trustedIP[0] ? trustedIP : "unreachable");
        addProperty(props, pc, domains[i], sysIP);
        if (trustedIP[0]) {
            if (strcmp(sysIP, trustedIP) != 0) mismatched++;
            tested++;
        }
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[256];
    snprintf(sum, sizeof(sum), "%d domains resolved, %d mismatched (vs system DNS)",
             tested, mismatched);
    return makeResult(ND_DIAG_DNS_POLLUTION, 0,
                      ND_STATUS_PASS, sum,
                      "getaddrinfo() — system DNS (no CLI)", d, props, pc, raw);
}

// ===========================================================================
// NdSpeedTestDiagLinux — Speed test via HTTP GET over TCP
// ===========================================================================
NdDiagnosticResult* NdSpeedTestDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[1024] = {}; int rl = 0;

    // Simple HTTP GET to measure throughput
    const char* host = "www.aliyun.com";
    const char* path = "/";
    int64_t bytes = 0;
    double mbps = 0;

    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "80", &hints, &res) == 0) {
        int sock = socket(res->ai_family, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct timeval tv = {10, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            if (connect(sock, res->ai_addr, res->ai_addrlen) == 0) {
                char req[256];
                snprintf(req, sizeof(req),
                         "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                         path, host);
                send(sock, req, strlen(req), 0);

                auto dlStart = std::chrono::steady_clock::now();
                char buf[8192];
                ssize_t n;
                while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) bytes += n;
                auto dlEnd = std::chrono::steady_clock::now();
                auto dlMs = std::chrono::duration_cast<std::chrono::milliseconds>(dlEnd - dlStart).count();
                mbps = dlMs > 0 ? (bytes * 8.0) / (dlMs * 1000.0) : 0;
            }
            close(sock);
        }
        freeaddrinfo(res);
    }

    rl = snprintf(raw, sizeof(raw), "Downloaded %lld bytes, %.1f Mbps\n", (long long)bytes, mbps);

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), bytes > 0 ? "%.1f Mbps" : "Speed test failed", mbps);
    return makeResult(ND_DIAG_SPEED_TEST, 0,
                      bytes > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, sum,
                      "HTTP GET (no CLI)", d, props, pc, raw);
}
