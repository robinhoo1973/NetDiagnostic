// =============================================================================
// nd_network_adapters.h — Network Adapters diagnostic (all platforms)
// =============================================================================

#ifndef ND_NETWORK_ADAPTERS_H
#define ND_NETWORK_ADAPTERS_H

#include "../common/nd_diagnostic_base.h"
#include "../common/nd_platform_bases.h"

// =============================================================================
// Linux
// =============================================================================
class NdNetworkAdapterDiagnosticLinux : public NdDiagnosticBase,
                                         public NdLinuxBase {
public:
    NdDiagnosticResult* execute(const NdDiagnosticInput* input) override;
    uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; }
    const char* displayName() const override { return "Network Adapters"; }
};

// =============================================================================
// macOS
// =============================================================================
class NdNetworkAdapterDiagnosticMacOS : public NdDiagnosticBase,
                                         public NdBsdBase {
public:
    NdDiagnosticResult* execute(const NdDiagnosticInput* input) override;
    uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; }
    const char* displayName() const override { return "Network Adapters"; }
};

// =============================================================================
// Windows
// =============================================================================
class NdNetworkAdapterDiagnosticWindows : public NdDiagnosticBase,
                                           public NdWindowsBase {
public:
    NdDiagnosticResult* execute(const NdDiagnosticInput* input) override;
    uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; }
    const char* displayName() const override { return "Network Adapters"; }
};

// =============================================================================
// iOS
// =============================================================================
class NdNetworkAdapterDiagnosticIOS : public NdDiagnosticBase,
                                       public NdIosBase {
public:
    NdDiagnosticResult* execute(const NdDiagnosticInput* input) override;
    uint32_t supportedPlatforms() const override { return ND_PLATFORM_IOS; }
    const char* displayName() const override { return "Network Adapters"; }
};

// =============================================================================
// Android
// =============================================================================
class NdNetworkAdapterDiagnosticAndroid : public NdDiagnosticBase,
                                           public NdAndroidBase {
public:
    NdDiagnosticResult* execute(const NdDiagnosticInput* input) override;
    uint32_t supportedPlatforms() const override { return ND_PLATFORM_ANDROID; }
    const char* displayName() const override { return "Network Adapters"; }
};

#endif // ND_NETWORK_ADAPTERS_H
