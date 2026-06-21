#!/usr/bin/env bash
# =============================================================================
# rebuild.sh — Quick native rebuild for development
#
# Builds Release binaries for the current host architecture.
# Output: dist/net_diagnostic-<os>-<arch>
#         dist/net_diagnostic_sim-<os>-<arch>
#
# Usage: ./rebuild.sh [clean]
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/netdiag-rebuild"
DIST_DIR="${TMPDIR:-/tmp}/netdiag-dist"
JOBS="$(nproc 2>/dev/null || echo 4)"

# ── OS/arch detection ──────────────────────────────────────────────────────
case "$(uname -s)" in
    Linux)  HOST_OS="Linux" ;;
    Darwin) HOST_OS="macOS" ;;
    MINGW*|MSYS*) HOST_OS="Windows" ;;
    *)      HOST_OS="$(uname -s)" ;;
esac
HOST_ARCH="$(uname -m)"
[[ "$HOST_ARCH" == "x86_64" ]] && HOST_ARCH="x86_64"
[[ "$HOST_ARCH" == "aarch64" ]] && HOST_ARCH="arm64"

echo "=== Native rebuild: ${HOST_OS}-${HOST_ARCH} ==="

[[ "${1:-}" == "clean" ]] && rm -rf "$BUILD_DIR"

mkdir -p "$BUILD_DIR" "$DIST_DIR"

# ── CMake Configure ────────────────────────────────────────────────────────
# Use arch-specific toolchain if available (sets -march and static flags)
TC_FILE=""
case "$HOST_ARCH" in
    arm64)  [[ -f "$PROJECT_DIR/scripts/toolchain/linux-arm64.cmake" ]] && TC_FILE="$PROJECT_DIR/scripts/toolchain/linux-arm64.cmake" ;;
    x86_64) [[ -f "$PROJECT_DIR/scripts/toolchain/linux-x86_64.cmake" ]] && TC_FILE="$PROJECT_DIR/scripts/toolchain/linux-x86_64.cmake" ;;
esac

CMAKE_ARGS=(
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_TESTS=OFF
    -DBUILD_SIMULATOR=ON
)
[[ -n "$TC_FILE" ]] && CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC_FILE")

cmake "${CMAKE_ARGS[@]}" -B "$BUILD_DIR" -S "$PROJECT_DIR" || {
    echo "ERROR: CMake configure failed"
    exit 1
}

# ── Build ──────────────────────────────────────────────────────────────────
ninja -C "$BUILD_DIR" net_diagnostic net_diagnostic_sim -j"$JOBS" || {
    echo "ERROR: Build failed"
    exit 1
}

# ── Copy outputs ───────────────────────────────────────────────────────────
BIN="$BUILD_DIR/net_diagnostic"
SIM="$BUILD_DIR/net_diagnostic_sim"
EXT=""
[[ "$HOST_OS" == "Windows" ]] && EXT=".exe"

cp "$BIN" "$DIST_DIR/net_diagnostic-${HOST_OS}-${HOST_ARCH}${EXT}" 2>/dev/null || {
    echo "ERROR: net_diagnostic binary not found at $BIN"
    exit 1
}
cp "$SIM" "$DIST_DIR/net_diagnostic_sim-${HOST_OS}-${HOST_ARCH}${EXT}" 2>/dev/null || true

echo ""
echo "Build complete:"
ls -lh "$DIST_DIR"/net_diagnostic* 2>/dev/null
echo ""
echo "Run: $DIST_DIR/net_diagnostic-${HOST_OS}-${HOST_ARCH}${EXT}"
