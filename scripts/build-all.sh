#!/usr/bin/env bash
# =============================================================================
# build-all.sh — Multi-platform build automation for NetDiagnostic-QT
#
# Usage:
#   ./scripts/build-all.sh                        # Build all available targets
#   ./scripts/build-all.sh linux-arm64            # Build specific target only
#   ./scripts/build-all.sh --clean                # Clean before building
#   ./scripts/build-all.sh --list                 # List targets and prerequisites
#
# Output naming:  dist/net_diagnostic-<OS>-<arch>[.exe]
#   e.g. dist/net_diagnostic-Linux-arm64
#        dist/net_diagnostic-Windows-x86_64.exe
#
# Prerequisites per target:
#   linux-arm64     : g++, Qt6 (native), CMake ≥ 3.22, ninja
#   linux-x86_64    : crossbuild-essential-amd64, Qt6:amd64
#   windows-x86_64  : mingw-w64-x86-64, Qt6-mingw-w64
#   macos-*         : requires Apple macOS host with Xcode
#   android-*       : requires Android NDK + Qt6 for Android
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$PROJECT_DIR/dist"
BUILD_BASE="${TMPDIR:-/tmp}/netdiag-build"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
log_info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_err()   { echo -e "${RED}[ERROR]${NC} $*"; }

# ── Target registry ─────────────────────────────────────────────────────────
# Fields: id os arch compiler toolchain ext prereq_description
declare -A T_OS T_ARCH T_CC T_TOOLCHAIN T_EXT T_PREREQ
TARGETS=()

reg() {
    local id="$1" os="$2" arch="$3" cc="$4" tc="$5" ext="$6" prereq="$7"
    TARGETS+=("$id")
    T_OS[$id]="$os"; T_ARCH[$id]="$arch"; T_CC[$id]="$cc"
    T_TOOLCHAIN[$id]="$tc"; T_EXT[$id]="$ext"; T_PREREQ[$id]="$prereq"
}

# Register available build targets
reg "linux-arm64"     "Linux"   "arm64"  "g++"                       ""                         ""   "g++ / Qt6 native"
reg "linux-x86_64"    "Linux"   "x86_64" "x86_64-linux-gnu-g++"      "linux-x86_64.cmake"       ""   "crossbuild-essential-amd64 / Qt6:amd64"
reg "windows-x86_64"  "Windows" "x86_64" "x86_64-w64-mingw32-g++"    "windows-x86_64.cmake"     ".exe" "mingw-w64-x86-64 / Qt6-mingw-w64"

# ── Check functions ──────────────────────────────────────────────────────────
check_cmd() { command -v "$1" >/dev/null 2>&1; }

# ── List mode ────────────────────────────────────────────────────────────────
list_targets() {
    echo ""
    echo "Available build targets:"
    echo "────────────────────────"
    for id in "${TARGETS[@]}"; do
        local cc="${T_CC[$id]}"
        local ok="✓"
        check_cmd "$cc" || ok="✗ (missing $cc)"
        printf "  %-22s %-12s %-8s  %s\n" "$id" "${T_OS[$id]}" "${T_ARCH[$id]}" "$ok"
        echo "      prereq: ${T_PREREQ[$id]}"
    done
    echo ""
    echo "Current host: $(uname -m) / $(uname -s)"
    echo "Native target: linux-arm64"
    exit 0
}

