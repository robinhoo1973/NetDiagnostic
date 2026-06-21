// =============================================================================
// NativeService.h — Stub: native plugin removed, all diagnostics use C++ fallbacks
// =============================================================================
#pragma once

#include <QString>
#include <optional>
#include "models/DiagId.h"
#include "models/DiagnosticResult.h"

class NativeService {
public:
    static NativeService& instance();

    /// Always returns false — native plugin is disabled.
    bool initialize();

    void shutdown();

    /// Always returns false — native plugin is disabled.
    bool isAvailable() const { return false; }

    /// Always returns false — never route to native.
    bool isNativeCapable(DiagId id) const;

    /// Always returns nullopt — all tests use C++ fallback.
    std::optional<DiagnosticResult> runDiagnostic(DiagId id, const QString& target = {},
                                                   int fromPort = 0, int toPort = 0);

    QString version() const { return m_version; }

private:
    NativeService() = default;
    ~NativeService();
    NativeService(const NativeService&) = delete;
    NativeService& operator=(const NativeService&) = delete;

    // Incomplete type forward — mapResult is never called (stub)
    struct NdDiagnosticResult;
    DiagnosticResult mapResult(const NdDiagnosticResult* nr) const;

    bool m_available = false;
    QString m_version;
    int    m_versionMajor = 0;
};
