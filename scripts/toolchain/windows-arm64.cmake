# =============================================================================
# Windows ARM64 cross-compilation toolchain (from Linux via LLVM-MinGW)
#
# GCC does NOT support aarch64-w64-mingw32 — only Clang/LLVM does.
# Requires: LLVM-MinGW from https://github.com/mstorsjo/llvm-mingw
#           The script auto-installs this via install_llvm_mingw_arm64().
# =============================================================================
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)

set(CMAKE_C_COMPILER   aarch64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER aarch64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER  aarch64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(WIN32 1)
set(MINGW 1)

# ── Linker flags (Clang/MinGW uses lld) ──────────────────────────────────
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld -static" CACHE STRING "")
