# ── Scripting Library ──────────────────────────────────────────
# Isolated STATIC library to prevent sol2 template bloat from
# triggering full rebuilds of aria_core (45+ .cc files).
#
# NOTE: MSVC does not transitively propagate STATIC library
# dependencies. Downstream consumers (tests) must explicitly link
# lualib + sol2 + nlohmann_json as needed.

add_library(aria_scripting STATIC
    lua_runtime.cc
    script_instance.cc
    script_manager.cc
    api_bindings.cc
    macro_recorder.cc
    script_window.cc
)

target_include_directories(aria_scripting PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LUA_INCLUDE_DIR}
)

target_compile_definitions(aria_scripting PUBLIC
    ARIA_FEATURE_SCRIPTING
)

target_link_libraries(aria_scripting PUBLIC
    ${LUA_LIBRARIES}
    sol2
    nlohmann_json
)
