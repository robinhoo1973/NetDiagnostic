#include <cstdio>
#include <chrono>
// =============================================================================
// nd_plugin.cpp — Public C ABI entry points
// =============================================================================

#include "../nd_plugin.h"
#include "nd_factory.h"
#include <cstring>
#include <cstdlib>

extern "C" {

int nd_init(void) {
    NdDiagnosticFactory::ensureInitialized();
    return 0;
}

void nd_shutdown(void) {
    NdDiagnosticFactory::destroyAll();
}

NdDiagnosticResult* nd_run_diagnostic(const NdDiagnosticInput* input) {
    if (!input) return nullptr;
    auto* diag = NdDiagnosticFactory::create(input->diag_id);
    if (!diag) return nullptr;
    return diag->execute(input);
}

int nd_check_permissions(NdDiagnosticId id,
                          NdPermissionInfo* out, int maxCount, int* outCount) {
    auto* diag = NdDiagnosticFactory::create(id);
    if (!diag) {
        *outCount = 0;
        return -1;
    }
    int count = 0;
    const NdPermissionInfo* perms = diag->requiredPermissions(&count);
    int copy = (count < maxCount) ? count : maxCount;
    for (int i = 0; i < copy; i++) {
        out[i] = perms[i];
    }
    *outCount = copy;
    return 0;
}

void nd_free_result(NdDiagnosticResult* result) {
    if (!result) return;
    // Free strdup'd strings inside each property
    for (int i = 0; i < result->property_count; i++) {
        free((void*)result->properties[i].label);
        free((void*)result->properties[i].value);
    }
    // Free strdup'd strings inside each permission
    for (int i = 0; i < result->perm_count; i++) {
        free((void*)result->permissions[i].id);
        free((void*)result->permissions[i].friendly_name);
        free((void*)result->permissions[i].fix_guide);
    }
    free(result->properties);
    free(result->permissions);
    // Free top-level strdup'd strings
    free((void*)result->summary);
    free((void*)result->details);
    free((void*)result->raw_output);
    free((void*)result->error_msg);
    free((void*)result->degradation_note);
    delete result;
}

const char* nd_version(void) {
    static char ver[16]; snprintf(ver,sizeof(ver),"%d.%d.%d",ND_VERSION_MAJOR,ND_VERSION_MINOR,ND_VERSION_PATCH); return ver;
}

} // extern "C"
