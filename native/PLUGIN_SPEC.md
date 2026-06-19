# NetDiagnostic Native Plugin Specification

## 1. Overview

The NetDiagnostic native plugin is a **C++ library** (static or shared) that provides
cross-platform network diagnostic capabilities via a pure-C ABI.  Qt C++ code
calls into it directly — no FFI layer required.

**Guiding principle:** zero CLI / shell dependencies.  Every diagnostic MUST
use only native OS APIs — Win32, POSIX, netlink, IOKit, JNI, etc.  No `popen`,
`system`, `Process.run`, or equivalent.

## 2. ABI Functions

All symbols are exported with `extern "C"` and `__declspec(dllexport)` /
`__attribute__((visibility("default")))` as appropriate.

### 2.1 `nd_version`

```c
const char* nd_version(void);
```

Returns a static version string, e.g. `"1.0.0"`.  Caller must NOT free.

### 2.2 `nd_init`

```c
int nd_init(void);
```

Initialises the plugin (creates the singleton diagnostic factory).
Returns 0 on success, non-zero on failure.  Idempotent — safe to call
multiple times.

### 2.3 `nd_shutdown`

```c
void nd_shutdown(void);
```

Destroys all diagnostic instances and releases resources.  After this call
`nd_init` must be called again before any other function.

### 2.4 `nd_run_diagnostic`

```c
NdDiagnosticResult* nd_run_diagnostic(const NdDiagnosticInput* input);
```

Runs a single diagnostic and returns a **heap-allocated** result.
Ownership transfers to the caller, who MUST free via `nd_free_result`.

Parameters are passed in `NdDiagnosticInput` (see §3.1).

Returns `NULL` on fatal error (invalid input, plugin not initialised).

### 2.5 `nd_check_permissions`

```c
int nd_check_permissions(
    NdDiagnosticId            id,
    NdPermissionInfo*         out_permissions,
    int                       max_count,
    int*                      out_count
);
```

Returns the permissions required by diagnostic `id`.  Writes up to
`max_count` entries into `out_permissions` and sets `*out_count` to the
actual number.  Returns the total number of permissions (may exceed
`max_count`).

Permission states (§3.4):
- `ND_PERM_GRANTED` — available
- `ND_PERM_DENIED`  — permanently denied
- `ND_PERM_NOT_APPLICABLE` — not relevant for this platform
- `ND_PERM_UNKNOWN` — can't determine at build time

### 2.6 `nd_free_result`

```c
void nd_free_result(NdDiagnosticResult* result);
```

Frees a result previously returned by `nd_run_diagnostic`, including all
internal `strdup`-allocated strings.  Passing `NULL` is a no-op.

## 3. Data Structures

### 3.1 `NdDiagnosticInput`

```c
typedef struct {
    NdDiagnosticId diag_id;
    const char    *target;       // hostname / IP / URL (nullable)
    int32_t        timeout_ms;   // max wait, 0 = use default
    int32_t        from_port;    // port scan range start
    int32_t        to_port;      // port scan range end
} NdDiagnosticInput;
```

### 3.2 `NdDiagnosticResult`

```c
typedef struct {
    NdDiagnosticId  diag_id;
    int32_t         error_code;       // 0 = success
    NdStatus        status;           // PASS / WARNING / FAIL / SKIPPED / ERROR / INFO
    const char     *summary;          // one-line human-readable summary (heap)
    const char     *details;          // full detail text (heap, may be multi-line)
    NdProperty     *properties;       // dynamic array of key-value pairs
    int32_t         property_count;   // number of entries in properties[]
    NdPermissionInfo *permissions;    // permissions consumed by this run
    int32_t         perm_count;       // number of entries in permissions[]
    int64_t         duration_us;      // wall-clock execution time (microseconds)
    const char     *raw_output;       // raw protocol output if applicable (heap)
    const char     *error_msg;        // error description on failure (heap)
    int32_t         is_degraded;      // 1 = running with reduced capability
    const char     *degradation_note; // explanation of what was limited (heap)
} NdDiagnosticResult;
```

### 3.3 `NdProperty`

```c
typedef struct {
    const char *label;   // e.g. "SSID", "Signal", "DNS Server"
    const char *value;   // e.g. "MyWiFi", "-47 dBm", "192.168.1.1"
} NdProperty;
```

### 3.4 `NdPermissionInfo`

```c
typedef enum {
    ND_PERM_GRANTED        = 0,
    ND_PERM_DENIED         = 1,
    ND_PERM_NOT_APPLICABLE = 2,
    ND_PERM_UNKNOWN        = 3,
} NdPermissionState;

typedef struct {
    const char        *id;            // permission identifier string
    NdPermissionState  state;         // current state
    const char        *friendly_name; // human-readable name
    const char        *fix_guide;     // instructions for the user
} NdPermissionInfo;
```

