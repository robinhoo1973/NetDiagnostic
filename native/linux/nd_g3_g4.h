#ifndef ND_G3_G4_H
#define ND_G3_G4_H
#include "../common/nd_diagnostic_base.h"
#include "../common/nd_platform_bases.h"

// G1 remaining: DHCP, IP Config, Active Connections
class NdDhcpDiagLinux     : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "DHCP Status"; } };
class NdIpConfigDiagLinux : public NdDiagnosticBase, public NdLinuxBase   { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "IP Configuration"; } };
class NdConnectionsDiagLinux : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Active Connections"; } };

// G3
class NdNetskopeDiagLinux      : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Netskope Status"; } };
class NdDnsServersDiagLinux    : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "DNS Servers"; } };
class NdDnsCacheDiagLinux      : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "DNS Cache"; } };
class NdDnsPollutionDiagLinux  : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "DNS Pollution"; } };
class NdInternetConnDiagLinux  : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Internet Connectivity"; } };
class NdSpeedTestDiagLinux     : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Internet Speed Test"; } };

// G4 (all pure native APIs — no popen/CLI)
class NdDnsResolveDiagLinux    : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "DNS Resolution"; } };
class NdPingDiagLinux          : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Ping (TCP)"; } };
class NdTracerouteDiagLinux    : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Traceroute (UDP)"; } };
class NdMtuDiagLinux           : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "MTU Discovery"; } };
class NdPortScanDiagLinux      : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return ND_PLATFORM_LINUX; } const char* displayName() const override { return "Port Scan"; } };
class NdCellularDiagLinux      : public NdDiagnosticBase, public NdLinuxBase { public: NdDiagnosticResult* execute(const NdDiagnosticInput*) override; uint32_t supportedPlatforms() const override { return 0; } const char* displayName() const override { return "Cellular Info"; } };

#endif
