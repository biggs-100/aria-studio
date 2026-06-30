# Tasks: P11 — Scripting API (Lua + Macros)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~1800–2500 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 → PR 2 → PR 3 → PR 4 |
| Delivery strategy | ask-on-risk |
| Chain strategy | stacked-to-main |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Infrastructure — Lua 5.4 vendor + sol2 + CMake | PR 1 | Base branch: main. Vendored deps only, no runtime code. |
| 2 | Runtime + Core API Bindings | PR 2 | Depends on PR 1. LuaRuntime, ScriptInstance, ScriptManager, sol2 bindings for transport/track/clip/timing. |
| 3 | Macro Recorder + File Watcher completion | PR 3 | Depends on PR 2. ActionRegistry, MacroRecorder, Win32/inotify/FSEvents platform backends. |
| 4 | Script UI | PR 4 | Depends on PR 3. ScriptWindow proxy, widget API, Lua callback dispatch. |

## Phase 1: Infrastructure — Lua 5.4 + sol2 + CMake

- [x] 1.1 Vendor Lua 5.4 source into `vendor/lua/` (replace stubs with real headers + .c sources)
- [x] 1.2 Create `vendor/lua/CMakeLists.txt` — build lualib as STATIC library
- [x] 1.3 Create `vendor/sol2/include/sol/sol.hpp` — vendor sol2 v3.3.0 header-only
- [x] 1.4 Update `cmake/FindDependencies.cmake` — add vendored sol2 include path
- [x] 1.5 Create `src/scripting/CMakeLists.txt` — `aria_scripting` OBJECT library target
- [x] 1.6 Modify root `CMakeLists.txt` — `add_subdirectory(src/scripting)` gated by `ARIA_FEATURE_SCRIPTING`
- [x] 1.7 Test: verify `ARIA_FEATURE_SCRIPTING=OFF` build succeeds without scripting
- [x] 1.8 Test: verify `ARIA_FEATURE_SCRIPTING=ON` build links `aria_scripting` + `lualib`

## Phase 2: Runtime — LuaRuntime + ScriptInstance + ScriptManager

- [x] 2.1 Create `src/scripting/lua_runtime.h/.cc` — `LuaRuntime` wrapping `sol::state` with lifecycle
- [x] 2.2 Implement sandbox in `LuaRuntime`: strip `os`/`io`/`loadfile`/`dofile`/`require` libs
- [x] 2.3 Implement instruction limit via `lua_sethook` (1M instr, watchdog on violation)
- [x] 2.4 Implement memory limit via `lua_setallocf` (16 MB cap, terminate on exceed)
- [x] 2.5 Create `src/scripting/script_instance.h/.cc` — `ScriptInstance` with id, filepath, `sol::state`, window set
- [x] 2.6 Create `src/scripting/script_manager.h/.cc` — `ScriptManager` owning all instances, tick(), reload()
- [x] 2.7 Modify `src/kernel/application.h/.cc` — add `ScriptManager*` member, call `script_manager_->tick()` in main loop
- [x] 2.8 Test: create VM, run valid script, assert output forwarded to log
- [x] 2.9 Test: sandbox — run `io.open`, `os.execute`, infinite loop, assert VM terminates with expected error
- [x] 2.10 Test: load syntax-error script, assert `sol::error` returned, VM remains usable
- [x] 2.11 Test: load → unload → reload script lifecycle, assert clean state on reload

## Phase 3: API Bindings — sol2 bindings for core DAW actions

