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
if(EXISTS "${CMAKE_SOURCE_DIR}/rust/Cargo.toml")
    set(RUST_PROJECT_DIR "${CMAKE_SOURCE_DIR}/rust")
    set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/rust")

    if(WIN32)
        set(RUST_LIB_NAME "aria_rust.dll")
        set(RUST_LIB_TYPE "SHARED")
    elseif(APPLE)
        set(RUST_LIB_NAME "libaria_rust.dylib")
        set(RUST_LIB_TYPE "SHARED")
    else()
        set(RUST_LIB_NAME "libaria_rust.so")
        set(RUST_LIB_TYPE "SHARED")
    endif()

    add_custom_target(rust_build ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Building Rust library..."
        COMMAND cargo build --manifest-path "${RUST_PROJECT_DIR}/Cargo.toml"
                ${CMAKE_BUILD_TYPE}
                --target-dir "${RUST_TARGET_DIR}"
        BYPRODUCTS "${RUST_TARGET_DIR}/release/${RUST_LIB_NAME}"
        COMMENT "Building ARIA Rust library"
    )

    add_library(aria_rust ${RUST_LIB_TYPE} IMPORTED GLOBAL)
    set_target_properties(aria_rust PROPERTIES
        IMPORTED_LOCATION "${RUST_TARGET_DIR}/release/${RUST_LIB_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${RUST_PROJECT_DIR}/include"
    )
    add_dependencies(aria_rust rust_build)
endif()

# ── TypeScript UI ─────────────────────────────────────────────
if(EXISTS "${CMAKE_SOURCE_DIR}/ui/package.json")
    add_custom_target(ts_ui_build ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Building TypeScript UI..."
        COMMAND pnpm install --frozen-lockfile
        COMMAND pnpm build
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/ui"
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/ui/dist/aria_ui.js"
        COMMENT "Building ARIA TypeScript UI"
    )

    add_library(aria_ui_bundle INTERFACE)
    target_include_directories(aria_ui_bundle INTERFACE
        "${CMAKE_SOURCE_DIR}/ui/dist"
    )
    add_dependencies(aria_ui_bundle ts_ui_build)
endif()

# ── Core Library ──────────────────────────────────────────────
add_library(aria_core STATIC
    src/kernel/application.cc
    src/kernel/service_locator.cc
    src/kernel/event_bus.cc
    src/kernel/command_queue.cc
    src/kernel/undo_stack.cc
    src/kernel/crash_handler.cc
)

target_include_directories(aria_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/kernel
)

target_link_libraries(aria_core PUBLIC
    aria_rust
    aria_ui_bundle
    ${SKIA_LIBRARIES}
    ${WEBGPU_LIBRARIES}
    ${LUA_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${ZSTD_LIBRARIES}
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
