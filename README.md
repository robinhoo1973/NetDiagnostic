# NetAnalysis

Cross-platform network diagnostic tool. Built with Qt 6 / QML and libcurl.

## Features

### Diagnostic Groups (38 tests)

| Group | Name | Tests | Description |
|-------|------|-------|-------------|
| G1 | System & Adapters | 7 | Network adapters, NIC advanced, WiFi, wired, DHCP status, IP configuration, active connections |
| G2 | Connectivity & Security | 6 | Network profile, TCP settings, default gateway, routing table, ARP table, proxy settings |
| G3 | Internet & DNS | 5 | Netskope status, DNS servers, DNS cache, DNS pollution, Internet speed test |
| G4 | Remote Host | 6 | DNS resolution, ping, traceroute, pathPing, MTU discovery, port scan |
| G5 | Website / URL | 13 | URL parsing, TCP connect, service banner, curl verbose, HTTP headers, security headers, SSL certificate, HTTP redirect, HTTP compression, HTTP timing, FTP, SSH, Email diagnostics |

### Key Features

- **Pure C++ diagnostics** — zero shell commands, direct OS API calls (Linux `/proc`/`/sys`, Windows Win32 API)
- **libcurl integration** — full HTTP/HTTPS support with curl-compatible verbose output
- **DNS resolution** — dig-style output with HEADER/QUESTION/ANSWER/AUTHORITY/ADDITIONAL sections
- **Speed test** — Ookla-compatible download/upload bandwidth measurement
- **Port scanner** — concurrent non-blocking socket scan with range merging
- **7-language UI** — English, French, German, Russian, Italian, Simplified/Traditional Chinese
- **Monospace detail output** — JetBrains Mono + DejaVu Sans Mono for aligned diagnostic tables
- **Badge-style status icons** — colored SVG badges for Pass/Warning/Fail/Skip/Error/Info
- **Group-sequential execution** — `std::thread` concurrency with `std::atomic` group tracking
- **Native C++ QDialog** — detail view with Status/Duration/Summary/Properties/Raw Output
- **Single-instance lock** — prevents duplicate application instances
- **Simulator mode** — device-frame UI for testing on desktop (Linux/Windows/macOS)

## Project Structure

```
NetDiagnostic-QT/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── doc/
│   ├── PROJECT_CHANGES.md      # Complete file manifest & change log
├── .gitignore
├── scripts/
│   ├── build-all.sh            # Self-contained multi-platform build system
│   ├── build-static.ps1        # Static build script (Windows)
│   ├── generate-icons.sh       # Icon generation helper
│   └── toolchain/              # CMake toolchain files
│       ├── linux-arm64.cmake
│       ├── linux-x86_64.cmake
│       ├── windows-x86_64.cmake
│       └── windows-arm64.cmake
├── src/
│   ├── main.cpp                # Production entry point
│   ├── main_simulator.cpp      # Simulator entry point
│   ├── app/
│   │   ├── AppState.h/.cpp     # Application state & diagnostic orchestration
│   │   └── NativeService.h/.cpp # Native OS service integration
│   ├── engine/
│   │   ├── PlatformCommand.h/.cpp  # Cross-platform command execution
│   │   ├── diagnostic/
│   │   │   ├── DiagnosticEngine.h/.cpp  # Diagnostic engine core
│   │   │   ├── G1G2G3Native.h/.cpp      # G1/G2/G3 native diagnostics
│   │   │   ├── G4RemoteHost.h/.cpp      # G4 remote host tests
│   │   │   └── G5WebsiteUrl.h/.cpp      # G5 website/URL tests
│   │   └── runner/
│   │       └── NetworkProbe.h/.cpp      # Network probing utilities
│   ├── models/
│   │   ├── DiagnosticResult.h/.cpp  # Immutable result struct
│   │   ├── DiagId.h                 # Test IDs, groups, statuses (38 tests)
│   │   └── ResultProperty.h         # Key-value result properties with severity
│   └── util/
│       ├── DebugSwitch.h            # Trace macros (disabled by default)
│       ├── Logger.h/.cpp            # Logging utility
│       └── PingParser.h/.cpp        # Ping output parser
├── resources/
│   ├── resources.qrc            # Qt resource file
│   ├── netanalysis.desktop      # Linux desktop entry
│   ├── netanalysis.rc           # Windows resource file (icon embed)
│   ├── config/                  # Platform config files
│   │   ├── android.conf, ios.conf, linux.conf, windows.conf
│   ├── fonts/
│   │   ├── JetBrainsMono-Regular.ttf, JetBrainsMono-Bold.ttf
│   │   └── DejaVuSansMono.ttf
│   ├── icons/                   # 40+ SVG/PNG/ICO icons
│   │   ├── app-icon.svg, netanalysis.ico, netanalysis.png
│   │   ├── badge-*.svg (check/close/warning/info/skip/error/circle/refresh)
│   │   ├── dashboard, diagnostics, config, report, settings
│   │   ├── check, circle, close, error, info, warning, skip
│   │   ├── play, stop, refresh, timer, spinner, target, globe
│   │   ├── portscan, wifi, tune, mail
│   │   ├── windows, linux, apple, android
│   │   └── sim-icon-*.svg (beaker/bug/flask/monitor-play/network-lab)
│   └── qml/
│       ├── main.qml             # Main window
│       ├── AppContent.qml       # Content layout
│       ├── theme/
│       │   ├── AppTheme.qml     # Theme constants (C++ injected)
│       │   ├── Translations.qml # 7-language translations
│       │   └── qmldir
│       ├── screens/
│       │   ├── DashboardScreen.qml    # Overview dashboard
│       │   ├── DiagnosticScreen.qml   # Main diagnostic tree view
│       │   ├── ConfigScreen.qml       # Test selection (SwitchListTile)
│       │   ├── ReportScreen.qml       # Report preview (planned)
│       │   ├── SettingsScreen.qml     # Application settings
│       │   └── SimulatorScreen.qml    # Device frame simulator
│       └── widgets/
│           ├── AppIcon.qml            # SVG icon component
│           ├── DiagGroupPanel.qml     # Expandable group panel
│           ├── DiagResultItem.qml     # Individual test result row
│           ├── LiveProgressPanel.qml  # Runtime progress display
│           ├── PortScanConfig.qml     # Port scan configuration
│           ├── SummaryCards.qml       # Pass/Warn/Fail/Skip summary
│           ├── TargetAnalysisPanel.qml # Target analysis section
│           └── TargetInputPanel.qml   # Target host input
├── tests/
│   ├── CMakeLists.txt
│   └── test_engine_quick.cpp     # 19 headless tests
└── doc/
    ├── design-spec.html          # Design specification
    └── design-tokens.json        # Design tokens
```

