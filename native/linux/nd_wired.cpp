#include "nd_wired.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <dirent.h>

// Read a /sys/class/net/<iface>/<file> value; returns empty string on failure.
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

// Check if an interface is wireless (has phy80211 symlink or wireless/ dir).
static bool isWireless(const char* iface) {
    char path[256];
    // Modern kernels: /sys/class/net/<iface>/phy80211 symlink
    snprintf(path, sizeof(path), "/sys/class/net/%s/phy80211", iface);
    DIR* d = opendir(path);
    if (d) { closedir(d); return true; }
    // Older kernels: /sys/class/net/<iface>/wireless/ directory
    snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", iface);
    d = opendir(path);
    if (d) { closedir(d); return true; }
    return false;
}

NdDiagnosticResult* NdWiredDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    bool found = false;

    // Enumerate /sys/class/net/ for non-loopback, non-wireless interfaces
    DIR* netDir = opendir("/sys/class/net");
    if (netDir) {
        struct dirent* entry;
        while ((entry = readdir(netDir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, "lo") == 0) continue;
            if (isWireless(entry->d_name)) continue;

            found = true;
            char carrier[32] = "unknown";
            char speed[32] = "unknown";
            char duplex[32] = "unknown";
            char operstate[32] = "unknown";

            readSysNet(entry->d_name, "carrier", carrier, sizeof(carrier));
            readSysNet(entry->d_name, "speed", speed, sizeof(speed));
            readSysNet(entry->d_name, "duplex", duplex, sizeof(duplex));
            readSysNet(entry->d_name, "operstate", operstate, sizeof(operstate));

            char val[256];
            snprintf(val, sizeof(val), "%s | %s Mbps | duplex=%s | carrier=%s",
                     operstate, speed, duplex, carrier);
            addProperty(props, pc, entry->d_name, val);
            rl += snprintf(raw + rl, sizeof(raw) - rl,
                           "%s: operstate=%s speed=%s duplex=%s carrier=%s\n",
                           entry->d_name, operstate, speed, duplex, carrier);
        }
        closedir(netDir);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), found ? "Wired adapter%s detected" : "No wired adapter found",
             found ? "s" : "");
    return makeResult(ND_DIAG_WIRED, 0,
                      found ? ND_STATUS_PASS : ND_STATUS_INFO, sum,
                      "/sys/class/net enumeration (no CLI)", d, props, pc, raw);
}
