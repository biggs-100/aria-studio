# ── Dependency Discovery ─────────────────────────────────────

# Skia
if(ARIA_FEATURE_GPU)
    find_package(Skia QUIET)
    if(NOT Skia_FOUND)
        message(STATUS "Skia not found via find_package, using custom path")
        set(SKIA_DIR "" CACHE PATH "Path to Skia installation")
        if(SKIA_DIR)
            set(SKIA_LIBRARIES "${SKIA_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}skia${CMAKE_STATIC_LIBRARY_SUFFIX}")
            set(SKIA_INCLUDE_DIRS "${SKIA_DIR}/include")
            mark_as_advanced(SKIA_DIR)
        else()
            message(WARNING "Skia not found. GPU rendering will not be available. "
                            "Set SKIA_DIR or install Skia via vcpkg.")
        endif()
    else()
        set(SKIA_LIBRARIES Skia::skia)
    endif()
endif()

# WebGPU (Dawn or wgpu-native)
if(ARIA_FEATURE_GPU)
    find_package(webgpu QUIET)
    if(NOT webgpu_FOUND)
        message(STATUS "WebGPU not found via find_package")
        set(WEBGPU_DIR "" CACHE PATH "Path to WebGPU (Dawn/wgpu) installation")
        if(WEBGPU_DIR)
            set(WEBGPU_LIBRARIES "${WEBGPU_DIR}/lib/webgpu${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set(WEBGPU_INCLUDE_DIRS "${WEBGPU_DIR}/include")
            mark_as_advanced(WEBGPU_DIR)
        else()
            message(WARNING "WebGPU not found. Set WEBGPU_DIR or install via vcpkg.")
        endif()
    else()
        set(WEBGPU_LIBRARIES webgpu::webgpu)
    endif()
endif()

# Lua 5.4 (only when scripting is enabled)
if(ARIA_FEATURE_SCRIPTING)
    find_package(Lua 5.4 QUIET)
    if(NOT Lua_FOUND)
        # Try vendored Lua
        if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/lua")
            message(STATUS "Using vendored Lua")
            set(LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vendor/lua/include")
            set(LUA_LIBRARIES "${CMAKE_SOURCE_DIR}/vendor/lua/${CMAKE_STATIC_LIBRARY_PREFIX}lua${CMAKE_STATIC_LIBRARY_SUFFIX}")
        else()
            message(FATAL_ERROR "Lua 5.4 not found. Install Lua or add to vendor/")
        endif()
    else()
        set(LUA_LIBRARIES Lua::lua)
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

# fmt (logging)
include(FetchContent)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt)

# spdlog (logging)
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)
FetchContent_MakeAvailable(spdlog)

message(STATUS "ARIA DAW Build Configuration:")
message(STATUS "  C++ Standard:    23")
message(STATUS "  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Skia:            ${SKIA_FOUND}")
message(STATUS "  WebGPU:          ${WEBGPU_FOUND}")
message(STATUS "  Lua:             ${Lua_FOUND}")
message(STATUS "  SQLite:          ${SQLite3_FOUND}")
message(STATUS "  ZSTD:            ${zstd_FOUND}")
message(STATUS "  Tests:           ${ARIA_BUILD_TESTS}")
