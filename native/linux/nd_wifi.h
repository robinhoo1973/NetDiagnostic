// =============================================================================
// nd_wifi.h — WiFi Diagnostics (G1.3)
// =============================================================================

#ifndef ND_WIFI_H
#define ND_WIFI_H

#include "../common/nd_diagnostic_base.h"
#include "../common/nd_platform_bases.h"

class NdWifiDiagLinux    : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "WiFi Diagnostics"; } const NdPermissionInfo* requiredPermissions(int*) const override; bool supportsDegradedMode() const override { return true; } };
class NdWifiDiagMacOS    : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "WiFi Diagnostics"; } const NdPermissionInfo* requiredPermissions(int*) const override; bool supportsDegradedMode() const override { return true; } };
class NdWifiDiagWindows  : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "WiFi Diagnostics"; } };
class NdWifiDiagIOS      : public NdDiagnosticBase, public NdIosBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_IOS; } const char* displayName() const override { return "WiFi Diagnostics"; } const NdPermissionInfo* requiredPermissions(int*) const override; bool supportsDegradedMode() const override { return true; } };
class NdWifiDiagAndroid  : public NdDiagnosticBase, public NdAndroidBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_ANDROID; } const char* displayName() const override { return "WiFi Diagnostics"; } const NdPermissionInfo* requiredPermissions(int*) const override; bool supportsDegradedMode() const override { return true; } };

#endif
