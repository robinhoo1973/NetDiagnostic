// =============================================================================
// G4RemoteHost.h — DNS resolution, ping, traceroute, pathping, MTU discovery
// =============================================================================
#pragma once

#include <QString>
#include "models/DiagnosticResult.h"
#include "models/TestId.h"

class PlatformCommand;

namespace G4RemoteHost {

DiagnosticResult dnsResolution(const QString& target, PlatformCommand* cmd);
DiagnosticResult ping(const QString& target, PlatformCommand* cmd);
DiagnosticResult traceroute(const QString& target, PlatformCommand* cmd);
DiagnosticResult pathPing(const QString& target, PlatformCommand* cmd);
DiagnosticResult mtuDiscovery(const QString& target, PlatformCommand* cmd);

} // namespace G4RemoteHost
