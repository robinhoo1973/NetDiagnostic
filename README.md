# NetAnalysis

Cross-platform network diagnostic tool. Built with Qt 6 / QML and libcurl.

## Features

### Diagnostic Groups

| Group | Name | Description |
|-------|------|-------------|
| G1 | System & Adapters | Network adapters, IP configuration, WiFi, DHCP, active connections |
| G2 | Connectivity & Security | Routing table, ARP table, proxy settings, TCP settings |
| G3 | Internet & DNS | DNS servers, DNS cache, DNS pollution check, Internet speed test |
| G4 | Remote Host | DNS resolution, ping, traceroute, pathPing, MTU discovery, port scan |
| G5 | Website / URL | HTTP headers, curl verbose, SSL certificate, redirect, compression, timing |

### Key Features

- **Pure C++ diagnostics** — zero shell commands, direct OS API calls (Linux `/proc`, Windows Win32 API)
- **libcurl integration** — full HTTP/HTTPS support with curl-compatible verbose output
- **DNS resolution** — dig-style output with HEADER/QUESTION/ANSWER/AUTHORITY/ADDITIONAL sections
- **Speed test** — Ookla-compatible download/upload bandwidth measurement
- **Port scanner** — concurrent non-blocking socket scan with range merging
- **7-language UI** — English, French, German, Russian, Italian, Simplified/Traditional Chinese
- **Monospace detail output** — DejaVu Sans Mono for aligned diagnostic tables
- **Badge-style status icons** — colored SVG badges for Pass/Warning/Fail/Skip/Info
- **Single-instance lock** — prevents duplicate application instances

## Build

### Linux (arm64 / x86_64)

```bash
# Install dependencies
sudo apt install qt6-base-dev qt6-quickcontrols2-dev libcurl4-openssl-dev cmake ninja-build

# Build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
ninja -C build net_diagnostic

# Output: build/net_diagnostic
```

### Windows (MinGW cross-compile)

```bash
./scripts/build-all.sh --target windows-x86_64
```

### Simulator

```bash
cmake -G Ninja -DBUILD_SIMULATOR=ON -B build -S .
ninja -C build net_diagnostic_sim
```

## Supported Platforms

| Platform | Status |
|----------|--------|
| Linux (arm64) | ✅ Full support |
| Linux (x86_64) | ✅ Full support |
| Windows (MinGW) | ✅ Full support |
| macOS | ⚠️ Partial (G1/G2/G3 limited, no traceroute) |
| iOS | ⚠️ Compiles, sandbox restrictions |
| Android | ✅ Mostly works |

## License

MIT
