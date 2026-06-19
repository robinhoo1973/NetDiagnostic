#include "nd_wifi.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <dirent.h>
#include <mutex>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
// NOTE: Do NOT include <net/if.h> — it conflicts with <linux/if.h> pulled
// in by <linux/wireless.h>.  Use linux/if.h types directly.
// IFNAMSIZ is defined by both; we rely on the kernel header version.
#include <unistd.h>
#include <cstdlib>

// Check if an interface is wireless via /sys/class/net/<iface>/phy80211
static bool isWireless(const char* iface) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/net/%s/phy80211", iface);
    DIR* d = opendir(path);
    if (d) { closedir(d); return true; }
    snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", iface);
    d = opendir(path);
    if (d) { closedir(d); return true; }
    return false;
}

// Read a /sys/class/net/<iface>/<file> value
static bool readSysNet(const char* iface, const char* file, char* out, size_t outLen) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/net/%s/%s", iface, file);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    bool ok = fgets(out, (int)outLen, f) != nullptr;
    if (ok) out[strcspn(out, "\n")] = 0;
    fclose(f);
    return ok;
}

// ===========================================================================
// WiFi helpers using Wireless Extensions (WE) ioctls
// WE is deprecated in favour of nl80211 but remains the simplest in-kernel
// API that works without external libraries on virtually all Linux systems.
// Future: migrate to netlink nl80211 for 6GHz / WPA3 details.
// ===========================================================================

static int wifi_sock = -1;
static std::mutex wifi_mutex;

static int getWifiSock() {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    if (wifi_sock < 0) wifi_sock = socket(AF_INET, SOCK_DGRAM, 0);
    return wifi_sock;
}

static void closeWifiSock() {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    if (wifi_sock >= 0) { close(wifi_sock); wifi_sock = -1; }
}

// Get ESSID via SIOCGIWESSID
static bool getEssid(const char* iface, char* essid, size_t essidLen) {
    int s = getWifiSock();
    if (s < 0) return false;
    struct iwreq wrq = {};
    wrq.u.essid.pointer = (caddr_t)essid;
    wrq.u.essid.length = (__u16)(essidLen - 1);
    wrq.u.essid.flags = 0;
    strncpy(wrq.ifr_name, iface, IFNAMSIZ);
    if (ioctl(s, SIOCGIWESSID, &wrq) < 0) return false;
    essid[wrq.u.essid.length] = 0;
    return wrq.u.essid.length > 0;
}

// Get frequency via SIOCGIWFREQ
static bool getFrequency(const char* iface, double* freqGHz) {
    int s = getWifiSock();
    if (s < 0) return false;
    struct iwreq wrq = {};
    strncpy(wrq.ifr_name, iface, IFNAMSIZ);
    if (ioctl(s, SIOCGIWFREQ, &wrq) < 0) return false;
    // Frequency in Hz * 10^5 for channels < 1GHz, Hz for higher
    double f = (double)wrq.u.freq.m;
    int e = wrq.u.freq.e;
    while (e > 6) { f *= 10; e--; }
    while (e < 6) { f /= 10; e++; }
    // Convert Hz to GHz
    *freqGHz = f / 1e9;
    return f > 0;
}

// Get signal level via SIOCGIWSTATS
// Returns dBm if available, otherwise relative quality number.
static bool getSignalQuality(const char* iface, char* out, size_t outLen) {
    int s = getWifiSock();
    if (s < 0) return false;
    struct iwreq wrq = {};
    snprintf(wrq.ifr_name, IFNAMSIZ, "%s", iface);
    struct iw_statistics stats = {};
    wrq.u.data.pointer = &stats;
    wrq.u.data.length = sizeof(stats);
    wrq.u.data.flags = 0;
    if (ioctl(s, SIOCGIWSTATS, &wrq) < 0) return false;

    // Prefer dBm if available (level), fall back to relative qual
    if (stats.qual.updated & IW_QUAL_DBM) {
        snprintf(out, outLen, "%d dBm", (int)stats.qual.level - 0x100);
    } else if (stats.qual.updated & IW_QUAL_LEVEL_UPDATED) {
        snprintf(out, outLen, "%d/%d (rel)", (int)stats.qual.qual, (int)stats.qual.level);
    } else {
        snprintf(out, outLen, "%d (qual)", (int)stats.qual.qual);
    }
    return true;
}

// Get AP MAC via SIOCGIWAP
static bool getApMac(const char* iface, char* mac, size_t macLen) {
    int s = getWifiSock();
    if (s < 0) return false;
    struct iwreq wrq = {};
    strncpy(wrq.ifr_name, iface, IFNAMSIZ);
    if (ioctl(s, SIOCGIWAP, &wrq) < 0) return false;
    snprintf(mac, macLen, "%02x:%02x:%02x:%02x:%02x:%02x",
             (unsigned char)wrq.u.ap_addr.sa_data[0],
             (unsigned char)wrq.u.ap_addr.sa_data[1],
             (unsigned char)wrq.u.ap_addr.sa_data[2],
             (unsigned char)wrq.u.ap_addr.sa_data[3],
             (unsigned char)wrq.u.ap_addr.sa_data[4],
             (unsigned char)wrq.u.ap_addr.sa_data[5]);
    return true;
}

