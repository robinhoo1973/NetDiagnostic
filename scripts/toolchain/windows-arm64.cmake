# =============================================================================
# Windows ARM64 cross-compilation toolchain (from Linux via MinGW-w64)
#
# Requires: apt install g++-aarch64-linux-gnu mingw-w64-tools
#           + Qt6 built for aarch64-w64-mingw32
# =============================================================================
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)

set(CMAKE_C_COMPILER   aarch64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER aarch64-w64-mingw32-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(WIN32 1)
set(MINGW 1)

# ── Static linking ───────────────────────────────────────────────────────
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static" CACHE STRING "")
