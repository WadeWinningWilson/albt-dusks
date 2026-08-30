# ============================================
# NEW CODE — ALBT multiplatform
# Cross-compile Game ABI 2 mods from Windows via Zig.
#
# Parameterised replacement for linux-x86_64-zig.cmake. Drive it with:
#   -DDUSK_ZIG_TRIPLE=aarch64-linux-gnu
#   -DDUSK_TARGET_SYSTEM=Linux
#   -DDUSK_TARGET_PROCESSOR=aarch64
#
# DUSK_TARGET_SYSTEM / DUSK_TARGET_PROCESSOR feed CMAKE_SYSTEM_NAME /
# CMAKE_SYSTEM_PROCESSOR, which is what the Mod SDK's _mod_lib_info() reads to
# name lib/<platform>/. Keep them matching the CI matrix names exactly.
# ============================================
# try_compile() spawns a sub-project that re-includes this file with a fresh
# cache, so the -D values must be forwarded explicitly or the guards below fire
# inside the compiler-ABI probe.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        DUSK_ZIG_TRIPLE DUSK_TARGET_SYSTEM DUSK_TARGET_PROCESSOR ZIG)

if (NOT DUSK_ZIG_TRIPLE)
    message(FATAL_ERROR "zig-cross: set -DDUSK_ZIG_TRIPLE (e.g. aarch64-linux-gnu)")
endif ()
if (NOT DUSK_TARGET_SYSTEM)
    message(FATAL_ERROR "zig-cross: set -DDUSK_TARGET_SYSTEM (Linux/Darwin/Android)")
endif ()
if (NOT DUSK_TARGET_PROCESSOR)
    message(FATAL_ERROR "zig-cross: set -DDUSK_TARGET_PROCESSOR (x86_64/aarch64/arm64)")
endif ()

set(CMAKE_SYSTEM_NAME "${DUSK_TARGET_SYSTEM}")
set(CMAKE_SYSTEM_PROCESSOR "${DUSK_TARGET_PROCESSOR}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if (NOT ZIG)
    set(ZIG "zig")
endif ()

set(CMAKE_C_COMPILER "${ZIG}")
set(CMAKE_CXX_COMPILER "${ZIG}")
set(CMAKE_C_COMPILER_ARG1 "cc")
set(CMAKE_CXX_COMPILER_ARG1 "c++")

set(_t "-target ${DUSK_ZIG_TRIPLE}")
set(CMAKE_C_FLAGS_INIT "${_t}")
set(CMAKE_CXX_FLAGS_INIT "${_t}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_t} -Wl,--allow-shlib-undefined")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_t} -Wl,--allow-shlib-undefined")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_t} -Wl,--allow-shlib-undefined")

# Apple's linker does not take --allow-shlib-undefined; mods there link the stub.
if (DUSK_TARGET_SYSTEM STREQUAL "Darwin" OR DUSK_TARGET_SYSTEM STREQUAL "iOS")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${_t}")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_t} -Wl,-undefined,dynamic_lookup")
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_t} -Wl,-undefined,dynamic_lookup")
endif ()

set(CMAKE_C_COMPILER_ID Clang)
set(CMAKE_CXX_COMPILER_ID Clang)
set(CMAKE_C_COMPILER_VERSION 19.0)
set(CMAKE_CXX_COMPILER_VERSION 19.0)
set(CMAKE_C_COMPILER_FRONTEND_VARIANT GNU)
set(CMAKE_CXX_COMPILER_FRONTEND_VARIANT GNU)
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
