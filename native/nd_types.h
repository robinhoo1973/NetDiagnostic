// =============================================================================
// nd_types.h — NetDiagnostic Native Plugin: Core Types
//
// Shared by all platforms.  Defines every enum, struct, and constant used
// across the diagnostic plugin boundary.
// =============================================================================

#ifndef ND_TYPES_H
#define ND_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Error codes ------------------------------------------------------------
#define ND_OK               0
#define ND_ERR_TIMEOUT      1
#define ND_ERR_PERMISSION   2
#define ND_ERR_NETWORK      3
#define ND_ERR_NOT_SUPPORTED 4
#define ND_ERR_INTERNAL     99

// --- Diagnostic status -------------------------------------------------------
typedef enum {
    ND_STATUS_PASS    = 0,
    ND_STATUS_WARNING = 1,
    ND_STATUS_FAIL    = 2,
    ND_STATUS_SKIPPED = 3,
    ND_STATUS_ERROR   = 4,
    ND_STATUS_INFO    = 5,
} NdStatus;

// --- Diagnostic identifiers (one per diagnostic) ----------------------------
typedef enum {
    // G1 — System & Adapters
    ND_DIAG_NETWORK_ADAPTERS      = 0,
    ND_DIAG_NIC_ADVANCED          = 1,
    ND_DIAG_WIFI                  = 2,
    ND_DIAG_WIRED                 = 3,
    ND_DIAG_DHCP_STATUS           = 4,
    ND_DIAG_IP_CONFIG             = 5,
    ND_DIAG_ACTIVE_CONNECTIONS    = 6,

    // G2 — Connectivity & Security
    ND_DIAG_NETWORK_PROFILE       = 7,
    ND_DIAG_TCP_SETTINGS          = 8,
    ND_DIAG_DEFAULT_GATEWAY       = 9,
    ND_DIAG_ROUTING_TABLE         = 10,
    ND_DIAG_ARP_TABLE             = 11,
    ND_DIAG_PROXY_SETTINGS        = 12,

    // G3 — Internet & DNS
    ND_DIAG_NETSKOPE_STATUS       = 13,
    ND_DIAG_DNS_SERVERS           = 14,
    ND_DIAG_DNS_CACHE             = 15,
    ND_DIAG_DNS_POLLUTION         = 16,
    ND_DIAG_INTERNET_CONNECTIVITY = 17,
    ND_DIAG_SPEED_TEST            = 18,

    // G4 — Remote Host
    ND_DIAG_DNS_RESOLUTION        = 19,
    ND_DIAG_PING                  = 20,
    ND_DIAG_TRACEROUTE            = 21,
    ND_DIAG_PATH_PING             = 22,
    ND_DIAG_MTU_DISCOVERY         = 23,
    ND_DIAG_PORT_SCAN             = 24,
    // G5 — cross-protocol
    ND_DIAG_TCP_CONNECT           = 25,
    ND_DIAG_SERVICE_BANNER        = 26,
    ND_DIAG_FTP_DIAGNOSTICS       = 27,
    ND_DIAG_SSH_DIAGNOSTICS       = 28,
    ND_DIAG_EMAIL_DIAGNOSTICS     = 29,
    // Mobile only
    ND_DIAG_CELLULAR_INFO         = 30,

    ND_DIAG_COUNT
} NdDiagnosticId;

// --- Platform bitmask (for capability queries) ------------------------------
#define ND_PLATFORM_LINUX    (1 << 0)
#define ND_PLATFORM_MACOS    (1 << 1)
#define ND_PLATFORM_WINDOWS  (1 << 2)
#define ND_PLATFORM_IOS      (1 << 3)
#define ND_PLATFORM_ANDROID  (1 << 4)
#define ND_PLATFORM_ALL      0x1F

// --- Permission state -------------------------------------------------------
typedef enum {
    ND_PERM_GRANTED        = 0,
    ND_PERM_DENIED         = 1,
    ND_PERM_NOT_APPLICABLE = 2,
    ND_PERM_UNKNOWN        = 3,
} NdPermissionState;

// --- Permission info --------------------------------------------------------
typedef struct {
    const char  *id;            // e.g. "wifi.ssid", "cellular.signal"
    NdPermissionState state;
    const char  *friendly_name; // e.g. "WiFi SSID"
    const char  *fix_guide;     // e.g. "Settings > Privacy > WiFi"
} NdPermissionInfo;

// --- Property (key-value pair) ----------------------------------------------
typedef struct {
    const char *label;
    const char *value;
} NdProperty;

// --- Diagnostic input -------------------------------------------------------
typedef struct {
    NdDiagnosticId diag_id;
    const char    *target;       // hostname / IP / URL (nullable)
    int32_t        timeout_ms;   // max wait, 0 = use default
    int32_t        from_port;    // port scan range start
    int32_t        to_port;      // port scan range end
} NdDiagnosticInput;

// --- Diagnostic result -------------------------------------------------------
typedef struct {
    NdDiagnosticId diag_id;
    int32_t        error_code;       // 0 = success
    NdStatus       status;
    const char    *summary;          // one-line human-readable
    const char    *details;          // multi-line verbose output
    int64_t        duration_us;
    int32_t        property_count;
    NdProperty    *properties;       // heap-allocated array
    const char    *raw_output;       // original raw output for reference
    const char    *error_msg;        // non-null only when error_code != 0

    // Permission results (added post-diagnostic)
    int32_t        perm_count;
    NdPermissionInfo *permissions;

    // Degradation info
    int32_t        is_degraded;      // 1 = running in degraded mode
    const char    *degradation_note; // why degraded
} NdDiagnosticResult;

// --- Plugin version ---------------------------------------------------------
#define ND_VERSION_MAJOR 1
#define ND_VERSION_MINOR 0
#define ND_VERSION_PATCH 0

#ifdef __cplusplus
}
#endif

#endif // ND_TYPES_H
