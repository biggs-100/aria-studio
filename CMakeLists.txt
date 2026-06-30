cmake_minimum_required(VERSION 3.28)
project(aria-daw VERSION 0.1.0 LANGUAGES CXX)

# ── C++ Standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

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

# ── Platform libraries (accumulated by MIDI and Audio sections) ──
set(PLATFORM_LIBS "")

# ── MIDI sources ──────────────────────────────────────────────────
set(MIDI_SOURCES
    src/midi/midi_clip.cc
    src/midi/note_tracker.cc
    src/midi/midi_node.cc
    src/midi/midi_node_types.cc
    src/midi/midi_graph.cc
    src/midi/midi_clock.cc
    src/midi/midi_engine.cc
    src/midi/midi_recorder.cc
    src/midi/jitter_compensator.cc
    src/midi/mpe.cc
    src/midi/driver/virtual_port_driver.cc
    src/midi/driver/platform_midi.cc
)

# ── Conditional MIDI drivers (platform-specific) ─────────────────
if(WIN32)
    list(APPEND MIDI_SOURCES src/midi/driver/winmm_driver.cc)
elseif(APPLE)
    # CoreMIDI on macOS requires Objective-C++ (.mm)
    set_source_files_properties(
        src/midi/driver/coremidi_driver.mm PROPERTIES
        LANGUAGE CXX
    )
    list(APPEND MIDI_SOURCES src/midi/driver/coremidi_driver.mm)
    list(APPEND PLATFORM_LIBS "-framework CoreMIDI" "-framework CoreFoundation")
elseif(UNIX)
    list(APPEND MIDI_SOURCES src/midi/driver/alsa_seq_driver.cc)
    if(PkgConfig_FOUND)
        pkg_check_modules(ALSA REQUIRED alsa)
        set(MIDI_INCLUDE_DIRS ${ALSA_INCLUDE_DIRS})
        set(MIDI_LINK_DIRS ${ALSA_LIBRARY_DIRS})
        list(APPEND PLATFORM_LIBS ${ALSA_LIBRARIES})
    endif()
endif()

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
    src/audio/wsola.cc
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

# ── Piano Roll sources ─────────────────────────────────────────
set(PIANOROLL_SOURCES
    src/pianoroll/note_collection.cc
    src/pianoroll/spatial_grid.cc
    src/pianoroll/piano_roll_canvas.cc
    src/pianoroll/piano_roll_renderer.cc
    src/pianoroll/piano_keyboard.cc
    src/pianoroll/tool_manager.cc
    src/pianoroll/selection_manager.cc
    src/pianoroll/snap_system.cc
    src/pianoroll/scale_system.cc
    src/pianoroll/chord_generator.cc
    src/pianoroll/quantize_engine.cc
    src/pianoroll/humanize_engine.cc
    src/pianoroll/ghost_notes.cc
    src/pianoroll/step_input.cc
    src/pianoroll/velocity_lane.cc
    src/pianoroll/expression_lane.cc
)

# ── Mixer sources ──────────────────────────────────────────────────
set(MIXER_SOURCES
    src/mixer/channel.cc
    src/mixer/mixer.cc
    src/mixer/routing.cc
    src/mixer/sidechain.cc
    src/mixer/builtin_eq.cc
    src/mixer/master_channel.cc
    src/mixer/mixer_ui.cc
)

# ── Plugin sources ───────────────────────────────────────────────────
set(PLUGIN_SOURCES
    src/plugin/audio_plugin.cc
    src/plugin/clap_host.cc
    src/plugin/clap_scanner.cc
    src/plugin/format_scanner.cc
    src/plugin/native_clap_factory.cc
    src/plugin/plugin_audio_node.cc
    src/plugin/plugin_blacklist.cc
    src/plugin/plugin_factory.cc
    src/plugin/plugin_host.cc
    src/plugin/plugin_sandbox.cc
    src/plugin/plugin_scanner.cc
    src/plugin/plugin_scanner_defaults.cc
    src/plugin/vst3_host.cc
    src/plugin/vst3_scanner.cc
)

# ── Timeline / Model / Project sources ─────────────────────────
set(MODEL_SOURCES
    src/model/marker.cc
    src/model/track.cc
    src/model/track_types.cc
    src/model/session.cc
    src/model/freeze_manager.cc
    src/model/clip.cc
    src/model/audio_clip.cc
    src/model/automation_clip_wrapper.cc
    src/model/routing.cc
)

set(TIMELINE_SOURCES
    src/timeline/timeline.cc
    src/timeline/tempo_track.cc
)

set(PROJECT_SOURCES
    src/project/project_manager.cc
    src/project/arrangement.cc
)

# ── Browser sources (P9: File Browser, Search, Preview, Waveform) ──
set(BROWSER_SOURCES
    src/browser/browser_engine.cc
    src/browser/search_provider.cc
    src/browser/preview_player.cc
    src/browser/waveform_cache.cc
    src/browser/browser_panel.cc
    src/browser/arrangement_drop_target.cc
)

