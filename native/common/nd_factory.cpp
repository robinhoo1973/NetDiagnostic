// =============================================================================
// nd_factory.cpp — Cross-platform diagnostic factory
// =============================================================================
#include "nd_factory.h"
#include "nd_diagnostic_base.h"

#if defined(__linux__) && !defined(__ANDROID__)
#  include "../linux/nd_network_adapters.h"
#  include "../linux/nd_nic_advanced.h"
#  include "../linux/nd_wifi.h"
#  include "../linux/nd_wired.h"
#  include "../linux/nd_g2.h"
#  include "../linux/nd_g3_g4.h"
#elif defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IOS
#  include "../linux/nd_network_adapters.h"
#  include "../linux/nd_nic_advanced.h"
#  include "../linux/nd_wifi.h"
#  include "../linux/nd_wired.h"
#  include "../linux/nd_g2.h"
#  include "../linux/nd_g3_g4.h"
#elif defined(_WIN32)
#  include "../linux/nd_network_adapters.h"
#  include "../linux/nd_nic_advanced.h"
#  include "../linux/nd_wifi.h"
#  include "../linux/nd_wired.h"
#  include "../linux/nd_g2.h"
#  include "../linux/nd_g3_g4.h"
#elif defined(__APPLE__) && TARGET_OS_IOS
#  include "../linux/nd_network_adapters.h"
#  include "../linux/nd_nic_advanced.h"
#  include "../linux/nd_wifi.h"
#  include "../linux/nd_wired.h"
#  include "../linux/nd_g3_g4.h"
#elif defined(__ANDROID__)
#  include "../linux/nd_network_adapters.h"
#  include "../linux/nd_wifi.h"
#  include "../linux/nd_wired.h"
#  include "../linux/nd_g2.h"
#  include "../linux/nd_g3_g4.h"
#endif

NdDiagnosticBase* NdDiagnosticFactory::s_instances[ND_DIAG_COUNT] = {};
std::mutex        NdDiagnosticFactory::s_mutex;
bool              NdDiagnosticFactory::s_initialized = false;

// Helper macro: pick the right class for this platform
#if defined(__linux__) && !defined(__ANDROID__)
#  define MAKE_ADAPTERS   new NdNetworkAdapterDiagnosticLinux()
#  define MAKE_NIC_ADV    new NdDiagnosticNotSupported(ND_DIAG_NIC_ADVANCED) /* FIXME: readlink segfault on ARM64 */
#  define MAKE_WIFI       new NdWifiDiagLinux()
#  define MAKE_WIRED      new NdWiredDiagLinux()
#  define MAKE_PROFILE    new NdNetProfileDiagLinux()
#  define MAKE_TCP        new NdTcpDiagLinux()
#  define MAKE_GW         new NdGatewayDiagLinux()
#  define MAKE_ROUTING    new NdRoutingDiagLinux()
#  define MAKE_ARP        new NdArpDiagLinux()
#  define MAKE_PROXY      new NdProxyDiagLinux()
#  define MAKE_DHCP       new NdDhcpDiagLinux()
#  define MAKE_IPCFG      new NdIpConfigDiagLinux()
#  define MAKE_CONN       new NdConnectionsDiagLinux()
#  define MAKE_NS         new NdNetskopeDiagLinux()
#  define MAKE_DNS_SRV    new NdDnsServersDiagLinux()
#  define MAKE_DNS_CACHE  new NdDiagnosticNotSupported(ND_DIAG_DNS_CACHE) /* SIGBUS on ARM64 */
#  define MAKE_DNS_POLL   new NdDnsPollutionDiagLinux()
#  define MAKE_INET       new NdInternetConnDiagLinux()
#  define MAKE_SPEED      new NdSpeedTestDiagLinux()
#  define MAKE_DNS_RES    new NdDnsResolveDiagLinux()
#  define MAKE_PING       new NdPingDiagLinux()
#  define MAKE_TRACE      new NdTracerouteDiagLinux()
#  define MAKE_MTU        new NdMtuDiagLinux()
#  define MAKE_PORT       new NdPortScanDiagLinux()
#  define MAKE_CELL       new NdCellularDiagLinux()

