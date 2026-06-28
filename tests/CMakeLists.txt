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