## Build

### Quick Start (automated, all platforms)

```bash
# Check dependencies only
./scripts/build-all.sh --check-only

# Native build (auto-detect host platform)
./scripts/build-all.sh

# Auto-fix ALL missing deps (installs cross-compilers, Qt6, ninja, cmake)
./scripts/build-all.sh --fix --target all

# Cross-compile specific target + simulator
./scripts/build-all.sh --target windows-x86_64 --sim

# Clean rebuild, skip dep check
./scripts/build-all.sh --target linux-arm64 --clean --no-check
```

### Build System Features

| Feature | Description |
|---------|-------------|
| `--fix` | Auto-installs missing tools from source (ninja, cmake, mingw-w64, LLVM-MinGW, Qt6) |
| `--target all` | Builds linux-arm64, linux-x86_64, windows-x86_64, windows-arm64 |
| `--sim` / `--sim-only` | Also build simulator variant with device-frame UI |
| `--clean` | Remove previous build artifacts |
| Cross-compilation | mingw-w64 (x86_64), LLVM-MinGW (aarch64), linux-gnu (x86_64) |
| Smart TMPDIR | Auto-detects small tmpfs and uses `~/.cache` for Qt6 source builds |

### CI/CD (GitHub Actions)

Automated builds for every push and PR via `.github/workflows/build.yml`:

| Platform | Arch | Compiler | Simulator |
|----------|------|----------|-----------|
| Linux | x86_64 | GCC | ✅ |
| Linux | arm64 | GCC (cross) | — |
| Windows | x86_64 | mingw-w64 | ✅ |
| Windows | arm64 | LLVM-MinGW | — |
| macOS | x86_64 | Apple Clang | ✅ |
| macOS | arm64 | Apple Clang | ✅ |

### Manual Build (single platform)

#### Linux (arm64 / x86_64)

```bash
# Install dependencies
sudo apt install qt6-base-dev qt6-quickcontrols2-dev libcurl4-openssl-dev cmake ninja-build

# Build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
ninja -C build net_diagnostic

# Output: build/net_diagnostic
```

#### Windows (cross-compile from Linux)

```bash
# x86_64 — mingw-w64 (GCC)
./scripts/build-all.sh --target windows-x86_64 --fix

# ARM64 — LLVM-MinGW (Clang)
./scripts/build-all.sh --target windows-arm64 --fix
```

#### macOS

```bash
# Prerequisites: Xcode Command Line Tools, Homebrew
brew install qt@6 cmake ninja curl

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
ninja -C build net_diagnostic
```

### Simulator

```bash
cmake -G Ninja -DBUILD_SIMULATOR=ON -B build -S .
ninja -C build net_diagnostic_sim
```

Simulator mode is limited to **Linux / Windows / macOS** (desktop platforms with full Qt6 QuickControls2 support).

### Headless Test

