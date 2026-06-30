# ── FindDawn.cmake ──────────────────────────────────────────────
#
# FetchContent-based discovery of Google Dawn (WebGPU implementation).
#
# After including this module (guarded by ARIA_FEATURE_GPU), the
# following targets are available:
#
#   Dawn::dawn_native   — Dawn native implementation (adapter, device)
#   Dawn::dawn_platform — Dawn platform abstraction layer
#   Dawn::dawn_proc     — Dawn procedure table
#   Dawn::webgpu_dawn   — webgpu.h C API header + stubs
#
# Variables set:
#   Dawn_FOUND        — TRUE on success
#   DAWN_INCLUDE_DIR  — Path to Dawn's include directory
#   DAWN_LIBRARIES    — List of imported Dawn targets
#
# Platform-specific system dependencies (D3D12, Metal, Vulkan)
# are linked automatically by Dawn's own CMake.

# ── Vendored source (preferred) vs FetchContent ──────────────
# If vendor/dawn/ exists, use it directly (no network fetch).
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/dawn/CMakeLists.txt")
    message(STATUS "Dawn: using vendored source at vendor/dawn/")
    set(DAWN_TAG "vendored" CACHE INTERNAL "")
    add_subdirectory("${CMAKE_SOURCE_DIR}/vendor/dawn" "build_vendor_dawn" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)

    set(DAWN_TAG "chromium/6824" CACHE STRING "Dawn git tag / revision")

    FetchContent_Declare(
        dawn
        GIT_REPOSITORY https://github.com/google/dawn.git
        GIT_TAG        ${DAWN_TAG}
        GIT_SHALLOW    TRUE
    )

    set(DAWN_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
    set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_D3D11  ON  CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_D3D12  ON  CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_METAL  ON  CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_VULKAN ON  CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(dawn)
endif()

# ── Variables ──────────────────────────────────────────────────
set(Dawn_FOUND TRUE)
set(DAWN_INCLUDE_DIR "${dawn_SOURCE_DIR}/include" CACHE INTERNAL "")

# ── Imported targets (aliases for Dawn's native targets) ───────
# Dawn's CMake generates targets with unprefixed names (dawn_native,
# dawn_platform, etc.). We create Dawn:: prefixed aliases for
# consistency with the rest of the ARIA build.

macro(_dawn_alias native_name alias_name)
    if(TARGET ${native_name} AND NOT TARGET ${alias_name})
        add_library(${alias_name} ALIAS ${native_name})
    endif()
endmacro()

_dawn_alias(dawn_native   Dawn::dawn_native)
_dawn_alias(dawn_platform Dawn::dawn_platform)
_dawn_alias(dawn_proc     Dawn::dawn_proc)
_dawn_alias(webgpu_dawn   Dawn::webgpu_dawn)
_dawn_alias(dawn_glfw     Dawn::dawn_glfw)  # optional, for examples

# ── Consumer variable ──────────────────────────────────────────
set(DAWN_LIBRARIES
    Dawn::dawn_native
    Dawn::dawn_platform
    Dawn::dawn_proc
    Dawn::webgpu_dawn
    CACHE INTERNAL "Dawn WebGPU libraries"
)

message(STATUS "Dawn: found (${DAWN_TAG}, include = ${DAWN_INCLUDE_DIR})")

mark_as_advanced(DAWN_INCLUDE_DIR DAWN_TAG)
