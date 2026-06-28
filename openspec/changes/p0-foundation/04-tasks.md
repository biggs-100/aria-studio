# P0 - Foundation: Tasks

> **Status**: Proposed
> **Priority**: P0 (Required for v1)

---

## T-001: Initialize Git repository

**Status**: 🔲 Pending
**Effort**: Small
**Dependencies**: None

### Description
Set up version control for the project.

### Acceptance Criteria
- [ ] `git init` run in project root
- [ ] `.gitignore` created (C++, Rust, Node, OS files, build outputs)
- [ ] `.gitattributes` created (LF normalization, diff config)
- [ ] Initial commit with existing documentation files
- [ ] Remote `origin` configured

### Files
- `.gitignore`
- `.gitattributes`

---

## T-002: Create CMake build system

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-001
**Reference**: `21_Build_System.md` §3

### Description
Create the root CMake build system with presets for Debug, Release, and Distribution. Configure compiler options, C++23 standard, and link all sub-projects.

### Acceptance Criteria
- [ ] `CMakeLists.txt` at root with project definition
- [ ] `CMakePresets.json` with debug, release, dist presets
- [ ] `cmake/CompilerOptions.cmake` with MSVC/Clang/GCC flags
- [ ] CMake finds Skia, WebGPU, Lua, SQLite, ZSTD dependencies
- [ ] `cmake --build --preset debug` succeeds
- [ ] `cmake --build --preset release` succeeds

### Files
- `CMakeLists.txt`
- `CMakePresets.json`
- `cmake/CompilerOptions.cmake`
- `cmake/FindSkia.cmake`
- `cmake/FindWebGPU.cmake`
- `cmake/FindLua.cmake`

---

## T-003: Create Rust workspace and C ABI library

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-001
**Reference**: `21_Build_System.md` §4

### Description
Set up the Rust workspace with database, filesystem, and networking crates. Export a `cdylib` with C ABI functions that C++ can link against.

### Acceptance Criteria
- [ ] `Cargo.toml` workspace with 3 members (database, filesystem, networking)
- [ ] Each crate compiles (`cargo build`)
- [ ] `cdylib` target produces `.dll`/`.so`/`.dylib`
- [ ] At least one `extern "C"` function exported (e.g., `aria_db_open`)
- [ ] CMake finds and links the Rust library

### Files
- `Cargo.toml`
- `rust/aria-database/Cargo.toml`
- `rust/aria-database/src/lib.rs`
- `rust/aria-filesystem/Cargo.toml`
- `rust/aria-filesystem/src/lib.rs`
- `rust/aria-networking/Cargo.toml`
- `rust/aria-networking/src/lib.rs`

---

## T-004: Set up TypeScript UI project

**Status**: 🔲 Pending
**Effort**: Small
**Dependencies**: T-001
**Reference**: `21_Build_System.md` §5

### Description
Initialize the TypeScript UI project with pnpm, esbuild bundling, and type checking.

### Acceptance Criteria
- [ ] `ui/package.json` with dependencies
- [ ] `ui/tsconfig.json` for type checking
- [ ] `esbuild` configured to bundle to `dist/aria_ui.js`
- [ ] `pnpm build` produces the bundle
- [ ] CMake target `aria_ui_bundle` depends on the build
- [ ] Basic TypeScript type check passes

### Files
- `ui/package.json`
- `ui/tsconfig.json`
- `ui/src/index.ts` (entry point)

---

## T-005: Create project directory structure with module stubs

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-002, T-003, T-004
**Reference**: `03_System_Architecture.md` §3.1

### Description
Create the full directory structure as specified in the architecture document. For each module, create a minimal C++ header/source pair that compiles. Create the C++ ↔ Rust FFI skeleton.

### Acceptance Criteria
- [ ] All module directories exist per architecture spec (audio, midi, dsp, plugin, automation, timeline, project, model, graphics, browser, scripting, ai, networking, database, platform, kernel)
- [ ] Each C++ module has a header stub (class declaration + basic API)
- [ ] Each C++ module has a source stub (empty implementation that compiles)
- [ ] Rust FFI header (`aria_rust.h`) with `extern "C"` declarations
- [ ] C++ side includes and calls Rust FFI
- [ ] Full build with all stubs succeeds

### Files
- `src/audio/audio_engine.h`, `src/audio/audio_engine.cc`
- `src/midi/midi_engine.h`, `src/midi/midi_engine.cc`
- ... (all modules per architecture)
- `src/aria_rust.h` (Rust FFI header)

