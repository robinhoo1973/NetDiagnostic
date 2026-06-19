#include <chrono>
// =============================================================================
// nd_network_adapters.cpp — Linux implementation
// =============================================================================

#include "nd_network_adapters.h"
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <sys/ioctl.h>

NdDiagnosticResult*
NdNetworkAdapterDiagnosticLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();

    NdProperty* props = nullptr;
    int32_t     propCount = 0;
    char        rawBuf[8192] = {};
    int         rawLen = 0;
    int         upCount = 0, totalCount = 0;
    char        line[256];

    // --- Enumerate interfaces via getifaddrs ---
    IfaceInfo ifaces[64];
    int n = enumerateInterfaces(ifaces, 64);

    if (n < 0) {
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        return makeResult(ND_DIAG_NETWORK_ADAPTERS, ND_ERR_INTERNAL,
            ND_STATUS_ERROR,
            "Failed to enumerate network adapters",
            "getifaddrs() returned error",
            dur, nullptr, 0, "getifaddrs() failed");
    }

    totalCount = n;

    for (int i = 0; i < n; i++) {
        auto& iface = ifaces[i];
        bool isUp = (iface.flags & IFF_UP) && (iface.flags & IFF_RUNNING);
        if (isUp) upCount++;

        // Build property line
        snprintf(line, sizeof(line), "%s | %s | %s | MTU=%d",
            iface.name,
            isUp ? "UP" : "DOWN",
            iface.mac,
            iface.mtu);
        addProperty(props, propCount, iface.name, line);

        // Build raw output
        rawLen += snprintf(rawBuf + rawLen, sizeof(rawBuf) - rawLen,
            "%-10s  %-4s  %-17s  MTU=%-5d  %s%s\n",
            iface.name,
            isUp ? "UP" : "DOWN",
            iface.mac,
            iface.mtu,
            iface.isLoopback ? "LOOPBACK " : "",
            iface.isWireless ? "WIRELESS" : "ETHERNET");
    }

    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();

    char summary[128];
    snprintf(summary, sizeof(summary), "%d / %d adapter(s) up", upCount, totalCount);

    NdStatus status = upCount > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING;

    return makeResult(ND_DIAG_NETWORK_ADAPTERS, 0, status,
        summary, "Enumerated via getifaddrs() + ioctl(SIOCGIFFLAGS)",
        dur, props, propCount, rawBuf);
}