# ── Build one target ─────────────────────────────────────────────────────────
build_target() {
    local id="$1"
    local build_dir="$BUILD_BASE/$id"
    local toolchain_file="${T_TOOLCHAIN[$id]}"
    local out_name="net_diagnostic-${T_OS[$id]}-${T_ARCH[$id]}${T_EXT[$id]}"
    local bin_name="net_diagnostic${T_EXT[$id]}"

    log_info "Building: ${T_OS[$id]} ${T_ARCH[$id]} → $out_name"

    rm -rf "$build_dir"
    mkdir -p "$build_dir" "$DIST_DIR"

    local cmake_args=(
        -G Ninja
        -B "$build_dir"
        -S "$PROJECT_DIR"
        -DCMAKE_BUILD_TYPE=Release
        -DBUILD_TESTS=OFF
    )

    if [ -n "$toolchain_file" ]; then
        cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/toolchain/$toolchain_file")
    fi

    # Configure
    log_info "  Configuring..."
    if ! cmake "${cmake_args[@]}" > "$build_dir/cmake-configure.log" 2>&1; then
        tail -20 "$build_dir/cmake-configure.log"
        log_err "CMake configure failed for $id"
        log_err "  → ${T_PREREQ[$id]}"
        return 1
    fi

    # Build
    log_info "  Compiling..."
    if ! cmake --build "$build_dir" > "$build_dir/cmake-build.log" 2>&1; then
        tail -30 "$build_dir/cmake-build.log"
        log_err "Build failed for $id"
        return 1
    fi

    # Find output binary
    local bin_path
    bin_path=$(find "$build_dir" -maxdepth 4 -name "$bin_name" -type f ! -name "*.o" ! -name "*.cmake" 2>/dev/null | head -1)
    if [ -z "$bin_path" ]; then
        log_err "Binary '$bin_name' not found after build"
        return 1
    fi

    cp "$bin_path" "$DIST_DIR/$out_name"
    strip "$DIST_DIR/$out_name" 2>/dev/null || true

    local bin_size
    bin_size=$(stat -c%s "$DIST_DIR/$out_name" 2>/dev/null || du -b "$DIST_DIR/$out_name" 2>/dev/null | cut -f1)
    local bin_size_h
    bin_size_h=$(du -h "$DIST_DIR/$out_name" 2>/dev/null | cut -f1)
    log_ok "Built: $out_name (${bin_size_h}B, ${bin_size} bytes)"

    return 0
}

# ── Main ─────────────────────────────────────────────────────────────────────
main() {
    local clean_mode=false filter_target=""
    local build_count=0 fail_count=0 skip_count=0

    for arg in "$@"; do
        case "$arg" in
            --clean|-c) clean_mode=true ;;
            --list|-l) list_targets ;;
            *) filter_target="$arg" ;;
        esac
    done

    echo ""
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║  NetDiagnostic-QT — Multi-Platform Build Automation         ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""
    log_info "Project:  $PROJECT_DIR"
    log_info "Dist:     $DIST_DIR"
    log_info "Host:     $(uname -m) / $(uname -s)"
    echo ""

    if $clean_mode; then
        log_info "Cleaning previous builds..."
        rm -rf "$BUILD_BASE"
    fi

    mkdir -p "$DIST_DIR"

    for id in "${TARGETS[@]}"; do
        if [ -n "$filter_target" ] && [ "$id" != "$filter_target" ]; then
            continue
        fi

        local cc="${T_CC[$id]}"
        if ! check_cmd "$cc"; then
            log_warn "Skipping $id — compiler '$cc' not found"
            log_warn "  → Install: ${T_PREREQ[$id]}"
            skip_count=$((skip_count + 1))
            echo ""
            continue
        fi

        if build_target "$id"; then
            build_count=$((build_count + 1))
        else
            fail_count=$((fail_count + 1))
        fi
        echo ""
    done

    # ── Summary ─────────────────────────────────────────────────────────────────
    echo "═══════════════════════════════════════════════════════════════"
    echo "  Build Summary"
    echo "═══════════════════════════════════════════════════════════════"
    log_ok "Succeeded: $build_count"
    [ $fail_count -gt 0 ] && log_err "Failed:    $fail_count"
    [ $skip_count -gt 0 ] && log_warn "Skipped:   $skip_count (install prerequisites)"
    echo ""
    log_info "Output: $DIST_DIR"
    ls -lh "$DIST_DIR"/net_diagnostic-* 2>/dev/null || log_warn "No binaries in dist/"
    echo ""

    if [ $fail_count -gt 0 ]; then exit 1; fi
}

main "$@"