```bash
ND_MAX_TESTS=2 ND_AUTORUN=1 QT_QPA_PLATFORM=offscreen \
    ./build/net_diagnostic
```

## Supported Platforms

| Platform | Arch | Compiler | Status |
|----------|------|----------|--------|
| Linux | arm64 | GCC | ✅ Full support (native) |
| Linux | x86_64 | GCC | ✅ Full support (cross-compile) |
| Windows | x86_64 | mingw-w64 (GCC) | ✅ Cross-compile via `--fix` |
| Windows | ARM64 | LLVM-MinGW (Clang) | ✅ Cross-compile (Qt6 via vcpkg/MSYS2) |
| macOS | x86_64 / arm64 | Apple Clang | ✅ Full support |
| Android | — | NDK | ⚠️ Mostly works |
| iOS | — | Xcode | ⚠️ Compiles, sandbox restrictions |

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| Qt 6 | ≥ 6.2 | Core, Concurrent, Quick, QuickControls2, Widgets, Network |
| libcurl | ≥ 7.80 | HTTP/HTTPS diagnostics (G5) |
| CMake | ≥ 3.22 | Build system |
| Ninja | any | Build tool (optional, Make works too) |

### Platform-specific

| Platform | Libraries |
|----------|-----------|
| Linux | resolv (DNS), glibc |
| Windows | ws2_32, winhttp, iphlpapi, wlanapi, dnsapi, ole32, shell32 |
| macOS | resolv (DNS), SystemConfiguration, CoreFoundation |
| iOS | libcurl, resolv (DNS), SystemConfiguration |

## Deployment

### TestFlight (iOS)

Automated deployment via `.github/workflows/deploy-testflight.yml`. This workflow:

1. Checks whether the `NetDiagnostic` app exists in App Store Connect — creates it if not
2. Builds the iOS `.app` with Qt 6 + Xcode
3. Code-signs with your distribution certificate
4. Exports a signed `.ipa`
5. Uploads to TestFlight via `altool`

#### Triggers

- **Automatic:** after the `Build & Release` workflow succeeds on `master`
- **Manual:** `workflow_dispatch` from GitHub Actions UI

#### Required GitHub Secrets

| Secret | How to obtain |
|--------|---------------|
| `APPSTORE_CONNECT_ISSUER_ID` | App Store Connect → Users and Access → Integrations → Keys |
| `APPSTORE_CONNECT_KEY_ID` | Same page — displayed alongside Issuer ID |
| `APPSTORE_CONNECT_API_KEY` | Downloaded `.p8` file content (include `-----BEGIN/END PRIVATE KEY-----`) |
| `IOS_TEAM_ID` | [developer.apple.com/account](https://developer.apple.com/account) → Membership |
| `IOS_DISTRIBUTION_CERT_BASE64` | `base64 -i dist.p12 \| tr -d '\n'` |
| `IOS_DISTRIBUTION_CERT_PASSWORD` | Password set when exporting `.p12` from Keychain |
| `IOS_PROVISIONING_PROFILE_BASE64` | `base64 -i profile.mobileprovision \| tr -d '\n'` |

#### One-time setup

**1. Register Bundle ID** (do this once):
- Go to [Apple Developer → Certificates, Identifiers & Profiles](https://developer.apple.com/account/resources/identifiers/list)
- Click **+** → **App IDs** → **App**
- Description: `NetDiagnostic`, Bundle ID: `com.robinhu.netdiagnostic`
- Enable any capabilities you need, then **Register**

**2. Create Distribution Certificate** (do this once):
- Xcode → Settings → Accounts → your Apple ID → Manage Certificates
- Click **+** → **Apple Distribution**
- Or via Keychain Access → Certificate Assistant → Request a Certificate from a Certificate Authority, then upload to Developer Portal

**3. Export .p12 from Keychain** (do this once):
- Open Keychain Access → find your **Apple Distribution** certificate
- Right-click → Export → `.p12` format → set a password
- Encode: `base64 -i dist.p12 | tr -d '\n'` → paste into `IOS_DISTRIBUTION_CERT_BASE64` secret

**4. Create App Store Provisioning Profile** (regenerate when cert changes):
- [Developer Portal → Profiles](https://developer.apple.com/account/resources/profiles/list) → **+**
- Type: **App Store**, App ID: `NetDiagnostic`, Certificate: select your distribution cert
- Download, then encode: `base64 -i profile.mobileprovision | tr -d '\n'` → paste into `IOS_PROVISIONING_PROFILE_BASE64` secret

**5. Create App Store Connect API Key** (already covered above).

#### Verification

After the workflow succeeds, check [App Store Connect → TestFlight](https://appstoreconnect.apple.com). The build will appear after ~15-30 minutes of processing. Add internal testers (up to 100, no review needed) or external testers (requires Beta App Review).

## License

MIT
