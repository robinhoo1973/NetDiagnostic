# =============================================================================
# Linux x86_64 cross-compilation toolchain (from aarch64 host)
#
# Requires: dpkg --add-architecture amd64
#           apt install crossbuild-essential-amd64 qt6-base-dev:amd64 ...
# =============================================================================
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   x86_64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER x86_64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(PKG_CONFIG_EXECUTABLE /usr/bin/x86_64-linux-gnu-pkg-config CACHE FILEPATH "pkg-config for target")

# ── Static linking hints ─────────────────────────────────────────────────
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++" CACHE STRING "")