### 3.5 Error Codes (returned by `nd_init`, `nd_run_diagnostic`)

| Code | Name | Meaning |
|------|------|---------|
| 0  | `ND_OK`              | Success |
| 1  | `ND_ERR_TIMEOUT`      | Operation exceeded timeout |
| 2  | `ND_ERR_PERMISSION`   | Required permission not granted |
| 3  | `ND_ERR_NETWORK`      | Network unreachable / DNS failure |
| 4  | `ND_ERR_NOT_SUPPORTED` | Diagnostic not implemented on this platform |
| 99 | `ND_ERR_INTERNAL`     | Internal error (allocation, unexpected OS error) |

## 4. Diagnostic IDs

```c
typedef enum {
    // G1 — System & Adapters (0-6, + cellular 30)
    ND_DIAG_NETWORK_ADAPTERS  = 0,
    ND_DIAG_NIC_ADVANCED      = 1,
    ND_DIAG_WIFI              = 2,
    ND_DIAG_WIRED             = 3,
    ND_DIAG_DHCP              = 4,
    ND_DIAG_IP_CONFIG         = 5,
    ND_DIAG_ACTIVE_CONNECTIONS = 6,

    // G2 — Connectivity & Security (7-12)
    ND_DIAG_NETWORK_PROFILE   = 7,
    ND_DIAG_TCP_SETTINGS      = 8,
    ND_DIAG_DEFAULT_GATEWAY   = 9,
    ND_DIAG_ROUTING_TABLE     = 10,
    ND_DIAG_ARP_TABLE         = 11,
    ND_DIAG_PROXY_SETTINGS    = 12,

    // G3 — Internet & DNS (13-18)
    ND_DIAG_NETSKOPE_STATUS   = 13,
    ND_DIAG_DNS_SERVERS       = 14,
    ND_DIAG_DNS_CACHE         = 15,
    ND_DIAG_DNS_POLLUTION     = 16,
    ND_DIAG_INTERNET_CONNECTIVITY = 17,
    ND_DIAG_SPEED_TEST        = 18,

    // G4 — Remote Host (19-24)
    ND_DIAG_DNS_RESOLUTION    = 19,
    ND_DIAG_PING              = 20,
    ND_DIAG_TRACEROUTE        = 21,
    ND_DIAG_PATH_PING         = 22,
    ND_DIAG_MTU_DISCOVERY     = 23,
    ND_DIAG_PORT_SCAN         = 24,

    // G5 — URL & Protocol (25-29)
    ND_DIAG_TCP_CONNECT       = 25,
    ND_DIAG_SERVICE_BANNER    = 26,
    ND_DIAG_FTP               = 27,
    ND_DIAG_SSH               = 28,
    ND_DIAG_EMAIL             = 29,

    // G6 — Mobile / Cellular (30)
    ND_DIAG_CELLULAR_INFO     = 30,

    ND_DIAG_COUNT             = 31
} NdDiagnosticId;
```

## 5. Platform Support Matrix

