#ifndef ND_G2_H
#define ND_G2_H
#include "../common/nd_diagnostic_base.h"
#include "../common/nd_platform_bases.h"

// G2.1 Network Profile
class NdNetProfileDiagLinux   : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Network Profile"; } };
class NdNetProfileDiagMacOS   : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "Network Profile"; } };
class NdNetProfileDiagWindows : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "Network Profile"; } };

// G2.2 TCP Settings
class NdTcpDiagLinux   : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "TCP Settings"; } };
class NdTcpDiagMacOS   : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "TCP Settings"; } };
class NdTcpDiagWindows : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "TCP Settings"; } };

// G2.3 Default Gateway
class NdGatewayDiagLinux   : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Default Gateway"; } };
class NdGatewayDiagMacOS   : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "Default Gateway"; } };
class NdGatewayDiagWindows : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "Default Gateway"; } };

// G2.4 Routing Table
class NdRoutingDiagLinux   : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Routing Table"; } };
class NdRoutingDiagMacOS   : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "Routing Table"; } };
class NdRoutingDiagWindows : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "Routing Table"; } };

// G2.5 ARP Table
class NdArpDiagLinux   : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "ARP Table"; } };
class NdArpDiagMacOS   : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "ARP Table"; } };
class NdArpDiagWindows : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "ARP Table"; } };

// G2.6 Proxy Settings
class NdProxyDiagLinux   : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Proxy Settings"; } };
class NdProxyDiagMacOS   : public NdDiagnosticBase, public NdBsdBase     { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_MACOS; } const char* displayName() const override { return "Proxy Settings"; } };
class NdProxyDiagWindows : public NdDiagnosticBase, public NdWindowsBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_WINDOWS; } const char* displayName() const override { return "Proxy Settings"; } };

#endif
