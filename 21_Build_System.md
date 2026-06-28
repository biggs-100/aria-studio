# ARIA DAW — Build System Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Languages**: CMake (C++), Cargo (Rust), pnpm (TypeScript)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Build Architecture](#2-build-architecture)
3. [C++ Build (CMake)](#3-c-build-cmake)
4. [Rust Build (Cargo)](#4-rust-build-cargo)
5. [TypeScript Build (pnpm)](#5-typescript-build-pnpm)
6. [Cross-Language Integration](#6-cross-language-integration)
7. [Build Variants](#7-build-variants)
8. [Dependency Management](#8-dependency-management)
9. [CI/CD Pipeline](#9-cicd-pipeline)
10. [Packaging & Distribution](#10-packaging--distribution)
11. [Development Workflow](#11-development-workflow)
12. [API Reference](#12-api-reference)
13. [RFC Index](#13-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Build System manages compilation, linking, testing, and packaging of the ARIA DAW across three languages (C++, Rust, TypeScript) and three platforms (Windows, macOS, Linux). It is designed for fast incremental builds, reproducible releases, and developer ergonomics.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Clean build | < 15 minutes |
| Incremental build | < 30 seconds (after C++ change) |
| Cross-platform | Windows, macOS, Linux in one CMake |
| Multi-language | C++ + Rust + TypeScript in one build |
| Packaging | Installer per platform |

### 1.3. Repository Structure

```
aria-daw/
├── CMakeLists.txt              # Root CMake
├── CMakePresets.json            # CMake presets
├── Cargo.toml                   # Rust workspace root
├── package.json                 # TypeScript UI
├── pnpm-lock.yaml
│
├── cmake/                       # CMake modules
│   ├── FindSkia.cmake
│   ├── FindWebGPU.cmake
│   ├── FindLua.cmake
│   └── CompilerOptions.cmake
│
├── src/                         # C++ source
│   ├── audio/
│   ├── midi/
│   ├── dsp/
│   ├── plugin/
│   ├── graphics/
│   └── main.cc
│
├── rust/                        # Rust source
│   ├── Cargo.toml
│   └── src/
│       ├── database/
│       ├── filesystem/
│       └── networking/
│
├── ui/                          # TypeScript source
│   ├── package.json
│   ├── tsconfig.json
│   └── src/
│       ├── components/
│       ├── views/
│       └── index.tsx
│
├── scripts/                     # Build/CI scripts
│   ├── build_rust.py           # Rust build helper
│   ├── package.py              # Packaging script
│   └── ci.py                   # CI helper
│
├── tests/                       # C++ tests
│   ├── unit/
│   └── integration/
│
└── dist/                        # Build output
    ├── bin/
    ├── lib/
    └── resources/
```

---

## 2. Build Architecture

### 2.1. Language Interdependency

```
C++ (Core engine)
  │
  ├── Depends on:
  │     ├── Rust cdylib (via C ABI)
  │     ├── Skia (prebuilt)
  │     ├── WebGPU (headers + loader)
  │     ├── Lua (vendored)
  │     ├── SQLite (vendored)
  │     └── ZSTD (vendored or system)
  │
  ├── Produces:
  │     ├── aria.exe / aria.app (main binary)
  │     ├── aria_core.dll / .so / .dylib
  │     └── aria_plugins.dll (native CLAP bundle)
  │
  └── Linked with:
        ├── Rust staticlib (via C ABI)
        └── TypeScript (embedded runtime assembly)
```

### 2.2. Build Steps

```
1. Bootstrap dependencies (vcpkg / Conan / system)
2. Build Rust library → cdylib
3. Build TypeScript UI → bundled assembly
4. Configure CMake
5. Build C++ (with Rust + TS linked)
6. Run tests
7. Package installer
```

---

## 3. C++ Build (CMake)

### 3.1. Root CMakeLists

```cmake
cmake_minimum_required(VERSION 3.28)
project(aria-daw VERSION 0.1.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(ARIA_BUILD_TESTS "Build tests" ON)
option(ARIA_BUILD_TOOLS "Build developer tools" OFF)
option(ARIA_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ARIA_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ARIA_ENABLE_LTO "Enable LTO" ON)

# Compiler options
include(cmake/CompilerOptions.cmake)

# Find dependencies
include(cmake/FindSkia.cmake)
include(cmake/FindWebGPU.cmake)
include(cmake/FindLua.cmake)

# Rust library
add_subdirectory(rust)

# TypeScript UI
add_subdirectory(ui)

# Core library
add_library(aria_core
    src/audio/audio_engine.cc
    src/midi/midi_engine.cc
    src/dsp/dsp_engine.cc
    src/plugin/plugin_host.cc
    src/graphics/render_engine.cc
    # ...
)

target_link_libraries(aria_core
    PUBLIC
        aria_rust          # Rust FFI library
        aria_ui_bundle     # TypeScript bundle
        skia               # Skia graphics
        webgpu             # WebGPU
        lua                # Lua scripting
        sqlite3            # Database
        zstd               # Compression
)

# Main executable
add_executable(aria
    src/main.cc
    src/platform/windows/main_windows.cc
)

target_link_libraries(aria PRIVATE aria_core)
```

### 3.2. CMake Presets

```json
{
  "version": 8,
  "configurePresets": [
    {
      "name": "debug",
      "displayName": "Debug",
      "binaryDir": "build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "ARIA_ENABLE_ASAN": "ON",
        "ARIA_ENABLE_LTO": "OFF"
      }
    },
    {
      "name": "release",
      "displayName": "Release",
      "binaryDir": "build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "ARIA_ENABLE_LTO": "ON"
      }
    },
    {
      "name": "dist",
      "displayName": "Distribution",
      "binaryDir": "build/dist",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "ARIA_ENABLE_LTO": "ON",
        "ARIA_BUILD_TESTS": "OFF",
        "ARIA_BUILD_TOOLS": "OFF"
      }
    }
  ],
  "buildPresets": [
    { "name": "debug", "configurePreset": "debug" },
    { "name": "release", "configurePreset": "release" },
    { "name": "dist", "configurePreset": "dist" }
  ]
}
```

### 3.3. Compiler Options

```cmake
# cmake/CompilerOptions.cmake
if(MSVC)
    add_compile_options(
        /W4
        /WX                    # Warnings as errors
        /utf-8
        /O2                    # Fast code
        /arch:AVX2
        /fp:fast
        /GL                    # Whole program optimization
        /EHsc                  # C++ exceptions (not SEH)
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Werror
        -march=native
        -ffast-math
        -fvisibility=hidden
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Werror
        -march=native
        -ffast-math
        -fvisibility=hidden
        -fno-omit-frame-pointer
    )
endif()
```

---

## 4. Rust Build (Cargo)

### 4.1. Cargo.toml

```toml
[workspace]
members = ["rust/aria-database", "rust/aria-filesystem", "rust/aria-networking"]

[package]
name = "aria-rust-core"
version = "0.1.0"
edition = "2021"

[lib]
crate-type = ["cdylib", "staticlib"]
name = "aria_rust"

[dependencies]
rusqlite = { version = "0.31", features = ["bundled", "fts5"] }
serde = { version = "1", features = ["derive"] }
serde_json = "1"
zstd = "0.13"
sha2 = "0.10"
walkdir = "2"

[profile.release]
lto = true
codegen-units = 1
opt-level = 3
strip = true
```

### 4.2. C ABI Export

```rust
// Rust exports a C-compatible ABI:
#[no_mangle]
pub extern "C" fn aria_db_open(path: *const c_char) -> *mut DbConnection {
    let path = unsafe { CStr::from_ptr(path) }.to_str().unwrap();
    match DbConnection::open(path) {
        Ok(conn) => Box::into_raw(Box::new(conn)),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn aria_db_search_samples(
    conn: *mut DbConnection,
    query: *const c_char,
    results: *mut SampleResult,
    max_results: u32,
) -> u32 {
    // Implementation
}
```

---

## 5. TypeScript Build (pnpm)

### 5.1. package.json

```json
{
  "name": "aria-ui",
  "private": true,
  "scripts": {
    "dev": "tsc --watch",
    "build": "tsc && esbuild src/index.ts --bundle --outfile=dist/aria_ui.js",
    "typecheck": "tsc --noEmit",
    "test": "vitest run",
    "lint": "eslint src/"
  },
  "devDependencies": {
    "typescript": "^5.4",
    "esbuild": "^0.20",
    "vitest": "^1.6",
    "eslint": "^8.57"
  }
}
```

### 5.2. Build Integration

```cmake
# CMake integrates TypeScript build:
add_custom_target(aria_ui_build
    COMMAND ${PNPM_CMD} build
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/ui
    BYPRODUCTS ${CMAKE_SOURCE_DIR}/ui/dist/aria_ui.js
)

add_library(aria_ui_bundle STATIC
    ${CMAKE_SOURCE_DIR}/ui/dist/aria_ui.js
)

add_dependencies(aria_ui_bundle aria_ui_build)
target_link_libraries(aria_core PUBLIC aria_ui_bundle)
```

---

## 6. Cross-Language Integration

### 6.1. Build Order

```
1. Rust build:      cargo build --release
                    → libaria_rust.a (static) / aria_rust.dll (dynamic)

2. TypeScript build: pnpm build
                    → aria_ui.js (bundled)

3. C++ CMake:       cmake --build . --config Release
                    → Links Rust staticlib + embeds TS bundle
                    → Outputs aria.exe / aria.app
```

### 6.2. Dependency Vendoring

```cmake
# Vendored dependencies (committed to repo):
# - Lua 5.4.6 (vendored/lua/)
# - SQLite 3.45 (vendored/sqlite/)
# - ZSTD 1.5.5 (vendored/zstd/)
# - STB libraries (vendored/stb/)
#
# External dependencies (fetched by CMake):
# - Skia (prebuilt binaries per platform)
# - WebGPU (Dawn or wgpu-native headers)
#
# System dependencies (optional):
# - ASIO SDK (Windows, user-provided)
# - CoreAudio (macOS, system)
# - ALSA (Linux, system package)
```

---

## 7. Build Variants

### 7.1. Build Types

| Variant | Optimization | Debug | LTO | ASAN | Use Case |
|---|---|---|---|---|---|
| **Debug** | -O0 -g | Full | Off | Optional | Development |
| **RelWithDebInfo** | -O2 -g | Full | On | Off | Profiling |
| **Release** | -O3 | None | On | Off | Local testing |
| **Distribution** | -O3 + PGO | Strip | On | Off | Public release |
| **Sanitized** | -O1 -g | Full | Off | On | Bug hunting |

### 7.2. Feature Flags

```cmake
# CMake options for feature selection:
option(ARIA_FEATURE_AI "Enable AI features" ON)
option(ARIA_FEATURE_NETWORKING "Enable networking" ON)
option(ARIA_FEATURE_SCRIPTING "Enable Lua scripting" ON)
option(ARIA_FEATURE_VST3 "Enable VST3 support" ON)
option(ARIA_FEATURE_AU "Enable Audio Unit support" OFF)
option(ARIA_FEATURE_LV2 "Enable LV2 support" OFF)
option(ARIA_FEATURE_GPU "Enable GPU rendering" ON)
```

---

## 8. Dependency Management

### 8.1. Dependency Table

| Dependency | Version | License | Sourced | Used For |
|---|---|---|---|---|
| Skia | M125 | BSD 3 | Prebuilt binary | GPU 2D rendering |
| WebGPU (Dawn) | HEAD | Apache 2 | Git submodule | GPU abstraction |
| Lua | 5.4.6 | MIT | Vendored | Scripting |
| SQLite | 3.45 | Public Domain | Vendored | Database |
| ZSTD | 1.5.5 | BSD | Vendored | Compression |
| Catch2 | 3.x | BSL | CMake Fetch | Testing |
| fmt | 10.x | MIT | CMake Fetch | Logging |
| spdlog | 1.13 | MIT | CMake Fetch | Logging |
| glad | 2.x | MIT | Vendored | OpenGL (fallback) |

### 8.2. Update Policy

```
Dependency update policy:
  - Patch updates: Auto (Dependabot/Renovate)
  - Minor updates: Reviewed, applied within 2 weeks
  - Major updates: Evaluated, migration plan required
  - Security fixes: Applied within 24 hours
  
  Breaking dependency changes:
  - Must not break API stability
  - Must maintain backwards compatibility
  - Migration path must be documented
```

---

## 9. CI/CD Pipeline

### 9.1. Pipeline Stages

```yaml
name: ARIA CI

on: [push, pull_request]

jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - run: cmake --build build --target lint
      - run: cargo clippy --all-targets
      - run: cd ui && npx eslint src/
      - run: cd ui && npx tsc --noEmit

  build:
    strategy:
      matrix:
        os: [windows-latest, macos-latest, ubuntu-latest]
        config: [debug, release]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
      - uses: actions/setup-node@v4
      - uses: actions-rust-lang/setup-rust-toolchain@v1
      
      - name: Configure CMake
        run: cmake --preset ${{ matrix.config }}
        
      - name: Build
        run: cmake --build --preset ${{ matrix.config }}
        
      - name: Test
        run: ctest --preset ${{ matrix.config }}

  package:
    needs: build
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [windows-latest, macos-latest, ubuntu-latest]
    steps:
      - name: Package
        run: python scripts/package.py --config release
      - uses: actions/upload-artifact@v4
        with:
          name: aria-${{ matrix.os }}-${{ github.sha }}
          path: dist/
```

### 9.2. Nightly Builds

```yaml
name: Nightly Build

on:
  schedule:
    - cron: '0 6 * * *'  # 6 AM UTC daily

jobs:
  nightly:
    runs-on: [windows-latest, macos-latest, ubuntu-latest]
    steps:
      - run: cmake --preset release
      - run: cmake --build --preset release
      - run: python scripts/run_nightly_tests.py
      - run: python scripts/package.py --version nightly
```

---

## 10. Packaging & Distribution

### 10.1. Package Structure

```
Windows (NSIS installer):
  ARIA DAW/
  ├── aria.exe
  ├── aria_core.dll
  ├── aria_rust.dll
  ├── aria_plugins.dll
  ├── resources/
  │   ├── themes/
  │   ├── scripts/
  │   └── icons/
  └── runtime/
      ├── vc_redist.x64.exe
      └── webgpu.dll

macOS (.app bundle):
  ARIA DAW.app/
  ├── Contents/
  │   ├── Info.plist
  │   ├── MacOS/
  │   │   └── aria
  │   └── Resources/
  │       ├── aria.icns
  │       ├── themes/
  │       ├── scripts/
  │       └── Frameworks/
  │           ├── libaria_rust.dylib
  │           └── libwebgpu.dylib

Linux (AppImage):
  ARIA_DAW-x86_64.AppImage
```

### 10.2. Versioning

```
ARIA DAW follows Semantic Versioning:
  MAJOR.MINOR.PATCH (e.g., 1.4.2)

  MAJOR: Breaking UI/API/project format changes
  MINOR: New features, backwards compatible
  PATCH: Bug fixes, no new features

  Pre-release suffixes:
    -alpha.1, -beta.2, -rc.3
  
  Build metadata:
    +sha.abcdef1, +nightly.20240627
```

---

## 11. Development Workflow

### 11.1. Common Commands

```bash
# First time setup
git clone https://github.com/biggs-100/aria-studio.git
cd aria-studio
python scripts/setup_dev.py    # Install dependencies

# Build
cmake --preset debug
cmake --build --preset debug

# Run
./build/debug/aria/aria

# Test
ctest --preset debug

# Rust only
cd rust && cargo build

# UI only
cd ui && pnpm dev
```

### 11.2. Docker Dev Environment

```dockerfile
# Dockerfile for dev environment (Linux)
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build \
    clang-17 lld-17 \
    libasound2-dev libpulse-dev \
    python3 nodejs npm \
    curl git

# Install Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

# Install pnpm
RUN npm install -g pnpm

WORKDIR /workspace
```

---

## 12. API Reference

### 12.1. Public API

```cpp
class BuildSystem {
public:
    // Build configuration
    static bool configure(const std::string& preset);
    static bool build(const std::string& preset);
    static bool test(const std::string& preset);
    static bool package(const std::string& config);
    
    // Version info
    static std::string version();
    static std::string build_type();
    static std::string commit_hash();
    static std::string build_timestamp();
    
    // Feature detection
    static bool has_feature(const std::string& feature);
    static std::vector<std::string> enabled_features();
};
```

---

## 13. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-BLD-001 | CMake Architecture & Presets | 🔲 Pending |
| RFC-BLD-002 | C++ Compiler Configuration | 🔲 Pending |
| RFC-BLD-003 | Rust Cargo Workspace | 🔲 Pending |
| RFC-BLD-004 | TypeScript Build (esbuild) | 🔲 Pending |
| RFC-BLD-005 | Cross-Language Build Order | 🔲 Pending |
| RFC-BLD-006 | Dependency Vendoring Strategy | 🔲 Pending |
| RFC-BLD-007 | Build Variants & Feature Flags | 🔲 Pending |
| RFC-BLD-008 | CI Pipeline (GitHub Actions) | 🔲 Pending |
| RFC-BLD-009 | Nightly Builds | 🔲 Pending |
| RFC-BLD-010 | Windows Packaging (NSIS) | 🔲 Pending |
| RFC-BLD-011 | macOS Packaging (.app bundle) | 🔲 Pending |
| RFC-BLD-012 | Linux Packaging (AppImage) | 🔲 Pending |
| RFC-BLD-013 | PGO (Profile-Guided Optimization) | 🔲 Pending |
| RFC-BLD-014 | Incremental Build Optimization | 🔲 Pending |
| RFC-BLD-015 | Developer Setup Script | 🔲 Pending |
| RFC-BLD-016 | Docker Dev Environment | 🔲 Pending |

---

## Appendix A: Build Times

| Step | Debug | Release | Distribution |
|---|---|---|---|
| CMake configure | 10s | 10s | 10s |
| Rust build | 45s | 120s | 120s |
| TypeScript build | 5s | 10s | 10s |
| C++ compile | 120s | 300s | 300s |
| C++ link | 10s | 30s | 45s |
| **Total incremental** | **~30s** | **~60s** | **~60s** |
| **Total clean** | **~3min** | **~8min** | **~8min** |

## Appendix B: Supported Compilers

| Compiler | Min Version | Tested | Notes |
|---|---|---|---|
| MSVC | 2022 17.8 | ✅ | Windows primary |
| Clang | 17 | ✅ | macOS, Linux primary |
| GCC | 13 | ✅ | Linux secondary |
| Apple Clang | 15 | ✅ | macOS (Xcode 15+) |

## Appendix C: Build System Dependencies

```
Required tools:
  - CMake ≥ 3.28
  - Ninja ≥ 1.11 (recommended) or Make
  - Python ≥ 3.11
  - pnpm ≥ 8
  - Rust/Cargo ≥ 1.75
  - Git ≥ 2.40

Optional tools:
  - clang-tidy (linting)
  - clang-format (formatting)
  - cppcheck (static analysis)
  - doxygen (documentation)
  - gperftools (profiling)
  - valgrind (Linux, memory checking)
```