| Diagnostic | Linux | Windows | macOS | Android | iOS | Implementation |
|------------|-------|---------|-------|---------|-----|----------------|
| ND_DIAG_NETWORK_ADAPTERS | ✅ | ✅ | ✅ | ✅ | ✅ | getifaddrs / GetAdaptersAddresses |
| ND_DIAG_NIC_ADVANCED | ✅ | ✅ | ✅ | ❌ | ❌ | /sys/bus/pci / Win32 registry / IOKit |
| ND_DIAG_WIFI | ✅ | ✅ | ✅ | ✅ | ⚠️ | nl80211 / WlanQueryInterface / CoreWLAN / JNI WifiManager / CNCopyCurrentNetworkInfo |
| ND_DIAG_WIRED | ✅ | ✅ | ✅ | ❌ | ❌ | /sys/class/net / Win32 / IOKit |
| ND_DIAG_DHCP | ✅ | ✅ | ✅ | ❌ | ❌ | netlink / Win32 DhcpRequestParams / SystemConfiguration |
| ND_DIAG_IP_CONFIG | ✅ | ✅ | ✅ | ❌ | ❌ | getifaddrs+netlink / GetAdaptersAddresses |
| ND_DIAG_ACTIVE_CONNECTIONS | ✅ | ✅ | ✅ | ❌ | ❌ | /proc/net/tcp / GetExtendedTcpTable |
| ND_DIAG_NETWORK_PROFILE | ✅ | ✅ | ✅ | ❌ | ❌ | netlink / Win32 Network List Manager / SCNetworkReachability |
| ND_DIAG_TCP_SETTINGS | ✅ | ✅ | ✅ | ❌ | ❌ | /proc/sys/net / Win32 registry |
| ND_DIAG_DEFAULT_GATEWAY | ✅ | ✅ | ✅ | ❌ | ❌ | netlink RTM_GETROUTE / GetIpForwardTable2 |
| ND_DIAG_ROUTING_TABLE | ✅ | ✅ | ✅ | ❌ | ❌ | netlink RTM_GETROUTE / GetIpForwardTable2 |
| ND_DIAG_ARP_TABLE | ✅ | ✅ | ✅ | ❌ | ❌ | /proc/net/arp+netlink RTM_GETNEIGH / GetIpNetTable2 |
| ND_DIAG_PROXY_SETTINGS | ✅ | ✅ | ✅ | ❌ | ❌ | env vars+gsettings / WinHTTP / SCDynamicStoreCopyProxies |
| ND_DIAG_NETSKOPE_STATUS | ✅ | ✅ | ✅ | ❌ | ❌ | /proc/net/tcp / Win32 process list |
| ND_DIAG_DNS_SERVERS | ✅ | ✅ | ✅ | ✅ | ✅ | resolv.conf+netlink / GetAdaptersAddresses / SCDynamicStore |
| ND_DIAG_DNS_CACHE | ✅ | ✅ | ✅ | ❌ | ❌ | nscd/systemd-resolved / Win32 DnsGetCacheDataTable |
| ND_DIAG_DNS_POLLUTION | ✅ | ✅ | ✅ | ❌ | ❌ | getaddrinfo+socket DNS queries |
| ND_DIAG_INTERNET_CONNECTIVITY | ✅ | ✅ | ✅ | ✅ | ✅ | TCP connect socket |
| ND_DIAG_SPEED_TEST | ✅ | ✅ | ✅ | ❌ | ❌ | TCP socket HTTP GET |
| ND_DIAG_DNS_RESOLUTION | ✅ | ✅ | ✅ | ✅ | ✅ | getaddrinfo |
| ND_DIAG_PING | ✅ | ✅ | ✅ | ❌ | ❌ | raw ICMP socket / IcmpSendEcho |
| ND_DIAG_TRACEROUTE | ✅ | ✅ | ✅ | ❌ | ❌ | UDP socket + TTL |
| ND_DIAG_PATH_PING | ✅ | ✅ | ✅ | ❌ | ❌ | composite ping+traceroute |
| ND_DIAG_MTU_DISCOVERY | ✅ | ✅ | ✅ | ❌ | ❌ | UDP socket + DF flag |
| ND_DIAG_PORT_SCAN | ✅ | ✅ | ✅ | ❌ | ❌ | TCP connect (via Qt QTcpSocket) |
| ND_DIAG_TCP_CONNECT | ✅ | ✅ | ✅ | ✅ | ✅ | TCP connect (via Qt QTcpSocket) |
| ND_DIAG_SERVICE_BANNER | ✅ | ✅ | ✅ | ✅ | ✅ | TCP connect + read |
| ND_DIAG_FTP | ✅ | ✅ | ✅ | ✅ | ✅ | TCP connect + read banner |
| ND_DIAG_SSH | ✅ | ✅ | ✅ | ✅ | ✅ | TCP connect + read banner |
| ND_DIAG_EMAIL | ✅ | ✅ | ✅ | ✅ | ✅ | TCP connect + read banner |
| ND_DIAG_CELLULAR_INFO | ❌ | ❌ | ❌ | ✅ | ✅ | JNI TelephonyManager / CTTelephonyNetworkInfo |

✅ = fully implemented with native APIs (no CLI)
⚠️ = requires OS entitlement, may be degraded
❌ = not supported on this platform (OS sandbox / no cellular hardware)

## 6. Platform-Specific Implementation Rules

### 6.1 Linux
- **Forbidden:** `popen`, `system`, `exec*`
- **Use instead:**
  - Network interfaces: `getifaddrs()`, `ioctl(SIOCGIF*)`
  - Routes / neighbours / links: netlink `NETLINK_ROUTE` socket (`RTM_GETROUTE`, `RTM_GETNEIGH`, `RTM_GETLINK`, `RTM_GETADDR`)
  - WiFi: netlink `nl80211` (generic netlink family)
  - Hardware info: `/sys/class/net/<iface>/`, `/sys/bus/pci/devices/*/`
  - TCP/UDP connections: `/proc/net/tcp`, `/proc/net/udp`
  - DNS config: `/etc/resolv.conf`, systemd-resolved D-Bus, netlink `RTM_GETDNS`
  - ARP: `/proc/net/arp` + netlink `RTM_GETNEIGH`
  - ICMP ping: raw socket `SOCK_RAW` + `IPPROTO_ICMP`
  - Traceroute: UDP socket + `IP_TTL` sockopt

