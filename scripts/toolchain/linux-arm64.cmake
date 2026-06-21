# =============================================================================
# Linux ARM64 native build — optimization and static linking settings
#
# This is NOT a cross-compilation toolchain (compiler = system default g++).
# It provides arch-specific tuning for arm64/aarch64 targets.
# =============================================================================
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ── Use system compiler (native build) ───────────────────────────────────
set(CMAKE_C_COMPILER   gcc   CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER g++   CACHE FILEPATH "")

# ── ARM64 tuning ─────────────────────────────────────────────────────────
# -march=armv8-a is baseline for all AArch64; omit -mcpu to let compiler
# auto-detect the host CPU for best performance.
set(CMAKE_C_FLAGS   "-O2 -march=armv8-a" CACHE STRING "")
set(CMAKE_CXX_FLAGS "-O2 -march=armv8-a" CACHE STRING "")

# ── Static linking for libgcc/libstdc++ (avoids runtime dependency) ──────
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++" CACHE STRING "")

# ── CMake find behaviour (native = search standard system paths) ─────────
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
