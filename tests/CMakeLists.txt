# ── Test Build ────────────────────────────────────────────

# Catch2 is fetched by FindDependencies.cmake
find_package(Catch2 REQUIRED)

# Test executable
add_executable(aria_tests
    test_main.cc
    unit/test_kernel.cc
    unit/test_event_bus.cc
    unit/test_undo_stack.cc
    unit/test_track.cc
    unit/test_project.cc
    test_common/audio_harness.cc
)

target_link_libraries(aria_tests PRIVATE
    aria_core
    Catch2::Catch2WithMain
)

target_include_directories(aria_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/src
)

# Register tests
include(Catch)
catch_discover_tests(aria_tests)
