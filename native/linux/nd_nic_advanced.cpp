#include "nd_nic_advanced.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <dirent.h>
#include <unistd.h>

// Read a PCI device sysfs attribute; returns empty on failure.
// FIXED: increased buffer sizes to prevent ARM64 readlink() overflow
static bool readPciAttr(const char* devName, const char* attr, char* out, size_t outLen) {
    char path[320];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/%s", devName, attr);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    bool ok = fgets(out, (int)outLen, f) != nullptr;
    if (ok) out[strcspn(out, "\n")] = 0;
    fclose(f);
    return ok;
}

// Check if device class indicates a network controller (0x02xx).
static bool isNetworkClass(const char* classHex) {
    // PCI class is 24-bit: 0xCCCCSS where CC=base class, SS=subclass
    unsigned int cls = 0;
    if (sscanf(classHex, "0x%x", &cls) == 1) {
        return (cls >> 16) == 0x02; // Network controller
    }
    return false;
}

NdDiagnosticResult* NdNicAdvancedDiagLinux::execute(const NdDiagnosticInput* input) {
    auto t0 = std::chrono::steady_clock::now();
    NdProperty* props = nullptr; int32_t pc = 0;
    char raw[4096] = {}; int rl = 0;
    int found = 0;

    // Enumerate /sys/bus/pci/devices/
    DIR* pciDir = opendir("/sys/bus/pci/devices");
    if (pciDir) {
        struct dirent* entry;
        while ((entry = readdir(pciDir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;

            char clsHex[32] = {};
            if (!readPciAttr(entry->d_name, "class", clsHex, sizeof(clsHex))) continue;
            if (!isNetworkClass(clsHex)) continue;

            char vendor[64] = {}, device[64] = {}, driver[64] = {};

            readPciAttr(entry->d_name, "vendor", vendor, sizeof(vendor));
            readPciAttr(entry->d_name, "device", device, sizeof(device));

            // Driver is a symlink; read via driver/module link
            char driverPath[480];
            snprintf(driverPath, sizeof(driverPath),
                     "/sys/bus/pci/devices/%s/driver", entry->d_name);
            char driverTgt[512] = {};
            ssize_t len = readlink(driverPath, driverTgt, sizeof(driverTgt) - 1);
            if (len > 0 && len < (ssize_t)sizeof(driverTgt)) {
                driverTgt[len] = '\0';
                // Extract module name from path (last component)
                const char* lastSlash = strrchr(driverTgt, '/');
                snprintf(driver, sizeof(driver), "%s",
                         lastSlash ? lastSlash + 1 : driverTgt);
            } else {
                snprintf(driver, sizeof(driver), "(none)");
            }

            char val[256];
            snprintf(val, sizeof(val), "%s | %s | driver=%s", vendor, device, driver);
            addProperty(props, pc, entry->d_name, val);
            rl += snprintf(raw + rl, sizeof(raw) - rl, "%s: vendor=%s device=%s driver=%s class=%s\n",
                           entry->d_name, vendor, device, driver, clsHex);
            found++;
        }
        closedir(pciDir);
    } else {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        return makeResult(ND_DIAG_NIC_ADVANCED, ND_ERR_INTERNAL, ND_STATUS_WARNING,
                          "/sys/bus/pci/devices not accessible",
                          "Kernel may not expose PCI sysfs (check CONFIG_PCI)",
                          d, nullptr, 0, nullptr);
    }

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    char sum[128];
    snprintf(sum, sizeof(sum), "%d NIC hardware entr%s found in /sys/bus/pci/devices",
             found, found == 1 ? "y" : "ies");
    return makeResult(ND_DIAG_NIC_ADVANCED, 0,
                      found > 0 ? ND_STATUS_PASS : ND_STATUS_WARNING, sum,
                      "/sys/bus/pci/devices enumeration (no CLI)", d, props, pc, raw);
}
