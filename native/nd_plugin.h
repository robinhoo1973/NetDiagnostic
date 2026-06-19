// =============================================================================
// nd_plugin.h — Public C ABI for the NetDiagnostic Native Plugin
//
// Qt C++ calls these functions directly.  Implementation delegates to
// NdDiagnosticFactory to create the appropriate platform diagnostic.
// =============================================================================

#ifndef ND_PLUGIN_H
#define ND_PLUGIN_H

#include "nd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialise the plugin.  Call once at startup.  Returns 0 on success.
int nd_init(void);

/// Shut down and free all resources.
void nd_shutdown(void);

/// Run a single diagnostic.  The returned NdDiagnosticResult is
/// heap-allocated; the caller must free it with nd_free_result().
NdDiagnosticResult* nd_run_diagnostic(const NdDiagnosticInput* input);

/// Check permissions for a specific diagnostic id.
/// Writes up to maxCount entries into out; returns actual count via outCount.
int nd_check_permissions(NdDiagnosticId id,
                         NdPermissionInfo* out, int maxCount, int* outCount);

/// Free a result previously returned by nd_run_diagnostic().
void nd_free_result(NdDiagnosticResult* result);

/// Get the plugin version string (e.g. "1.0.0").
const char* nd_version(void);

#ifdef __cplusplus
}
#endif

#endif // ND_PLUGIN_H
