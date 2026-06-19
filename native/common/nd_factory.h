// =============================================================================
// nd_factory.h — Diagnostic factory
//
// Returns the correct platform-specific diagnostic instance for the
// current compilation target.  Uses preprocessor conditionals so the
// resulting binary contains only the target platform's code.
// =============================================================================

#ifndef ND_FACTORY_H
#define ND_FACTORY_H

#include "nd_diagnostic_base.h"
#include <cstddef>
#include <mutex>

class NdDiagnosticFactory {
public:
    /// Create the diagnostic for the given id on the current platform.
    /// Returns nullptr if the id is unknown.
    /// Caller does NOT own the pointer (singleton instances).
    static NdDiagnosticBase* create(NdDiagnosticId id);

    /// Release all singleton instances (call at shutdown).
    static void destroyAll();

    /// Register a custom diagnostic (for testing / mocking).
    static void registerCustom(NdDiagnosticId id, NdDiagnosticBase* instance);

public: // (factory methods)
    NdDiagnosticFactory() = delete;

    static NdDiagnosticBase* s_instances[ND_DIAG_COUNT];
    static std::mutex        s_mutex;  // protects s_instances[] and s_initialized
    static bool              s_initialized;
    static void ensureInitialized();
};

#endif // ND_FACTORY_H
