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
