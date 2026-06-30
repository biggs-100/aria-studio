# ── Dependency Discovery ─────────────────────────────────────

# Dawn WebGPU (Google) / Skia GPU — FetchContent from source
# See: cmake/FindDawn.cmake and cmake/FindSkiaFetch.cmake
if(ARIA_FEATURE_GPU)
    include(cmake/FindDawn.cmake)
    include(cmake/FindSkiaFetch.cmake)
endif()

# Lua 5.4 (only when scripting is enabled)
if(ARIA_FEATURE_SCRIPTING)
    find_package(Lua 5.4 QUIET)
    if(NOT Lua_FOUND)
        # Build vendored Lua 5.4.7 from source
        if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/lua/CMakeLists.txt")
            message(STATUS "Using vendored Lua (built from source)")
            add_subdirectory("${CMAKE_SOURCE_DIR}/vendor/lua" "${CMAKE_BINARY_DIR}/vendor/lua")
            set(LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vendor/lua/include")
            set(LUA_LIBRARIES lualib)
        else()
            message(FATAL_ERROR "Lua 5.4 not found. Install Lua or add to vendor/")
        endif()
    else()
        set(LUA_LIBRARIES Lua::lua)
    endif()
endif()

# sol2 (header-only, for Lua C++ bindings)
if(ARIA_FEATURE_SCRIPTING)
    if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/sol2/include/sol/sol.hpp")
        message(STATUS "sol2: using vendored headers from vendor/sol2/")
        add_library(sol2 INTERFACE)
        target_include_directories(sol2 INTERFACE "${CMAKE_SOURCE_DIR}/vendor/sol2/include")
    else()
        message(WARNING "sol2 not found in vendor/sol2/ — scripting tests will not compile")
        add_library(sol2 INTERFACE)
    endif()
endif()

# SQLite 3
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    set(SQLITE_LIBRARIES SQLite::SQLite3)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/vendor/sqlite/sqlite3.c")
    message(STATUS "Using vendored SQLite")
    set(SQLITE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vendor/sqlite")
    add_library(sqlite3 STATIC "${CMAKE_SOURCE_DIR}/vendor/sqlite/sqlite3.c")
    target_include_directories(sqlite3 PUBLIC "${CMAKE_SOURCE_DIR}/vendor/sqlite")
    set(SQLITE_LIBRARIES sqlite3)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/vendor/sqlite")
    message(STATUS "Using vendored SQLite (headers, no source)")
    set(SQLITE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vendor/sqlite")
    set(SQLITE_LIBRARIES "")
else()
    message(WARNING "SQLite3 not found. Install SQLite or add to vendor/")
    set(SQLITE_LIBRARIES "")
endif()

# ZSTD
find_package(zstd QUIET)
if(NOT zstd_FOUND)
    if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/zstd")
        message(STATUS "Using vendored ZSTD")
        set(ZSTD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vendor/zstd/include")
        set(ZSTD_LIBRARIES "${CMAKE_SOURCE_DIR}/vendor/zstd/${CMAKE_STATIC_LIBRARY_PREFIX}zstd${CMAKE_STATIC_LIBRARY_SUFFIX}")
    else()
        message(WARNING "ZSTD not found. Install ZSTD or add to vendor/")
    endif()
else()
    set(ZSTD_LIBRARIES zstd::zstd)
endif()

# Catch2 (for tests)
if(ARIA_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.0
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

# ImGui (provisional browser UI, refined in P10)
# ImGui is optional. Set IMGUI_DIR to the path containing imgui.h and
# the ARIA_FEATURE_IMGUI flag will be enabled automatically.
# Example: cmake -DIMGUI_DIR=C:/path/to/imgui ..
if(DEFINED IMGUI_DIR AND EXISTS "${IMGUI_DIR}/imgui.h")
    add_library(aria_imgui INTERFACE)
    target_include_directories(aria_imgui INTERFACE "${IMGUI_DIR}")
    target_compile_definitions(aria_imgui INTERFACE ARIA_FEATURE_IMGUI=1)
    message(STATUS "ImGui: found at ${IMGUI_DIR}")
else()
    # ImGui not available — browser_panel.cc will compile as empty stubs.
    # Refined browser UI in P10 may add a FetchContent or vcpkg dependency.
    add_library(aria_imgui INTERFACE)
    message(STATUS "ImGui: not found — browser panel disabled (set IMGUI_DIR to enable)")
endif()

# nlohmann_json (JSON serialization) — vendored in vendor/json/ takes precedence
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/json/include/nlohmann/json.hpp")
    message(STATUS "nlohmann_json: using vendored headers from vendor/json/")
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE "${CMAKE_SOURCE_DIR}/vendor/json/include")
else()
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# fmt (logging) — vendored in vendor/fmt/ takes precedence
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/fmt/include/fmt/core.h")
    message(STATUS "fmt: using vendored headers from vendor/fmt/")
    add_library(fmt INTERFACE)
    target_include_directories(fmt INTERFACE "${CMAKE_SOURCE_DIR}/vendor/fmt/include")
else()
    include(FetchContent)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.2.1
    )
    FetchContent_MakeAvailable(fmt)
endif()

# spdlog (logging) — vendored in vendor/spdlog/ takes precedence
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/spdlog/include/spdlog/spdlog.h")
    message(STATUS "spdlog: using vendored headers from vendor/spdlog/")
    add_library(spdlog INTERFACE)
    target_include_directories(spdlog INTERFACE "${CMAKE_SOURCE_DIR}/vendor/spdlog/include")
else()
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# CLAP (header-only C API, optional — vendored headers in vendor/clap/ take precedence)
# FetchContent provides the canonical CLAP headers from the upstream repository.
# The vendored headers under vendor/clap/include/ are used by default.
# Set ARIA_USE_FETCHCONTENT_CLAP=ON to override with upstream headers.
option(ARIA_USE_FETCHCONTENT_CLAP "Fetch CLAP headers from upstream via FetchContent" OFF)
if(ARIA_USE_FETCHCONTENT_CLAP)
    FetchContent_Declare(
        clap
        GIT_REPOSITORY https://github.com/free-audio/clap.git
        GIT_TAG v1.2.2
    )
    FetchContent_GetProperties(clap)
    if(NOT clap_POPULATED)
        FetchContent_Populate(clap)
    endif()
    set(CLAP_INCLUDE_DIR ${clap_SOURCE_DIR}/include)
    message(STATUS "CLAP: using upstream headers from ${CLAP_INCLUDE_DIR}")
else()
    # Prefer vendored headers
    set(CLAP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vendor/clap/include")
    message(STATUS "CLAP: using vendored headers from ${CLAP_INCLUDE_DIR}")
endif()

message(STATUS "ARIA DAW Build Configuration:")
message(STATUS "  C++ Standard:    23")
message(STATUS "  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Dawn:            ${Dawn_FOUND}")
message(STATUS "  Skia:            ${Skia_FOUND}")
message(STATUS "  Lua:             ${Lua_FOUND}")
message(STATUS "  SQLite:          ${SQLite3_FOUND}")
message(STATUS "  ZSTD:            ${zstd_FOUND}")
message(STATUS "  Tests:           ${ARIA_BUILD_TESTS}")
