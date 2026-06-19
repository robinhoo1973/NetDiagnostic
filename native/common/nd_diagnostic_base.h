// =============================================================================
// nd_diagnostic_base.h — Abstract base class for all diagnostics
//
// Every platform-specific diagnostic inherits from this (possibly through
// one of the platform base classes defined in nd_platform_bases.h).
// =============================================================================

#ifndef ND_DIAGNOSTIC_BASE_H
#define ND_DIAGNOSTIC_BASE_H

#include "../nd_types.h"
#include <cstdint>
#include <cstddef>

class NdDiagnosticBase {
public:
    virtual ~NdDiagnosticBase() = default;

    // ---- mandatory overrides -----------------------------------------------

    /// Execute the diagnostic and return a heap-allocated result.
    /// The caller owns the result and must call releaseResult().
    virtual NdDiagnosticResult* execute(const NdDiagnosticInput* input) = 0;

    /// Bitmask of platforms on which this diagnostic works.
    /// Return ND_PLATFORM_* OR-ed together.
    virtual uint32_t supportedPlatforms() const = 0;

    /// Human-readable name for UI display.
    virtual const char* displayName() const = 0;

    // ---- optional overrides ------------------------------------------------

    /// Permissions required for full functionality.
    /// Default: none required.
    virtual const NdPermissionInfo* requiredPermissions(int* outCount) const {
        *outCount = 0;
        return nullptr;
    }

    /// Whether this diagnostic can produce useful results even without
    /// the listed permissions (degraded mode).
    virtual bool supportsDegradedMode() const { return false; }

    /// Default timeout in milliseconds.
    virtual int32_t defaultTimeoutMs() const { return 30000; }

    // ---- utility methods (shared by all subclasses) ------------------------
    /// Allocate and populate a NdDiagnosticResult.
    static NdDiagnosticResult* makeResult(
        NdDiagnosticId id,
        int32_t        errorCode,
        NdStatus       status,
        const char*    summary,
        const char*    details,
        int64_t        durationUs,
        NdProperty*    properties,
        int32_t        propCount,
        const char*    rawOutput,
        const char*    errorMsg = nullptr,
        int32_t        isDegraded = 0,
        const char*    degradationNote = nullptr);

    /// Append a property to a dynamically-growing array.
    static void addProperty(
        NdProperty*&   props,
        int32_t&       count,
        const char*    label,
        const char*    value);

    /// Convenience: mark result as degraded (permission-limited).
    static void markDegraded(
        NdDiagnosticResult* result,
        const char* note);

    /// Safe append to a fixed-size buffer: writes to buf+*pos, clips if full.
    /// Returns number of bytes written (excluding null terminator), 0 if clipped.
    static int safeAppend(char* buf, int bufSize, int* pos, const char* fmt, ...);

    // ---- normalisation helpers (cross-platform consistency) ----------------

    /// Convert any MAC address format to lowercase colon-separated.
    static void normaliseMac(const char* raw, char* out, int outSize);

    /// Convert speed strings ("1 Gbps", "100 Mbps") to integer Mbps.
    static int normaliseSpeedMbps(const char* speedStr);

    /// Convert dBm to percentage (0-100).
    /// Uses common approximation: quality = 2*(dBm + 100), clamped.
    static int dBmToPercent(int dBm);

    /// Convert percentage (0-100) to approximate dBm.
    static int percentToDBm(int percent);
};

// =============================================================================
// Convenience: "none" implementation for unsupported platforms
// =============================================================================

class NdDiagnosticNotSupported : public NdDiagnosticBase {
public:
    explicit NdDiagnosticNotSupported(NdDiagnosticId id)
        : m_id(id) {}

    NdDiagnosticResult* execute(const NdDiagnosticInput*) override {
        return makeResult(m_id, ND_ERR_NOT_SUPPORTED, ND_STATUS_SKIPPED,
            "Not supported on this platform",
            "This diagnostic is not available on the current OS.",
            0, nullptr, 0, nullptr);
    }

    uint32_t supportedPlatforms() const override { return 0; }
    const char* displayName() const override { return "Unsupported"; }

private:
    NdDiagnosticId m_id;
};

#endif // ND_DIAGNOSTIC_BASE_H