# ── Graphics sources (ALL guarded by ARIA_FEATURE_GPU) ──
# These depend on Dawn+Skia which may not be available.
set(GRAPHICS_SOURCES "")

if(ARIA_FEATURE_GPU)
    list(APPEND GRAPHICS_SOURCES
        src/graphics/graphics_engine.cc
        src/graphics/animator.cc
        src/graphics/skia_canvas.cc
        src/graphics/widget.cc
        src/graphics/widget_rect.cc
        src/graphics/widget_text.cc
        src/graphics/widget_button.cc
        src/graphics/widget_container.cc
        src/graphics/layout_engine.cc
        src/graphics/dirty_tracker.cc
        src/graphics/render_loop.cc
        src/graphics/focus_manager.cc
        src/graphics/shortcut_manager.cc
        src/graphics/ffi/bridge.cc
        src/graphics/ffi/event_dispatcher.cc
        src/graphics/theme_engine.cc
        src/graphics/widget_themed.cc
        src/graphics/file_watcher.cc
        src/browser/browser_panel_gpu.cc
        src/browser/browser_engine.cc
        src/browser/search_provider.cc
        src/browser/preview_player.cc
        src/browser/waveform_cache.cc
        src/browser/arrangement_drop_target.cc
    )
endif()

# ── Automation sources ──────────────────────────────────────────
set(AUTOMATION_SOURCES
    src/automation/automation_engine.cc
    src/automation/automation_clip.cc
    src/automation/step_clip.cc
    src/automation/lfo_clip.cc
    src/automation/envelope.cc
    src/automation/modulation_types.cc
    src/automation/modulation_source.cc
    src/automation/step_sequencer_source.cc
    src/automation/macros.cc
    src/automation/random_source.cc
    src/automation/note_property_source.cc
    src/automation/modulation_matrix.cc
    src/automation/nested_modulation.cc
    src/automation/lane.cc
    src/automation/recorder.cc
    src/automation/cache.cc
)

add_library(aria_core STATIC
    src/kernel/application.cc
    src/kernel/service_locator.cc
    src/kernel/event_bus.cc
    src/kernel/command_queue.cc
    src/kernel/undo_stack.cc
    src/kernel/crash_handler.cc
    ${AUDIO_SOURCES}
    ${DRIVER_SOURCES}
    ${MIDI_SOURCES}
    ${PIANOROLL_SOURCES}
    ${MODEL_SOURCES}
    ${TIMELINE_SOURCES}
    ${PROJECT_SOURCES}
    ${MIXER_SOURCES}
    ${PLUGIN_SOURCES}
    ${AUTOMATION_SOURCES}
    ${BROWSER_SOURCES}
    ${GRAPHICS_SOURCES}
)

target_include_directories(aria_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/kernel
    ${CMAKE_SOURCE_DIR}/vendor/vst3sdk
)

# CLAP include directory (from FindDependencies: vendored or FetchContent)
if(CLAP_INCLUDE_DIR)
    target_include_directories(aria_core PRIVATE ${CLAP_INCLUDE_DIR})
endif()

# GPU / Graphics include directory
if(ARIA_FEATURE_GPU)
    target_include_directories(aria_core PRIVATE ${CMAKE_SOURCE_DIR}/src/graphics)
    target_compile_definitions(aria_core PRIVATE ARIA_FEATURE_GPU)

    # Dawn WebGPU platform-specific link dependencies
    if(WIN32)
        target_link_libraries(aria_core PRIVATE dxgi d3d12 d3d11)
    elseif(APPLE)
        target_link_libraries(aria_core PRIVATE
            "-framework Metal"
            "-framework QuartzCore"
            "-framework Foundation"
            "-framework IOSurface"
        )
    elseif(UNIX)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            target_link_libraries(aria_core PRIVATE Vulkan::Vulkan)
        endif()
    endif()
endif()

# VST3 compile definition (enables VST3 code paths)
if(ARIA_FEATURE_VST3)
    target_compile_definitions(aria_core PRIVATE ARIA_FEATURE_VST3)
endif()

if(DEFINED MIDI_INCLUDE_DIRS)
    target_include_directories(aria_core PRIVATE ${MIDI_INCLUDE_DIRS})
    target_link_directories(aria_core PRIVATE ${MIDI_LINK_DIRS})
endif()

if(UNIX AND NOT APPLE AND DEFINED DRIVER_INCLUDE_DIRS)
    target_include_directories(aria_core PRIVATE ${DRIVER_INCLUDE_DIRS})
    target_link_directories(aria_core PRIVATE ${DRIVER_LINK_DIRS})
endif()

target_link_libraries(aria_core PUBLIC
    aria_rust
    aria_ui_bundle
    aria_imgui
    nlohmann_json
    ${SKIA_LIBRARIES}
    ${DAWN_LIBRARIES}
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
