// =============================================================================
// NativeService.h — Direct C++ wrapper around the nd_plugin native diagnostics
//
// Calls nd_init/nd_run_diagnostic/nd_free_result directly from C++.
// =============================================================================
#pragma once

#include <QString>
#include <optional>
#include "models/TestId.h"
#include "models/DiagnosticResult.h"
#include "nd_types.h"

class NativeService {
public:
    static NativeService& instance();

    /// Initialize the native plugin. Returns false on failure.
    bool initialize();

    /// Shut down and free resources.
    void shutdown();

    /// Whether the plugin loaded successfully.
    bool isAvailable() const { return m_available; }

    /// Does the native plugin support this test?
    bool isNativeCapable(TestId id) const;

    /// Run a diagnostic via the native plugin.
    /// Returns nullopt if plugin unavailable or test not supported.
    std::optional<DiagnosticResult> runDiagnostic(TestId id, const QString& target = {},
                                                   int fromPort = 0, int toPort = 0);

    /// Get the native plugin version string.
    QString version() const { return m_version; }

private:
    NativeService() = default;
    ~NativeService();
    NativeService(const NativeService&) = delete;
    NativeService& operator=(const NativeService&) = delete;

    DiagnosticResult mapResult(const NdDiagnosticResult* nr) const;

    bool m_available = false;
    QString m_version;
    int    m_versionMajor = 0;
};
