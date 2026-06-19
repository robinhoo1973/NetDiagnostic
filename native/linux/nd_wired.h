// =============================================================================
// nd_wired.h — Wired/Ethernet Diagnostics (G1.4)
// =============================================================================

#ifndef ND_WIRED_H
#define ND_WIRED_H

#include "../common/nd_diagnostic_base.h"
#include "../common/nd_platform_bases.h"

class NdWiredDiagLinux    : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Wired Diagnostics"; } };
class NdWiredDiagMacOS    : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "Wired Diagnostics"; } };
class NdWiredDiagWindows  : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "Wired Diagnostics"; } };
class NdWiredDiagIOS      : public NdDiagnosticBase, public NdIosBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_IOS; } const char* displayName() const override { return "Wired Diagnostics"; } };
class NdWiredDiagAndroid  : public NdDiagnosticBase, public NdAndroidBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_ANDROID; } const char* displayName() const override { return "Wired Diagnostics"; } };

#endif