#elif defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IOS
// macOS platform classes + shared POSIX implementations
#  define MAKE_ADAPTERS   new NdNetworkAdapterDiagnosticMacOS()
#  define MAKE_NIC_ADV    new NdNicAdvancedDiagMacOS()
#  define MAKE_WIFI       new NdWifiDiagMacOS()
#  define MAKE_WIRED      new NdWiredDiagMacOS()
#  define MAKE_PROFILE    new NdNetProfileDiagMacOS()
#  define MAKE_TCP        new NdTcpDiagMacOS()
#  define MAKE_GW         new NdGatewayDiagMacOS()
#  define MAKE_ROUTING    new NdRoutingDiagMacOS()
#  define MAKE_ARP        new NdArpDiagMacOS()
#  define MAKE_PROXY      new NdProxyDiagMacOS()
#  define MAKE_DHCP       new NdDhcpDiagLinux()  // shared posix impl
#  define MAKE_IPCFG      new NdIpConfigDiagLinux()
#  define MAKE_CONN       new NdConnectionsDiagLinux()
#  define MAKE_NS         new NdNetskopeDiagLinux()
#  define MAKE_DNS_SRV    new NdDnsServersDiagLinux()
#  define MAKE_DNS_CACHE  new NdDiagnosticNotSupported(ND_DIAG_DNS_CACHE) /* SIGBUS on ARM64 */
#  define MAKE_DNS_POLL   new NdDnsPollutionDiagLinux()
#  define MAKE_INET       new NdInternetConnDiagLinux()
#  define MAKE_SPEED      new NdSpeedTestDiagLinux()
#  define MAKE_DNS_RES    new NdDnsResolveDiagLinux()
#  define MAKE_PING       new NdPingDiagLinux()
#  define MAKE_TRACE      new NdTracerouteDiagLinux()
#  define MAKE_MTU        new NdMtuDiagLinux()
#  define MAKE_PORT       new NdPortScanDiagLinux()
#  define MAKE_CELL       new NdCellularDiagLinux()

#elif defined(_WIN32)
#  define MAKE_ADAPTERS   new NdNetworkAdapterDiagnosticWindows()
#  define MAKE_NIC_ADV    new NdNicAdvancedDiagWindows()
#  define MAKE_WIFI       new NdWifiDiagWindows()
#  define MAKE_WIRED      new NdWiredDiagWindows()
#  define MAKE_PROFILE    new NdNetProfileDiagWindows()
#  define MAKE_TCP        new NdTcpDiagWindows()
#  define MAKE_GW         new NdGatewayDiagWindows()
#  define MAKE_ROUTING    new NdRoutingDiagWindows()
#  define MAKE_ARP        new NdArpDiagWindows()
#  define MAKE_PROXY      new NdProxyDiagWindows()
#  define MAKE_DHCP       new NdDhcpDiagLinux()
#  define MAKE_IPCFG      new NdIpConfigDiagLinux()
#  define MAKE_CONN       new NdConnectionsDiagLinux()
#  define MAKE_NS         new NdNetskopeDiagLinux()
#  define MAKE_DNS_SRV    new NdDnsServersDiagLinux()
#  define MAKE_DNS_CACHE  new NdDiagnosticNotSupported(ND_DIAG_DNS_CACHE) /* SIGBUS on ARM64 */
#  define MAKE_DNS_POLL   new NdDnsPollutionDiagLinux()
#  define MAKE_INET       new NdInternetConnDiagLinux()
#  define MAKE_SPEED      new NdSpeedTestDiagLinux()
#  define MAKE_DNS_RES    new NdDnsResolveDiagLinux()
#  define MAKE_PING       new NdPingDiagLinux()
#  define MAKE_TRACE      new NdTracerouteDiagLinux()
#  define MAKE_MTU        new NdMtuDiagLinux()
#  define MAKE_PORT       new NdPortScanDiagLinux()
#  define MAKE_CELL       new NdCellularDiagLinux()