---

## T-006: Implement core kernel

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-005
**Reference**: `03_System_Architecture.md` §4.3

### Description
Implement the kernel module: application lifecycle, service locator, typed event bus, command queue, and undo stack.

### Acceptance Criteria
- [ ] `Application` class with init/run/shutdown lifecycle
- [ ] `ServiceLocator` with explicit registration and lookup
- [ ] `EventBus` typed with publish/subscribe (both sync and async dispatch)
- [ ] `CommandQueue` with future-based dispatch
- [ ] `UndoStack` with push/undo/redo/snapshot
- [ ] `CrashHandler` that catches signals/exceptions and saves recovery state
- [ ] Unit tests for each component
- [ ] Application boots and runs a trivial event loop

### Files
- `src/kernel/application.h`, `src/kernel/application.cc`
- `src/kernel/service_locator.h`, `src/kernel/service_locator.cc`
- `src/kernel/event_bus.h`, `src/kernel/event_bus.cc`
- `src/kernel/command_queue.h`, `src/kernel/command_queue.cc`
- `src/kernel/undo_stack.h`, `src/kernel/undo_stack.cc`
- `src/kernel/crash_handler.h`, `src/kernel/crash_handler.cc`
- `src/main.cc`

---

## T-007: Set up testing infrastructure

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-005
**Reference**: `20_Testing.md` §3, §5

### Description
Configure Catch2 test framework, create the audio test harness, implement minimal test plugins, and verify the testing pipeline.

### Acceptance Criteria
- [ ] Catch2 v3 integrated via CMake FetchContent
- [ ] Test build target (`aria_tests`)
- [ ] At least one test per kernel module passing
- [ ] `AudioTestHarness` class with sine/impulse/noise generation
- [ ] `ctest` integration (all tests run via `ctest --preset debug`)
- [ ] Test suite runs in CI (simulate with local run)

### Files
- `tests/CMakeLists.txt`
- `tests/unit/test_kernel.cc`
- `tests/unit/test_event_bus.cc`
- `tests/test_common/audio_harness.h`, `tests/test_common/audio_harness.cc`

---

## T-008: Create developer environment setup

**Status**: 🔲 Pending
**Effort**: Small
**Dependencies**: T-002, T-003, T-004

### Description
Create scripts and configs for developer onboarding: dependency installation, Docker dev environment, and IDE configuration.

### Acceptance Criteria
- [ ] `scripts/setup_dev.py` installs all dependencies
- [ ] Dockerfile for Linux dev environment
- [ ] `.vscode/tasks.json` with common build/test tasks
- [ ] `.vscode/launch.json` with debug configuration
- [ ] `.vscode/c_cpp_properties.json` with intellisense config
- [ ] `README.md` with build instructions

### Files
- `scripts/setup_dev.py`
- `Dockerfile`
- `.vscode/tasks.json`
- `.vscode/launch.json`
- `.vscode/c_cpp_properties.json`
- `README.md`

---

## T-009: Configure CI pipeline

**Status**: 🔲 Pending
**Effort**: Small
**Dependencies**: T-002, T-003, T-004, T-007
**Reference**: `21_Build_System.md` §9

### Description
Create GitHub Actions workflow for CI: lint, build, test across all three platforms.

### Acceptance Criteria
- [ ] `.github/workflows/ci.yml` triggers on push/PR
- [ ] Matrix build for Windows, macOS, Linux
- [ ] Lint step: clang-tidy, clippy, eslint, tsc
- [ ] Build step: cmake --build debug + release
- [ ] Test step: ctest
- [ ] Pipeline passes on initial run

### Files
- `.github/workflows/ci.yml`

---

## Task Dependencies

```
T-001 (Git)
  ├── T-002 (CMake)
  │   ├── T-005 (Stubs)
  │   │   ├── T-006 (Kernel)
  │   │   ├── T-007 (Testing)
  │   │   │   └── T-009 (CI)
  │   │   └── T-008 (Dev Env)
  │   └── T-004 (TypeScript)
  └── T-003 (Rust)
      └── T-005 (Stubs)

Legend:
  ── depends on
  ── can be parallel
```

---

## Review Workload Forecast

- **Estimated changed lines**: ~3,500 (headers, sources, configs, scripts)
- **Files changed**: ~60
- **400-line budget risk**: High — this change exceeds the 800-line review budget
- **Decision needed before apply**: Yes — this is a foundation change, not a feature. It needs to be committed as a single coherent unit.