// Get bitrate via SIOCGIWRATE
static bool getBitrate(const char* iface, double* mbps) {
    int s = getWifiSock();
    if (s < 0) return false;
    struct iwreq wrq = {};
    strncpy(wrq.ifr_name, iface, IFNAMSIZ);
    if (ioctl(s, SIOCGIWRATE, &wrq) < 0) return false;
    // Value in bps (scaled by e)
    double r = (double)wrq.u.bitrate.value;
    *mbps = r / 1e6;
    return r > 0;
}

// ===========================================================================
// NdWifiDiagLinux::execute — main diagnostic
// ===========================================================================

static const NdPermissionInfo s_wifiPermsLinux[] = {
    {"wifi.ioctl", ND_PERM_UNKNOWN, "WiFi information (SIOCGIW*)",
     "Requires a wireless interface.  WE ioctls need no special privileges."},
};

const NdPermissionInfo* NdWifiDiagLinux::requiredPermissions(int* c) const {
    *c = 1; return s_wifiPermsLinux;
}

NdDiagnosticResult* NdWifiDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    bool connected = false;
    char ssid[128] = "N/A";
    char iface[32] = {};

    // Step 1: find wireless interfaces via /sys/class/net/
    DIR* netDir = opendir("/sys/class/net");
    if (netDir) {
        struct dirent* entry;
        while ((entry = readdir(netDir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            if (isWireless(entry->d_name)) {
                strncpy(iface, entry->d_name, sizeof(iface) - 1);
                break; // use first wireless interface
            }
        }
        closedir(netDir);
    }

    rl += snprintf(raw + rl, sizeof(raw) - rl, "Interface: %s\n",
                   iface[0] ? iface : "none");

    if (iface[0]) {
        // Step 2: ESSID (WiFi network name)
        getEssid(iface, ssid, sizeof(ssid));
        if (ssid[0]) {
            connected = true;
            addProperty(props, pc, "SSID", ssid);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "SSID: %s\n", ssid);
        }

        // Step 3: AP MAC
        char apMac[32] = {};
        if (getApMac(iface, apMac, sizeof(apMac)) && apMac[0]) {
            addProperty(props, pc, "AP MAC", apMac);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "AP MAC: %s\n", apMac);
        }

        // Step 4: frequency
        double freqGHz = 0;
        if (getFrequency(iface, &freqGHz) && freqGHz > 0) {
            // Determine band
            const char* band = freqGHz > 5.0 ? "5 GHz" :
                               freqGHz > 2.4 ? "2.4 GHz" : "unknown";
            char fStr[64];
            snprintf(fStr, sizeof(fStr), "%.3f GHz (%s)", freqGHz, band);
            addProperty(props, pc, "Frequency", fStr);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "Frequency: %s\n", fStr);
        }

        // Step 5: signal quality via WE statistics
        char sigStr[64] = {};
        if (getSignalQuality(iface, sigStr, sizeof(sigStr))) {
            if (sigStr[0]) {
                addProperty(props, pc, "Signal", sigStr);
                rl += snprintf(raw + rl, sizeof(raw) - rl, "Signal: %s\n", sigStr);
            }
        }

        // Step 6: TX bitrate
        double bitrate = 0;
        if (getBitrate(iface, &bitrate) && bitrate > 0) {
            char brStr[64];
            snprintf(brStr, sizeof(brStr), "%.1f Mbps", bitrate);
            addProperty(props, pc, "TX Rate", brStr);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "TX Rate: %s\n", brStr);
        }

        // Step 7: supplementary /sys/class/net info
        char mtu[32] = {}, operstate[32] = {};
        if (readSysNet(iface, "mtu", mtu, sizeof(mtu)))
            addProperty(props, pc, "MTU", mtu);
        if (readSysNet(iface, "operstate", operstate, sizeof(operstate)))
            addProperty(props, pc, "Operstate", operstate);
    }

    closeWifiSock();

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[256];
    snprintf(sum, sizeof(sum), connected ? "Connected to %s" : "%s",
             connected ? ssid : (iface[0] ? "WiFi interface found — not connected"
                                          : "No WiFi interface detected"));
    auto* r = makeResult(ND_DIAG_WIFI, 0,
                         connected ? ND_STATUS_PASS : ND_STATUS_INFO,
                         sum,
                         iface[0] ? "/sys/class/net + WE ioctls (no CLI)"
                                  : "No wireless interface",
                         d, props, pc, raw);
    if (!connected) markDegraded(r, "No active WiFi connection — signal/channel info unavailable");
    return r;
}