#elif defined(__APPLE__) && TARGET_OS_IOS
#  define MAKE_ADAPTERS   new NdNetworkAdapterDiagnosticIOS()
#  define MAKE_NIC_ADV    new NdNicAdvancedDiagIOS()
#  define MAKE_WIFI       new NdWifiDiagIOS()
#  define MAKE_WIRED      new NdWiredDiagIOS()
#  define MAKE_DHCP       new NdDhcpDiagLinux()
#  define MAKE_IPCFG      new NdIpConfigDiagLinux()
#  define MAKE_CONN       new NdConnectionsDiagLinux()
#  define MAKE_DNS_SRV    new NdDnsServersDiagLinux()
#  define MAKE_CELL       new NdCellularDiagnosticIOS()
// iOS skips: Profile/TCP/GW/Routing/ARP/Proxy/NS/DNS-Cache/Inet/DNS-Res/Ping/Trace/MTU/Port
#  define MAKE_PROFILE    new NdDiagnosticNotSupported(ND_DIAG_NETWORK_PROFILE)
#  define MAKE_TCP        new NdDiagnosticNotSupported(ND_DIAG_TCP_SETTINGS)
#  define MAKE_GW         new NdDiagnosticNotSupported(ND_DIAG_DEFAULT_GATEWAY)
#  define MAKE_ROUTING    new NdDiagnosticNotSupported(ND_DIAG_ROUTING_TABLE)
#  define MAKE_ARP        new NdDiagnosticNotSupported(ND_DIAG_ARP_TABLE)
#  define MAKE_PROXY      new NdDiagnosticNotSupported(ND_DIAG_PROXY_SETTINGS)
#  define MAKE_NS         new NdDiagnosticNotSupported(ND_DIAG_NETSKOPE_STATUS)
#  define MAKE_DNS_CACHE  new NdDiagnosticNotSupported(ND_DIAG_DNS_CACHE)
#  define MAKE_DNS_POLL   new NdDiagnosticNotSupported(ND_DIAG_DNS_POLLUTION)
#  define MAKE_INET       new NdDiagnosticNotSupported(ND_DIAG_INTERNET_CONNECTIVITY)
#  define MAKE_SPEED      new NdDiagnosticNotSupported(ND_DIAG_SPEED_TEST)
#  define MAKE_DNS_RES    new NdDiagnosticNotSupported(ND_DIAG_DNS_RESOLUTION)
#  define MAKE_PING       new NdDiagnosticNotSupported(ND_DIAG_PING)
#  define MAKE_TRACE      new NdDiagnosticNotSupported(ND_DIAG_TRACEROUTE)
#  define MAKE_MTU        new NdDiagnosticNotSupported(ND_DIAG_MTU_DISCOVERY)
#  define MAKE_PORT       new NdDiagnosticNotSupported(ND_DIAG_PORT_SCAN)

#elif defined(__ANDROID__)
#  define MAKE_ADAPTERS   new NdNetworkAdapterDiagnosticAndroid()
#  define MAKE_WIFI       new NdWifiDiagAndroid()
#  define MAKE_DHCP       new NdDhcpDiagLinux()
#  define MAKE_IPCFG      new NdIpConfigDiagLinux()
#  define MAKE_CONN       new NdConnectionsDiagLinux()
#  define MAKE_ROUTING    new NdRoutingDiagLinux()
#  define MAKE_ARP        new NdArpDiagLinux()
#  define MAKE_DNS_SRV    new NdDnsServersDiagLinux()
#  define MAKE_DNS_POLL   new NdDnsPollutionDiagLinux()
#  define MAKE_INET       new NdInternetConnDiagLinux()
#  define MAKE_SPEED      new NdSpeedTestDiagLinux()
#  define MAKE_DNS_RES    new NdDnsResolveDiagLinux()
#  define MAKE_CELL       new NdCellularDiagnosticAndroid()
#  define MAKE_NIC_ADV    new NdNicAdvancedDiagAndroid()
#  define MAKE_WIRED      new NdWiredDiagAndroid()
#  define MAKE_PROFILE    new NdNetProfileDiagLinux()
#  define MAKE_TCP        new NdTcpDiagLinux()
#  define MAKE_GW         new NdGatewayDiagLinux()
#  define MAKE_PROXY      new NdProxyDiagLinux()
#  define MAKE_NS         new NdNetskopeDiagLinux()
#  define MAKE_DNS_CACHE  new NdDiagnosticNotSupported(ND_DIAG_DNS_CACHE) /* SIGBUS on ARM64 */
#  define MAKE_PING       new NdPingDiagLinux()
#  define MAKE_TRACE      new NdTracerouteDiagLinux()
#  define MAKE_MTU        new NdMtuDiagLinux()
#  define MAKE_PORT       new NdPortScanDiagLinux()
#endif

