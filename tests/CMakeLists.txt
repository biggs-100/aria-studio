# ── Test Build ────────────────────────────────────────────

# Catch2 is fetched by FindDependencies.cmake via FetchContent.
# The targets are created by FetchContent's add_subdirectory.
# Ensure extras/ is in the module path for catch_discover_tests.
if(DEFINED catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

# Audio engine tests (standalone — covers Slice 1 + Slice 2)
add_executable(aria_audio_tests
    unit/test_audio_main.cc
    unit/test_audio_buffer.cc
    unit/test_audio_graph.cc
    unit/test_audio_clock.cc
    unit/test_transport.cc
    unit/test_tempo_map.cc
    test_common/audio_harness.cc
)

target_link_libraries(aria_audio_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_audio_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

# Register tests for the main audio test suite
include(Catch)
catch_discover_tests(aria_audio_tests)

# ── Driver mock tests (platform-agnostic) ────────────────────
# These tests verify the AudioDriver interface contract using mocks,
# without requiring actual audio hardware.
add_executable(aria_driver_tests
    unit/test_audio_main.cc
    unit/test_driver_mock.cc
    test_common/audio_harness.cc
)

target_link_libraries(aria_driver_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_driver_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_driver_tests)

# ── Slice 3: Pipeline, PDC, Multi-core, Diagnostics tests ───
add_executable(aria_slice3_tests
    unit/test_audio_main.cc
    unit/test_audio_pipeline.cc
    unit/test_pdc.cc
    unit/test_multicore.cc
    unit/test_diagnostics.cc
    unit/test_wsola.cc
    test_common/audio_harness.cc
)

target_link_libraries(aria_slice3_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_slice3_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_slice3_tests)

# ── MIDI engine tests (Slice 1: Data model, graph, clock) ──────
add_executable(aria_midi_tests
    unit/test_midi_main.cc
    unit/test_midi_types.cc
    unit/test_midi_graph.cc
    unit/test_midi_clock.cc
    unit/test_note_tracker.cc
)

target_link_libraries(aria_midi_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_midi_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_midi_tests)

# ── MIDI engine tests (Slice 2: drivers, recorder, MPE) ─────────
add_executable(aria_midi_slice2_tests
    unit/test_midi_main.cc
    unit/test_midi_recorder.cc
    unit/test_mpe.cc
    unit/test_midi_driver_mock.cc
)

target_link_libraries(aria_midi_slice2_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_midi_slice2_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_midi_slice2_tests)

# ── Slice 4: Export, Metering, SRC tests ───────────────────────
add_executable(aria_slice4_tests
    unit/test_audio_main.cc
    unit/test_export.cc
    unit/test_metering.cc
    unit/test_src.cc
    unit/test_offline_render.cc
    test_common/audio_harness.cc
)

target_link_libraries(aria_slice4_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_slice4_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_slice4_tests)

# ── Timeline tests ─────────────────────────────────────────
add_executable(aria_timeline_tests
    unit/test_timeline.cc
)

target_link_libraries(aria_timeline_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_timeline_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_timeline_tests)

# ── Marker tests ───────────────────────────────────────────
add_executable(aria_marker_tests
    unit/test_markers.cc
)

target_link_libraries(aria_marker_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_marker_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_marker_tests)

# ── Mixer tests (Slice 1: Channel strip) ────────────────────────
add_executable(aria_mixer_channel_tests
    unit/test_channel.cc
)

target_link_libraries(aria_mixer_channel_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_mixer_channel_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_mixer_channel_tests)

# ── Mixer tests (Slice 1: Mixer engine + summing) ──────────────
add_executable(aria_mixer_engine_tests
    unit/test_mixer.cc
    test_common/audio_harness.cc
)

target_link_libraries(aria_mixer_engine_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_mixer_engine_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_mixer_engine_tests)

# ── Mixer tests (Slice 2: BuiltIn EQ) ──────────────────────────────
add_executable(aria_mixer_eq_tests
    unit/test_builtin_eq.cc
)

target_link_libraries(aria_mixer_eq_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_mixer_eq_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_mixer_eq_tests)

# ── Mixer tests (Slice 2: Master channel) ─────────────────────────
add_executable(aria_mixer_master_tests
    unit/test_master_channel.cc
)

target_link_libraries(aria_mixer_master_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_mixer_master_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_mixer_master_tests)

# ── Mixer tests (Slice 1: Routing, sends, sidechain) ───────────
add_executable(aria_mixer_routing_tests
    unit/test_routing.cc
)

target_link_libraries(aria_mixer_routing_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_mixer_routing_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_mixer_routing_tests)

# ── Project manager tests ──────────────────────────────────
add_executable(aria_project_tests
    unit/test_project_manager.cc
    unit/test_arrangement.cc
)

target_link_libraries(aria_project_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_project_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_project_tests)

# ── Session model tests (Phase 8b: Session C++ model) ────────────
add_executable(aria_session_tests
    unit/test_session.cc
)

target_link_libraries(aria_session_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_session_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_session_tests)

# ── Model types tests (Phase 8: TrackID, ClipID, FadeShape) ──────
add_executable(aria_model_types_tests
    unit/test_model_types.cc
)

target_link_libraries(aria_model_types_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_model_types_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_model_types_tests)

# ── Clip model tests (Phase 8: Clip base + evaluate_fade) ────────
add_executable(aria_clip_tests
    unit/test_clip.cc
)

target_link_libraries(aria_clip_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_clip_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_clip_tests)

# ── AudioClip tests (Phase 8: AudioClip + WaveformCache) ─────────
add_executable(aria_audio_clip_tests
    unit/test_audio_clip.cc
)

target_link_libraries(aria_audio_clip_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_audio_clip_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_audio_clip_tests)

# ── AutomationClipWrapper tests (Phase 8) ────────────────────────
add_executable(aria_automation_wrapper_tests
    unit/test_automation_clip_wrapper.cc
)

target_link_libraries(aria_automation_wrapper_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_automation_wrapper_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_automation_wrapper_tests)

# ── FreezeManager tests (Phase 8b: Freeze & Bounce) ──────────────
add_executable(aria_freeze_tests
    unit/test_freeze_manager.cc
)

target_link_libraries(aria_freeze_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_freeze_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_freeze_tests)

# ── Track types tests (Phase 8: Group/VCAT/Return/Master) ─────────
add_executable(aria_track_types_tests
    unit/test_track_types.cc
)

target_link_libraries(aria_track_types_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_track_types_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_track_types_tests)

# ── Piano Roll tests (Slice 1: Note model, spatial index, renderer) ──
add_executable(aria_pianoroll_tests
    unit/test_note.cc
    unit/test_note_collection.cc
    unit/test_spatial_grid.cc
    unit/test_view_transform.cc
)

target_link_libraries(aria_pianoroll_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_pianoroll_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_pianoroll_tests)

# ── Piano Roll tests (Slice 2: Tools) ──────────────────────────────
add_executable(aria_pianoroll_slice2_tests
    unit/test_tools.cc
    unit/test_selection.cc
    unit/test_snap.cc
)

target_link_libraries(aria_pianoroll_slice2_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_pianoroll_slice2_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_pianoroll_slice2_tests)

# ── Piano Roll tests (Slice 4: Scale, chords, quantize, humanize, ghosts, step input) ──
add_executable(aria_pianoroll_slice4_tests
    unit/test_scale_system.cc
    unit/test_chord_generator.cc
    unit/test_quantize.cc
    unit/test_humanize.cc
    unit/test_ghost_notes.cc
    unit/test_step_input.cc
)

target_link_libraries(aria_pianoroll_slice4_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_pianoroll_slice4_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_pianoroll_slice4_tests)

# ── Piano Roll tests (Slice 3: Velocity lane) ──────────────────────
add_executable(aria_pianoroll_velocity_tests
    unit/test_velocity_lane.cc
)

target_link_libraries(aria_pianoroll_velocity_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_pianoroll_velocity_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_pianoroll_velocity_tests)

# ── Piano Roll tests (Slice 3: Expression lanes) ──────────────────
add_executable(aria_pianoroll_expression_tests
    unit/test_expression_lane.cc
)

target_link_libraries(aria_pianoroll_expression_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_pianoroll_expression_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_pianoroll_expression_tests)

# ── Plugin sandbox tests (Phase 3) ────────────────────────────────
add_executable(aria_plugin_sandbox_tests
    unit/test_plugin_sandbox.cc
    test_common/test_plugin.cc
)

target_link_libraries(aria_plugin_sandbox_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_plugin_sandbox_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_plugin_sandbox_tests)

# ── Plugin blacklist tests (Phase 3) ──────────────────────────────
add_executable(aria_plugin_blacklist_tests
    unit/test_plugin_blacklist.cc
)

target_link_libraries(aria_plugin_blacklist_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_plugin_blacklist_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_plugin_blacklist_tests)

# ── Plugin scanner tests (Phase 4) ───────────────────────────────
add_executable(aria_plugin_scanner_tests
    unit/test_plugin_scanner.cc
)

target_link_libraries(aria_plugin_scanner_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_plugin_scanner_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_plugin_scanner_tests)

# ── Plugin tests (Phase 5 + 6: adapter, params, factory, host, native) ─
add_executable(aria_plugin_tests
    unit/test_plugin_audio_node.cc
    unit/test_plugin_params.cc
    unit/test_plugin_factory.cc
    unit/test_plugin_host.cc
    unit/test_native_clap_factory.cc
    test_common/test_plugin.cc
)

target_link_libraries(aria_plugin_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_plugin_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_plugin_tests)

# ── Plugin types tests (PR 1 foundation) ──────────────────────────
add_executable(aria_plugin_types_tests
    unit/test_plugin_types.cc
    test_common/test_plugin.cc
)

target_link_libraries(aria_plugin_types_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_plugin_types_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_plugin_types_tests)

# ── Automation engine tests (Phase 4: Engine + Build + Tests) ──
add_executable(aria_automation_tests
    unit/test_automation_points.cc
    unit/test_automation_clips.cc
    unit/test_modulation.cc
    unit/test_recorder.cc
)

target_link_libraries(aria_automation_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_automation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_automation_tests)

# ── Graphics engine tests (GPU rendering, guarded by ARIA_FEATURE_GPU) ──
if(ARIA_FEATURE_GPU)
    add_executable(aria_graphics_tests
        unit/test_graphics_engine.cc
    )

    target_link_libraries(aria_graphics_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_graphics_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_graphics_tests)

    # ── SkiaCanvas tests (PR2: GPU 2D draw primitives) ───────────
    add_executable(aria_skia_canvas_tests
        unit/test_skia_canvas.cc
    )

    target_link_libraries(aria_skia_canvas_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_skia_canvas_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_skia_canvas_tests)

    # ── RenderLoop tests (PR2: frame timing, budget, profiling) ──
    add_executable(aria_render_loop_tests
        unit/test_render_loop.cc
    )

    target_link_libraries(aria_render_loop_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_render_loop_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_render_loop_tests)

    # ── Animation tests (P10c: easing, Animator, widget integration) ──
add_executable(aria_animation_tests
    unit/test_animation.cc
)

target_link_libraries(aria_animation_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_animation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_animation_tests)

# ── Widget tests (PR2: tree, layout, hit-test, paint) ────────
    add_executable(aria_widget_tests
        unit/test_widget.cc
    )

    target_link_libraries(aria_widget_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_widget_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_widget_tests)

    # ── ContainerWidget tests (PR2: clipping, transform, scroll) ─
    add_executable(aria_widget_container_tests
        unit/test_widget_container.cc
    )

    target_link_libraries(aria_widget_container_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_widget_container_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_widget_container_tests)

    # ── FFI bridge tests (PR3: JSON → SkiaCanvas, event dispatch) ─
    add_executable(aria_ffi_bridge_tests
        unit/test_ffi_bridge.cc
    )

    target_link_libraries(aria_ffi_bridge_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_ffi_bridge_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_ffi_bridge_tests)

    # ── Browser GPU panel tests (PR3: tree node, waveform, panel) ─
    add_executable(aria_browser_gpu_tests
        unit/test_browser_panel_gpu.cc
    )

    target_link_libraries(aria_browser_gpu_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_browser_gpu_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_browser_gpu_tests)

    # ── Input system tests (P10c: FocusManager, ShortcutManager,
    #     EventDispatcher touch/pen, Widget accessibility) ─────────
    add_executable(aria_input_system_tests
        unit/test_input_system.cc
    )

    target_link_libraries(aria_input_system_tests PRIVATE
        aria_core
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_input_system_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_input_system_tests)
endif()

# ── Theme engine tests (not GPU-dependent) ──────────────────────────
add_executable(aria_theme_tests
    unit/test_theme.cc
)

target_link_libraries(aria_theme_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_theme_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_theme_tests)

# ── Integration tests: Transport sync ──────────────────────────
add_executable(aria_integration_transport_tests
    integration/test_transport_sync.cc
)

target_link_libraries(aria_integration_transport_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_integration_transport_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_integration_transport_tests)

# ── Integration tests: Freeze → unfreeze clip restoration ────
add_executable(aria_integration_freeze_tests
    integration/test_freeze_restore.cc
)

target_link_libraries(aria_integration_freeze_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_integration_freeze_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_integration_freeze_tests)

# ── Integration tests: WSOLA polymorphism ─────────────────────
add_executable(aria_integration_wsola_tests
    integration/test_wsola_poly.cc
)

target_link_libraries(aria_integration_wsola_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_integration_wsola_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

include(Catch)
catch_discover_tests(aria_integration_wsola_tests)

# ── Database integration tests (requires Rust aria-bridge staticlib) ──
# The Rust static library must be built separately via `cargo build` before
# these tests can link. If the library is not found, the target is skipped.
if(ARIA_BUILD_TESTS)
    # Try to locate the compiled Rust bridge static library
    set(RUST_TARGET_DIR "${CMAKE_SOURCE_DIR}/rust/target")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(RUST_PROFILE "release")
    else()
        set(RUST_PROFILE "debug")
    endif()

    if(WIN32)
        set(RUST_LIB_NAME "aria_bridge")
        set(RUST_LIB_SUFFIX ".lib")
    else()
        set(RUST_LIB_NAME "aria_bridge")
        set(RUST_LIB_SUFFIX ".a")
    endif()

    find_library(ARIA_BRIDGE_LIB
        NAMES ${RUST_LIB_NAME}
        PATHS "${RUST_TARGET_DIR}/${RUST_PROFILE}"
        NO_DEFAULT_PATH
    )

    if(ARIA_BRIDGE_LIB)
        message(STATUS "Found aria_bridge: ${ARIA_BRIDGE_LIB}")

        # ── Browser engine tests (P9: Engine, Search, Preview, Waveform) ──
        add_executable(aria_browser_tests
            test_browser.cpp
        )

        target_link_libraries(aria_browser_tests PRIVATE
            aria_core
            ${ARIA_BRIDGE_LIB}
            Catch2::Catch2WithMain
        )

        target_include_directories(aria_browser_tests PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/rust/include
        )

        if(WIN32)
            target_link_libraries(aria_browser_tests PRIVATE
                userenv ws2_32 advapi32
            )
        endif()

        include(Catch)
        catch_discover_tests(aria_browser_tests)

        # ── DB integration tests ────────────────────────────────────────
        add_executable(aria_db_integration_tests
            test_db_integration.cpp
        )

        target_link_libraries(aria_db_integration_tests PRIVATE
            aria_core
            ${ARIA_BRIDGE_LIB}
            Catch2::Catch2WithMain
        )

        target_include_directories(aria_db_integration_tests PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/rust/include
        )

        # Windows needs additional system libs for the Rust static lib
        if(WIN32)
            target_link_libraries(aria_db_integration_tests PRIVATE
                userenv ws2_32 advapi32
            )
        endif()

        include(Catch)
        catch_discover_tests(aria_db_integration_tests)

        # ── File watcher manual test (requires real filesystem events) ──
        add_executable(aria_watcher_manual_test
            test_watcher_manual.cpp
        )

        target_link_libraries(aria_watcher_manual_test PRIVATE
            aria_core
            ${ARIA_BRIDGE_LIB}
            Catch2::Catch2WithMain
        )

        target_include_directories(aria_watcher_manual_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/rust/include
        )

        if(WIN32)
            target_link_libraries(aria_watcher_manual_test PRIVATE
                userenv ws2_32 advapi32
            )
        endif()

        # Manual tests are NOT auto-discovered — run explicitly with:
        # ctest --preset debug -R "watcher_manual" -V
        # They require filesystem event generation on the host OS.
    else()
        message(STATUS "aria_bridge static library not found at ${RUST_TARGET_DIR}/${RUST_PROFILE}/ — skipping DB integration tests & watcher manual tests")
        message(STATUS "  Build Rust libs first: cd rust && cargo build")
    endif()
endif()

# ── File watcher tests (PR 3: platform backends + debounce) ──
# NOTE: file_watcher.cc is compiled directly here because aria_core
# has pre-existing VST3 compilation errors when building on Windows.
if(ARIA_FEATURE_SCRIPTING)
    add_executable(aria_file_watcher_tests
        scripting/test_file_watcher.cc
        ${CMAKE_SOURCE_DIR}/src/graphics/file_watcher.cc
    )

    target_link_libraries(aria_file_watcher_tests PRIVATE
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_file_watcher_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
    )

    include(Catch)
    catch_discover_tests(aria_file_watcher_tests)
endif()

# ── Scripting build tests (PR 1: Infrastructure verification) ──
if(ARIA_FEATURE_SCRIPTING)
    add_executable(aria_scripting_tests
        scripting/test_scripting_build.cc
        scripting/test_lua_runtime.cc
        scripting/test_script_instance.cc
        scripting/test_script_manager.cc
        scripting/test_api_bindings.cc
        scripting/test_action_registry.cc
        scripting/test_macro_recorder.cc
        scripting/test_script_window.cc
    )

    target_link_libraries(aria_scripting_tests PRIVATE
        aria_scripting
        lualib
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_scripting_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/src
        ${LUA_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/vendor/sol2/include
    )

    include(Catch)
    catch_discover_tests(aria_scripting_tests)
else()
    add_executable(aria_scripting_tests
        scripting/test_scripting_build.cc
    )

    target_link_libraries(aria_scripting_tests PRIVATE
        Catch2::Catch2WithMain
    )

    target_include_directories(aria_scripting_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
    )

    include(Catch)
    catch_discover_tests(aria_scripting_tests)
endif()