- [x] 3.1 Create `src/scripting/api_bindings.h/.cc` — declare `aria.*` namespace with sol2 `set_function()`
- [x] 3.2 Implement `aria.transport.*` bindings: play(), stop(), record(), is_playing()
- [x] 3.3 Implement `aria.tracks.*` bindings: list(), get(name) — PR 2 subset
- [x] 3.4 Implement `aria.clips.*` bindings: move(track, beat), trim(start, end) via clip usertype (PR 3)
- [x] 3.5 Implement `aria.timing.*` bindings: position(), tempo(), beats_to_seconds() (PR 3)
- [x] 3.6 Register all bindings in `LuaRuntime::install_bindings(ActionRegistry&)` — integrate bindings with ActionRegistry
- [x] 3.7 Create `tests/CMakeLists.txt` entry for `aria_scripting_tests` test executable
- [x] 3.8 Test: call `aria.transport.play()` from Lua, verify mock transport handler invoked
- [x] 3.9 Test: `aria.tracks.get("Bass")` on nonexistent track returns `nil`
- [x] 3.10 Test: `clip:move(track=-1)` returns error, clip unchanged (PR 3)

## Phase 4: Macro Recorder — ActionRegistry + MacroRecorder

- [x] 4.1 Create `src/scripting/macro_recorder.h/.cc` — `ActionRegistry` with register/unregister/find/list
- [x] 4.2 Add capture hook to `ActionRegistry` — wraps CommandQueue dispatch during recording
- [x] 4.3 Implement `MacroRecorder` — start()/stop(), session buffer, action capture with timestamps
- [x] 4.4 Implement `MacroRecorder::to_lua_script()` — serialize captured actions + timing as `.lua` string
- [x] 4.5 Implement `MacroRecorder::save(name)` / `load(name)` — persist/read `.lua` files to user macros dir
- [x] 4.6 Test: ActionRegistry — register, list, duplicate (replace handler), find missing, assert behavior
- [x] 4.7 Test: MacroRecorder — start/stop empty session returns empty, start with 3 actions serializes correctly
- [x] 4.8 Test: serialize with interleaved timing inserts `aria.timing.wait()` calls
- [x] 4.9 Test: save named macro → load → verify string content matches

## Phase 5: File Watcher completion — platform backends

- [x] 5.1 Implement Win32 backend in `src/graphics/file_watcher.cc` — `ReadDirectoryChangesW` with IOCP
- [x] 5.2 Implement Linux backend — `inotify` init, watch add/remove, event read loop
- [x] 5.3 Implement macOS backend — `FSEvents` stream creation, callback, CFType cleanup
- [x] 5.4 Add debounce logic per file type: 200ms for `.lua`, 2s for media/project files
- [x] 5.5 Wire file watcher events → `ScriptManager::reload()` for `.lua` hot-reload
- [x] 5.6 Test: simulate `.lua` save → assert `ScriptManager::reload()` fires within 500ms
- [x] 5.7 Test: rapid 5 saves in 1s → assert single reload after debounce (200ms)
- [x] 5.8 Test: `.lua` file created → event dispatched, not auto-loaded
- [x] 5.9 Test: `.lua` file deleted → warning logged, running script continues

## Phase 6: Script UI — ScriptWindow + widget API

- [x] 6.1 Create `src/scripting/script_window.h/.cc` — `ScriptWindow` proxy widget subclass of `Widget`
- [x] 6.2 Implement `ScriptWindow::add_button(label, sol::function on_click)` — create C++ button, bind Lua callback
- [x] 6.3 Implement `ScriptWindow::add_slider(label, min, max, def, sol::function on_change)`
- [x] 6.4 Implement `ScriptWindow::add_text_input(label, default, sol::function on_change)`
- [x] 6.5 Expose `aria.ui.window(title, width, height)` in API bindings — returns ScriptWindow proxy to Lua
- [x] 6.6 Implement auto-cleanup: `ScriptManager::unload()` destroys all owned ScriptWindows
- [x] 6.7 Implement error resilience: Lua callback throw → catch + log, window stays open
- [x] 6.8 Test: create window, add button + slider, simulate click → Lua callback invoked
- [x] 6.9 Test: invalid dimensions (≤0) or empty title → error, no window created
- [x] 6.10 Test: unload script with 3 windows → all windows closed, resources freed
- [x] 6.11 Test: unsupported widget type → error returned, no widget created

**Total**: 65/65 tasks complete ✅
