// =============================================================================
// nd_platform_bases.h — Platform-specific base classes
//
// These provide shared OS-level primitives (e.g. getifaddrs() on POSIX,
// Win32 API wrappers on Windows).  Concrete diagnostic classes multiply-
// inherit from one of these + NdDiagnosticBase.
// =============================================================================

#ifndef ND_PLATFORM_BASES_H
#define ND_PLATFORM_BASES_H

#include "nd_diagnostic_base.h"

// =============================================================================
// NdPosixBase — POSIX systems (Linux / macOS / Android / iOS)
// =============================================================================
class NdPosixBase {
protected:
    struct IfaceInfo {
        char name[32];
        char mac[18];
        int  flags;       // IFF_UP, IFF_RUNNING, etc.
        int  mtu;
        bool isLoopback;
        bool isWireless;
    };

    /// Enumerate all network interfaces via getifaddrs().
    /// Returns count, or -1 on failure.
    int enumerateInterfaces(IfaceInfo* out, int maxCount);

    /// Get a single interface by name.
    bool getInterfaceByName(const char* name, IfaceInfo* out);

    /// Resolve hostname to IP addresses via getaddrinfo().
    bool resolveHost(const char* host, char* ipv4Buf, int ipv4Size,
                     char* ipv6Buf, int ipv6Size, int* ipv4Count, int* ipv6Count);
};

// =============================================================================
// NdBsdBase — BSD-heritage systems (macOS / iOS)
// =============================================================================
class NdBsdBase : public NdPosixBase {
protected:
    /// Get DNS servers from SystemConfiguration framework (macOS/iOS).
    bool getDnsServers(char* outBuf, int bufSize);
};

// =============================================================================
// NdLinuxBase — Linux-specific primitives
// =============================================================================
class NdLinuxBase : public NdPosixBase {
protected:
    /// Get Wi-Fi info via nl80211 netlink socket.
    /// Fills ssid, bssid, signal_dBm, freq_mHz, channel.
    bool nl80211GetWifiInfo(const char* iface,
                            char* ssid, int ssidSize,
                            char* bssid, int bssidSize,
                            int* signaldBm,
                            int* freqMhz,
                            int* channel);

    /// Get routing table via netlink RTM_GETROUTE.
    int netlinkGetRoutes(char* outBuf, int bufSize);

    /// Get ARP/neighbour table via netlink RTM_GETNEIGH.
    int netlinkGetNeighbors(char* outBuf, int bufSize);
};

// =============================================================================
// NdWindowsBase — Windows Win32 API wrappers
// =============================================================================
class NdWindowsBase {
protected:
    struct WinAdapterInfo {
        wchar_t name[128];
        wchar_t description[256];
        char    mac[18];
        int     status;    // 1=up, 0=down
        int64_t speedBps;
        int     mtu;
        bool    isWireless;
    };

    /// Enumerate adapters via GetAdaptersAddresses().
    int enumerateAdapters(WinAdapterInfo* out, int maxCount);

    /// Get Wi-Fi info via WlanQueryInterface().
    bool getWifiInfo(char* ssid, int ssidSize,
                     char* bssid, int bssidSize,
                     int* signalPercent,
                     int* channel,
                     char* security, int secSize);

    /// Get routing table via GetIpForwardTable2().
    int getRoutes(char* outBuf, int bufSize);

    /// Get ARP table via GetIpNetTable2().
    int getArpTable(char* outBuf, int bufSize);

    /// Get active TCP connections via GetExtendedTcpTable().
    int getTcpConnections(char* outBuf, int bufSize);
};

// =============================================================================
// NdIosBase — iOS sandbox-aware primitives
// =============================================================================
class NdIosBase : public NdBsdBase {
protected:
    /// Check if a specific entitlement is granted.
    bool hasEntitlement(const char* entitlementId);

    /// Get current Wi-Fi SSID (requires wifi-info entitlement).
    bool getWifiSsid(char* ssid, int ssidSize);
};

// =============================================================================
// NdAndroidBase — Android JNI-aware primitives
// =============================================================================
class NdAndroidBase : public NdLinuxBase {
protected:
    /// Check Android runtime permission.
    bool hasPermission(const char* permissionName);

    /// Get Wi-Fi info via WifiManager JNI.
    bool getWifiInfo(char* ssid, int ssidSize,
                     char* bssid, int bssidSize,
                     int* signaldBm,
                     int* frequencyMhz);

    /// Get cellular info via TelephonyManager JNI.
    bool getCellularInfo(char* carrier, int carrierSize,
                         char* radioType, int radioSize,
                         int* signaldBm,
                         int* signalLevel,
                         bool* isRoaming,
                         bool* dataConnected);
};

#endif // ND_PLATFORM_BASES_H
