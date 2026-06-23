// =============================================================================
// NativeService.cpp — stub: native plugin removed, all tests use C++ fallbacks
// =============================================================================
#include "app/NativeService.h"
#include "util/Logger.h"

NativeService& NativeService::instance() {
    static NativeService inst;
    return inst;
}

NativeService::~NativeService() { }

bool NativeService::initialize() {
    Logger::instance().event(QStringLiteral("Native plugin disabled — using C++ implementations"));
    return false;
}

void NativeService::shutdown() { m_available = false; }

bool NativeService::isNativeCapable(DiagId) const {
    return false; // Never route to native — always use C++ fallback
}

std::optional<DiagnosticResult> NativeService::runDiagnostic(DiagId, const QString&,
                                                             int, int) {
    return std::nullopt; // Always fall through to C++ implementation
}

DiagnosticResult NativeService::mapResult(const NdDiagnosticResult*) const {
    return DiagnosticResult{}; // Never called
}