void NdDiagnosticFactory::ensureInitialized() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) return;
    s_instances[ND_DIAG_NETWORK_ADAPTERS]   = MAKE_ADAPTERS;
    s_instances[ND_DIAG_NIC_ADVANCED]       = MAKE_NIC_ADV;
    s_instances[ND_DIAG_WIFI]               = MAKE_WIFI;
    s_instances[ND_DIAG_WIRED]              = MAKE_WIRED;
    s_instances[ND_DIAG_NETWORK_PROFILE]    = MAKE_PROFILE;
    s_instances[ND_DIAG_TCP_SETTINGS]       = MAKE_TCP;
    s_instances[ND_DIAG_DEFAULT_GATEWAY]    = MAKE_GW;
    s_instances[ND_DIAG_ROUTING_TABLE]      = MAKE_ROUTING;
    s_instances[ND_DIAG_ARP_TABLE]          = MAKE_ARP;
    s_instances[ND_DIAG_PROXY_SETTINGS]     = MAKE_PROXY;
    s_instances[ND_DIAG_DHCP_STATUS]        = MAKE_DHCP;
    s_instances[ND_DIAG_IP_CONFIG]          = MAKE_IPCFG;
    s_instances[ND_DIAG_ACTIVE_CONNECTIONS] = MAKE_CONN;
    s_instances[ND_DIAG_NETSKOPE_STATUS]       = MAKE_NS;
    s_instances[ND_DIAG_DNS_SERVERS]           = MAKE_DNS_SRV;
    s_instances[ND_DIAG_DNS_CACHE]             = MAKE_DNS_CACHE;
    s_instances[ND_DIAG_DNS_POLLUTION]         = MAKE_DNS_POLL;
    s_instances[ND_DIAG_INTERNET_CONNECTIVITY] = MAKE_INET;
    s_instances[ND_DIAG_SPEED_TEST]            = MAKE_SPEED;
    s_instances[ND_DIAG_DNS_RESOLUTION]  = MAKE_DNS_RES;
    s_instances[ND_DIAG_PING]            = MAKE_PING;
    s_instances[ND_DIAG_TRACEROUTE]      = MAKE_TRACE;
    s_instances[ND_DIAG_MTU_DISCOVERY]   = MAKE_MTU;
    s_instances[ND_DIAG_PORT_SCAN]       = MAKE_PORT;
    s_instances[ND_DIAG_CELLULAR_INFO]   = MAKE_CELL;
    // G5 cross-protocol — use Qt C++ implementation
    for (int i = ND_DIAG_TCP_CONNECT; i < ND_DIAG_COUNT; i++)
        if (!s_instances[i]) s_instances[i] = new NdDiagnosticNotSupported((NdDiagnosticId)i);
    s_initialized = true;
}

NdDiagnosticBase* NdDiagnosticFactory::create(NdDiagnosticId id) {
    if (id >= ND_DIAG_COUNT) return nullptr;
    ensureInitialized();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_instances[id]) return s_instances[id];
        // Allocate and track in the registry so destroyAll() can clean it up
        auto* fallback = new NdDiagnosticNotSupported(id);
        s_instances[id] = fallback;
        return fallback;
    }
}

void NdDiagnosticFactory::destroyAll() {
    std::lock_guard<std::mutex> lock(s_mutex);
    for (int i = 0; i < ND_DIAG_COUNT; i++) { delete s_instances[i]; s_instances[i] = nullptr; }
    s_initialized = false;
}
void NdDiagnosticFactory::registerCustom(NdDiagnosticId id, NdDiagnosticBase* i) {
    if (id >= ND_DIAG_COUNT) return;
    ensureInitialized();
    std::lock_guard<std::mutex> lock(s_mutex);
    delete s_instances[id];
    s_instances[id] = i;
}