### 6.2 Windows
- **Forbidden:** `_popen`, `system`, `CreateProcess` for CLI tools, PowerShell
- **Use instead:**
  - Network adapters: `GetAdaptersAddresses()` (iphlpapi)
  - Routes: `GetIpForwardTable2()` (netioapi)
  - ARP/neighbours: `GetIpNetTable2()` (netioapi)
  - WiFi: `WlanOpenHandle` + `WlanQueryInterface` (wlanapi)
  - TCP/UDP: `GetExtendedTcpTable()`, `GetExtendedUdpTable()` (iphlpapi)
  - DNS cache: `DnsGetCacheDataTable()` (dnsapi)
  - DHCP: `DhcpRequestParams()` (dhcpsapi)
  - ICMP ping: `IcmpSendEcho()` (icmpapi) — requires admin on some versions
  - Registry: `RegOpenKeyEx` + `RegQueryValueEx` (advapi32)
  - Proxy: WinHTTP `WinHttpGetIEProxyConfigForCurrentUser()`
  - Network list: `INetworkListManager` COM interface

### 6.3 macOS
- **Forbidden:** `popen`, `system`
- **Use instead:**
  - Network: `getifaddrs()`, `SCDynamicStoreCopyValue()`, `SCNetworkReachabilityCreateWithName()`
  - WiFi: CoreWLAN `CWWiFiClient` / `CWInterface`
  - DNS: `SCDynamicStoreCopyValue()` with `State:/Network/Service/.*/DNS`
  - Proxy: `SCDynamicStoreCopyProxies()`
  - Hardware: IOKit `IOServiceGetMatchingServices()`
  - ICMP: raw socket `SOCK_DGRAM` + `IPPROTO_ICMP`

### 6.4 Android
- **Forbidden:** `popen`, `system`, `dumpsys`
- **Use instead:**
  - Network: C `fopen("/proc/net/dev")`, `getifaddrs()`
  - WiFi: JNI → `WifiManager.getConnectionInfo()`
  - Cellular: JNI → `TelephonyManager.getAllCellInfo()` / `getSignalStrength()`
  - DNS: `getaddrinfo()`, read `/etc/resolv.conf`
  - TCP connect: standard POSIX sockets
  - Connectivity: JNI → `ConnectivityManager.getActiveNetworkInfo()`
  - For diagnostics that genuinely cannot work without root: return
    `ND_STATUS_SKIPPED` with clear `degradation_note`

### 6.5 iOS
- **Forbidden:** `popen`, `system`
- **Use instead:**
  - Network: `getifaddrs()` with `AF_LINK`
  - WiFi: `CNCopyCurrentNetworkInfo()` (requires `com.apple.developer.networking.wifi-info` entitlement)
  - Cellular: `CTTelephonyNetworkInfo` / `CTCarrier`
  - DNS: `SCDynamicStoreCopyValue()`, `getaddrinfo()`
  - Network state: `NWPathMonitor` (Network framework)
  - Most G2/G3/G4 diagnostics are unavailable due to iOS sandbox;
    return `ND_STATUS_SKIPPED` with a human-readable `degradation_note`

## 7. Memory Management Contract

- `nd_run_diagnostic` allocates results on the heap.
- The **caller** MUST free each result via `nd_free_result`.
- The `NativeService` C++ wrapper (in `src/app/NativeService.cpp`) handles
  string copying into `QString` and calls `nd_free_result` automatically.

## 8. Thread Safety

- Plugin initialisation (`nd_init` / `nd_shutdown`) is NOT thread-safe;
  call once at startup.
- `nd_run_diagnostic` and `nd_check_permissions` ARE thread-safe;
  the factory uses internal locking (`std::mutex` around instance creation).
- Concurrent calls to `nd_run_diagnostic` for DIFFERENT diagnostic IDs
  are safe.  Same-ID concurrent calls share the singleton instance and
  must be internally synchronised by the diagnostic implementation.

## 9. Versioning

The version string returned by `nd_version()` follows SemVer.
Callers should check `nd_version()` at startup and may refuse to run if the
major version differs from expected.

## 10. Testing

- Each platform implementation must have a `registerCustom()` factory entry
  point for injecting mock instances.
- Unit tests build the library and call each diagnostic directly via the C ABI,
  verifying no crash and sensible output shapes.
