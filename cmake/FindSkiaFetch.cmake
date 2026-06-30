# ── FindSkiaFetch.cmake ─────────────────────────────────────────
#
# FetchContent-based Skia build from source.
#
# Skia uses the GN meta-build system (not CMake). This module:
#   1. Downloads Skia source via FetchContent.
#   2. Creates a custom build target that runs GN → ninja.
#   3. Exposes imported targets for consumers.
#
# After including this module (guarded by ARIA_FEATURE_GPU):
#
#   Skia::skia          — Main Skia library (static)
#   Skia::skia_gpu      — GPU backend (part of skia, separate target)
#
# Variables set:
#   Skia_FOUND          — TRUE if Skia source was fetched
#   SKIA_INCLUDE_DIR    — Path to Skia's include directory
#
# Requirements (installed separately, not vendored):
#   - GN (>= 2023)     — https://gn.googlesource.com/gn
#   - Ninja (>= 1.11)  — https://ninja-build.org
#   - Python 3          — Used by Skia's build scripts
#
# If GN or ninja are not found, a FATAL_ERROR is emitted when
# ARIA_FEATURE_GPU is ON. Set Skia_FOUND to FALSE and skip
# GPU features if the toolchain is unavailable.
#
# Platform-specific GN arguments are applied based on
# ${CMAKE_SYSTEM_NAME}.

# ── Vendored source (preferred) vs FetchContent ──────────────
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/skia/BUILD.gn")
    message(STATUS "Skia: using vendored source at vendor/skia/")
    set(SKIA_TAG "vendored" CACHE INTERNAL "")
    set(skia_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/skia")
else()
    include(FetchContent)
    set(SKIA_TAG "m126" CACHE STRING "Skia git tag / branch")
    FetchContent_Declare(
        skia
        GIT_REPOSITORY https://github.com/google/skia.git
        GIT_TAG        chrome/m126
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(skia)
    if(NOT skia_POPULATED)
        FetchContent_Populate(skia)
    endif()
endif()

# ── Toolchain discovery ────────────────────────────────────────
find_program(SKIA_GN   gn   DOC "Path to the GN meta-build system")
find_program(SKIA_NINJA ninja DOC "Path to the Ninja build system")

if(NOT SKIA_GN OR NOT SKIA_NINJA)
    message(WARNING "Skia: GN (found: ${SKIA_GN}) or ninja (found: ${SKIA_NINJA}) missing."
                    " GPU rendering disabled."
                    " Install GN (https://gn.googlesource.com/gn) and ninja (https://ninja-build.org).")
    set(Skia_FOUND FALSE PARENT_SCOPE)
    return()
endif()

message(STATUS "Skia: GN found at ${SKIA_GN}")
message(STATUS "Skia: ninja found at ${SKIA_NINJA}")

# ── Build output directory ─────────────────────────────────────
# Placed under the CMake build tree so `cmake --build --preset clean`
# removes the Skia build artefacts as well.
set(SKIA_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/skia_build" CACHE INTERNAL "")

# ── GN arguments ───────────────────────────────────────────────
# These mirror the design decisions for GPU-backed Skia with Dawn.
# See: design.md ADR — Skia backend: Ganesh (GPU), Dawn FetchContent.

set(SKIA_GN_ARGS
    "skia_enable_gpu=true"
    "skia_use_dawn=true"
    "skia_enable_tools=false"
    "skia_enable_graphite=true"
    "skia_use_gl=false"        # We use Dawn, not OpenGL
    "skia_use_vulkan=false"    # We use Dawn, not raw Vulkan
    "skia_use_metal=false"     # We use Dawn, not raw Metal
    "skia_enable_spirv_validation=false"
    "skia_enable_pdf=false"
    "skia_enable_skottie=false"
    "skia_enable_svg=false"
    "skia_use_expat=false"
    "skia_use_libjpeg_turbo=false"
    "skia_use_libpng=false"
    "skia_use_libwebp=false"
    "skia_use_zlib=false"
    "skia_use_icu=false"
    "skia_use_harfbuzz=false"
    "skia_use_sfntly=false"
    "skia_use_freetype=false"
    "extra_cflags=[\"-DSKIA_C_API\"]"
    "is_official_build=true"
)

if(MSVC)
    list(APPEND SKIA_GN_ARGS
        "extra_cflags=[\"/utf-8\", \"/arch:AVX2\"]"
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND SKIA_GN_ARGS
        "extra_cflags=[\"-ffast-math\", \"-march=native\"]"
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND SKIA_GN_ARGS
        "extra_cflags=[\"-ffast-math\", \"-march=native\"]"
    )
endif()

string(REPLACE ";" " " SKIA_GN_ARGS_STR "${SKIA_GN_ARGS}")

# ── Custom build target ────────────────────────────────────────
# Skia's GN build is invoked via add_custom_target so it runs at
# build time (not configure time). This prevents long configure
# stalls and allows parallel builds.

if(NOT TARGET skia_build)
    add_custom_target(skia_build
        COMMAND ${SKIA_GN} gen "${SKIA_OUT_DIR}"
            --args="${SKIA_GN_ARGS_STR}"
        COMMAND ${SKIA_NINJA} -C "${SKIA_OUT_DIR}" skia
        COMMENT "Building Skia (GN + ninja)..."
        BYPRODUCTS
            "${SKIA_OUT_DIR}/libskia.a"
            "${SKIA_OUT_DIR}/libskia_gpu.a"
        WORKING_DIRECTORY ${skia_SOURCE_DIR}
        USES_TERMINAL
    )
endif()

# ── Imported targets ───────────────────────────────────────────
# We create INTERFACE IMPORTED targets that depend on skia_build
# so that consumers can link against them and trigger the build.

if(NOT TARGET Skia::skia)
    add_library(Skia::skia STATIC IMPORTED GLOBAL)
    add_dependencies(Skia::skia skia_build)

    if(WIN32)
        set_target_properties(Skia::skia PROPERTIES
            IMPORTED_LOCATION "${SKIA_OUT_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}skia${CMAKE_STATIC_LIBRARY_SUFFIX}"
        )
    else()
        set_target_properties(Skia::skia PROPERTIES
            IMPORTED_LOCATION "${SKIA_OUT_DIR}/libskia.a"
        )
    endif()

    target_include_directories(Skia::skia INTERFACE
        "${skia_SOURCE_DIR}/include"
        "${skia_SOURCE_DIR}/include/core"
        "${skia_SOURCE_DIR}/include/gpu"
        "${skia_SOURCE_DIR}/include/gpu/ganesh"
        "${skia_SOURCE_DIR}/include/gpu/ganesh/dawn"
        "${skia_SOURCE_DIR}/include/config"
        "${SKIA_OUT_DIR}/gen"       # Generated headers
    )
endif()

# ── Variables ──────────────────────────────────────────────────
set(Skia_FOUND TRUE PARENT_SCOPE)
set(SKIA_INCLUDE_DIR "${skia_SOURCE_DIR}/include" CACHE INTERNAL "")
set(SKIA_LIBRARIES Skia::skia CACHE INTERNAL "")

message(STATUS "Skia: source = ${skia_SOURCE_DIR}")
message(STATUS "Skia: build  = ${SKIA_OUT_DIR}")
message(STATUS "Skia: GN args: ${SKIA_GN_ARGS_STR}")

mark_as_advanced(SKIA_INCLUDE_DIR SKIA_OUT_DIR SKIA_TAG SKIA_GN SKIA_NINJA)
