cmake_minimum_required(VERSION 3.28)
project(aria-daw VERSION 0.1.0 LANGUAGES CXX)

# ── C++ Standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ───────────────────────────────────────────────────
option(ARIA_BUILD_TESTS "Build tests" ON)
option(ARIA_BUILD_TOOLS "Build developer tools" OFF)
option(ARIA_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ARIA_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ARIA_ENABLE_LTO "Enable LTO" ON)
option(ARIA_FEATURE_AI "Enable AI features" ON)
option(ARIA_FEATURE_NETWORKING "Enable networking" ON)
option(ARIA_FEATURE_SCRIPTING "Enable Lua scripting" ON)
option(ARIA_FEATURE_VST3 "Enable VST3 support" ON)
option(ARIA_FEATURE_AU "Enable Audio Unit support" OFF)
option(ARIA_FEATURE_LV2 "Enable LV2 support" OFF)
option(ARIA_FEATURE_GPU "Enable GPU rendering" ON)

# ── Compiler Options ──────────────────────────────────────────
include(cmake/CompilerOptions.cmake)

# ── Dependencies ──────────────────────────────────────────────
include(cmake/FindDependencies.cmake)

# ── Rust Library ──────────────────────────────────────────────
# Rust workspace member crates produce individual .rlib outputs.
# For C++ linkage we use an INTERFACE target so consumers can link
# without requiring a compiled Rust cdylib during C++ development.
add_library(aria_rust INTERFACE)
target_include_directories(aria_rust INTERFACE "${CMAKE_SOURCE_DIR}/rust/include")

# ── TypeScript UI ─────────────────────────────────────────────
if(EXISTS "${CMAKE_SOURCE_DIR}/ui/package.json")
    add_custom_target(ts_ui_build
        COMMAND ${CMAKE_COMMAND} -E echo "Building TypeScript UI..."
        COMMAND pnpm --dir "${CMAKE_SOURCE_DIR}/ui" install --frozen-lockfile
        COMMAND pnpm --dir "${CMAKE_SOURCE_DIR}/ui" build
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/ui/dist/aria_ui.js"
        COMMENT "Building ARIA TypeScript UI"
    )

    add_library(aria_ui_bundle INTERFACE)
    target_include_directories(aria_ui_bundle INTERFACE
        "${CMAKE_SOURCE_DIR}/ui/dist"
    )
endif()

# ── Core Library ──────────────────────────────────────────────

# ── Audio sources (shared) ──────────────────────────────────────
set(AUDIO_SOURCES
    src/audio/audio_clock.cc
    src/audio/audio_engine.cc
    src/audio/audio_graph.cc
    src/audio/audio_node.cc
    src/audio/audio_node_types.cc
    src/audio/buffer_pool.cc
    src/audio/diagnostics.cc
    src/audio/multi_core_distributor.cc
    src/audio/pdc_manager.cc
    src/audio/simd_ops.cc
    src/audio/src_converter.cc
    src/audio/tempo_map.cc
    src/audio/track_processor.cc
    src/audio/transport.cc
    # ── Export / Offline ────────────────────────────────────────
    src/audio/export/dither.cc
    src/audio/export/file_writer.cc
    src/audio/export/offline_renderer.cc
    # ── Metering / Analysis ─────────────────────────────────────
    src/audio/metering/lufs_meter.cc
    src/audio/metering/peak_meter.cc
    src/audio/metering/phase_meter.cc
    src/audio/metering/rms_meter.cc
    src/audio/metering/spectrum_analyzer.cc
)

# ── Audio Driver sources (platform-specific) ────────────────────
set(DRIVER_SOURCES
    src/audio/driver/platform_driver.cc
)

set(PLATFORM_LIBS "")

if(WIN32)
    list(APPEND DRIVER_SOURCES
        src/audio/driver/asio_driver.cc
        src/audio/driver/wasapi_driver.cc
    )
    list(APPEND PLATFORM_LIBS ole32)
endif()

if(APPLE)
    list(APPEND DRIVER_SOURCES
        src/audio/driver/coreaudio_driver.mm
    )
    list(APPEND PLATFORM_LIBS
        "-framework CoreAudio"
        "-framework AudioUnit"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
endif()

if(UNIX AND NOT APPLE)
    list(APPEND DRIVER_SOURCES
        src/audio/driver/alsa_driver.cc
        src/audio/driver/pipewire_driver.cc
    )
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ALSA REQUIRED alsa)
        pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
        set(DRIVER_INCLUDE_DIRS
            ${ALSA_INCLUDE_DIRS}
            ${PIPEWIRE_INCLUDE_DIRS}
        )
        set(DRIVER_LINK_DIRS
            ${ALSA_LIBRARY_DIRS}
            ${PIPEWIRE_LIBRARY_DIRS}
        )
        list(APPEND PLATFORM_LIBS
            ${ALSA_LIBRARIES}
            ${PIPEWIRE_LIBRARIES}
        )
    endif()
endif()

add_library(aria_core STATIC
    src/kernel/application.cc
    src/kernel/service_locator.cc
    src/kernel/event_bus.cc
    src/kernel/command_queue.cc
    src/kernel/undo_stack.cc
    src/kernel/crash_handler.cc
    ${AUDIO_SOURCES}
    ${DRIVER_SOURCES}
)

target_include_directories(aria_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/kernel
)

if(UNIX AND NOT APPLE AND DEFINED DRIVER_INCLUDE_DIRS)
    target_include_directories(aria_core PRIVATE ${DRIVER_INCLUDE_DIRS})
    target_link_directories(aria_core PRIVATE ${DRIVER_LINK_DIRS})
endif()

target_link_libraries(aria_core PUBLIC
    aria_rust
    aria_ui_bundle
    ${SKIA_LIBRARIES}
    ${WEBGPU_LIBRARIES}
    ${LUA_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${ZSTD_LIBRARIES}
    ${PLATFORM_LIBS}
)

# ── Main Executable ───────────────────────────────────────────
add_executable(aria
    src/main.cc
    src/platform/platform_impl.cc
)

target_link_libraries(aria PRIVATE aria_core)

# ── Tests ─────────────────────────────────────────────────────
if(ARIA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
