#include "nd_diagnostic_base.h"
#include "nd_platform_bases.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

int NdDiagnosticBase::safeAppend(char* buf, int bufSize, int* pos, const char* fmt, ...) {
    if (*pos >= bufSize - 1) return 0; // buffer full, clip silently
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *pos, bufSize - *pos, fmt, args);
    va_end(args);
    if (n < 0) return 0;
    if (*pos + n >= bufSize - 1) {
        buf[bufSize - 1] = 0;
        *pos = bufSize - 1;
    } else {
        *pos += n;
    }
    return n;
}
#ifndef _WIN32
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif

NdDiagnosticResult* NdDiagnosticBase::makeResult(
    NdDiagnosticId id, int32_t err, NdStatus st, const char* sum,
    const char* det, int64_t dur, NdProperty* p, int32_t pc,
    const char* raw, const char* errMsg, int32_t deg, const char* degNote) {
    auto* r = new NdDiagnosticResult();
    r->diag_id = id; r->error_code = err; r->status = st;
    r->summary = sum ? strdup(sum) : nullptr;
    r->details = det ? strdup(det) : nullptr;
    r->duration_us = dur;
    r->property_count = pc; r->properties = p;
    r->raw_output = raw ? strdup(raw) : nullptr;
    r->error_msg = errMsg ? strdup(errMsg) : nullptr;
    r->is_degraded = deg;
    r->degradation_note = degNote ? strdup(degNote) : nullptr;
    r->perm_count = 0; r->permissions = nullptr;
    return r;
}

void NdDiagnosticBase::addProperty(NdProperty*& props, int32_t& count,
                                    const char* label, const char* value) {
    NdProperty* tmp = (NdProperty*)realloc(props, (count + 1) * sizeof(NdProperty));
    if (!tmp) return; // allocation failed — keep existing props intact
    props = tmp;
    // Zero-initialize to avoid garbage on partial OOM
    props[count].label = nullptr;
    props[count].value = nullptr;
    props[count].label = label ? strdup(label) : nullptr;
    props[count].value = value ? strdup(value) : nullptr;
    count++;
}

void NdDiagnosticBase::markDegraded(NdDiagnosticResult* r, const char* note) {
    r->is_degraded = 1;
    r->degradation_note = note ? strdup(note) : nullptr;
}

int NdPosixBase::enumerateInterfaces(IfaceInfo* out, int maxCount) {
#ifndef _WIN32
    struct ifaddrs* ifa2;
    if (getifaddrs(&ifa2) != 0) return -1;
    int count = 0;
    for (struct ifaddrs* ifa = ifa2; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        int family = ifa->ifa_addr->sa_family;
        if (family != AF_INET) continue;
        if (count >= maxCount) break;
        IfaceInfo& info = out[count];
        strncpy(info.name, ifa->ifa_name, sizeof(info.name)-1);
        info.flags = ifa->ifa_flags;
        info.isLoopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
        memset(info.mac, 0, sizeof(info.mac));
        snprintf(info.mac, sizeof(info.mac), "?:?:?:?:?:?");
        struct ifreq ifr; memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ-1);
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) { ioctl(sock, SIOCGIFMTU, &ifr); close(sock); }
        info.mtu = ifr.ifr_mtu > 0 ? ifr.ifr_mtu : 1500;
        info.isWireless = strstr(ifa->ifa_name, "wl") || strstr(ifa->ifa_name, "Wi");
        count++;
    }
    freeifaddrs(ifa2);
    return count;
#else
    (void)out; (void)maxCount;
    return 0; // Windows: no POSIX getifaddrs
#endif
}
