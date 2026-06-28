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

# ── Full test suite (requires model, timeline, etc.) ──────
# Disabled until all component libraries are built.
# add_executable(aria_tests ...)
