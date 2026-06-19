// =============================================================================
// nd_nic_advanced.h — NIC Advanced Properties (G1.2)
// =============================================================================

#ifndef ND_NIC_ADVANCED_H
#define ND_NIC_ADVANCED_H

#include "../common/nd_diagnostic_base.h"
#include "../common/nd_platform_bases.h"

class NdNicAdvancedDiagLinux    : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "NIC Advanced"; } };
class NdNicAdvancedDiagMacOS    : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "NIC Advanced"; } };
class NdNicAdvancedDiagWindows  : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "NIC Advanced"; } };
class NdNicAdvancedDiagIOS      : public NdDiagnosticBase, public NdIosBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_IOS; } const char* displayName() const override { return "NIC Advanced"; } };
class NdNicAdvancedDiagAndroid  : public NdDiagnosticBase, public NdAndroidBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_ANDROID; } const char* displayName() const override { return "NIC Advanced"; } };

#endif
