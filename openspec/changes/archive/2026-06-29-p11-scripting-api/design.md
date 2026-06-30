# Design: P11 — Scripting API (Lua + Macros) in ARIA DAW

## Technical Approach

Six-phase buildout: vendor Lua 5.4 + sol2 3.x → LuaRuntime sandbox → sol2 API bindings → MacroRecorder → FileWatcher completion → Script UI. Scripts run on the main thread, mutate DAW state via CommandQueue. Feature flag `ARIA_FEATURE_SCRIPTING` (existing) gates all new code.

## Architecture Decisions

| Decision | Choice | Alternatives Considered | Rationale |
|---|---|---|---|
| **Binding layer** | sol2 3.x over Lua 5.4 C-API | Raw C API, LuaBridge | 164+ API functions × ~30 lines raw C = ~5000 lines of glue. sol2 reduces to ~500 lines declarative. C++23 already in use — variadic templates, `std::function`. Header-only: matches existing vendoring pattern (`vendor/json/`, `vendor/spdlog/`). |
| **Thread model** | Scripts execute on **main thread** | Worker thread pool | DAW state (transport, tracks, clips) is all mutable C++ objects. All mutation goes through `CommandQueue` (mutex-guarded). No thread-safe MT needed for P11. Audio thread scripts explicitly out-of-scope. |
| **Sandbox strategy** | `lua_sethook` (1M instr limit) + `lua_setallocf` (16 MB) + stripped `os`/`io`/`loadfile`/`dofile`/`require` | OS-level seccomp/pledge | sol2 wraps C-API; we call raw `lua_sethook` + `lua_setallocf` against the `lua_State*` inside `sol::state`. VM terminates on violation. No cross-platform OS sandbox available. |
| **Script lifecycle** | `ScriptManager` owns `ScriptInstance` (id, `sol::state`, filepath, window set) | Global `sol::state` | Isolation: each script gets its own VM. Reset VMs (not destroy) on unload to reuse memory. Windows owned by that script destroyed on unload/error. |
| **Macro capture** | New `ActionRegistry` (string-keyed `std::function`) + `MacroRecorder` wrapping `CommandQueue` | Intercept at UI event level | `CommandQueue::Command` is `std::function<void()>` — no introspection. `ActionRegistry` provides named, serializable action handlers. `MacroRecorder::start()` wraps a hook around `CommandQueue::dispatch()` to capture action IDs + params. |
| **Script UI** | `ScriptWindow` proxy widget (subclass of `Widget`) owns a sub-tree of native C++ widgets | Separate SkiaCanvas overlay | Matches existing widget tree ownership pattern. `ScriptWindow` lives in a floating container managed by the window manager. Widget creation API calls C++ widget constructors and attaches them to the proxy. |
| **File watcher** | Complete Win32 `ReadDirectoryChangesW` + Linux `inotify` + macOS `FSEvents` | Polling-only fallback | Current skeleton fires once then stops. Per spec: debounce 200ms for `.lua`, 2s for media. `FileWatcher` already has the right interface — only the platform impl bodies need filling. Polling kept as compile-time fallback for unsupported platforms. |
| **Build isolation** | `aria_scripting` STATIC library target | Add sources to `aria_core` | sol2 templates bloat compilation. Isolating scripting sources prevents rebuild of the entire `aria_core` (45+ .cc files) when script bindings change. Links into `aria` executable alongside `aria_core`. |

## Data Flow

```
┌──────────────┐    .lua file     ┌──────────────────┐
│  FileWatcher  │ ──hot-reload──► │  ScriptManager   │
│  (platform)   │                 │  (owns instances) │
└──────────────┘                 └──────┬───────────┘
                                        │ load / unload
                                        ▼
       ┌─────────────────────┐   ┌──────────────┐
       │ MacroRecorder       │   │ ScriptInstance│
       │ (captures actions)  │   │ (sol::state)  │
       │ → emits .lua        │   │              │
       └────────┬────────────┘   └──────┬───────┘
                │                       │ sol2 bindings
                ▼                       ▼
       ┌──────────────────────────────────────┐
       │          ActionRegistry              │
       │  ("transport.play" → handler fn)     │
       └──────────────────┬───────────────────┘
                          │ dispatch()
                          ▼
       ┌──────────────────────────────────────┐
       │          CommandQueue                │
       │  (thread-safe, main-thread drain)   │
       └──────────────────┬───────────────────┘
                          │ process_pending()
                          ▼
       ┌──────────────────────────────────────┐
       │          DAW Model State             │
       │  (transport, tracks, clips, etc.)    │
       └──────────────────────────────────────┘
```

**Main loop** (`Application::run()`):
```
event_bus_->dispatch_pending()
  → command_queue_->process_pending()   ← script actions drain here
  → script_manager_->tick()             ← Lua UI callbacks, hot-reload checks
  → render_loop_->tick()                ← ScriptWindow renders as normal widget
```

## File Changes

| File | Action | Description |
|---|---|---|
| `vendor/lua/` | Modify | Replace stubs with real Lua 5.4 headers + compiled library |
| `vendor/sol2/` | Create | Sol2 v3.3.0 header-only `include/sol/sol.hpp` |
| `vendor/lua/CMakeLists.txt` | Modify | Build vendored Lua 5.4 as STATIC `lualib` target |
| `cmake/FindDependencies.cmake` | Modify | Add `find_package`/vendored path for sol2; Lua path already exists |
| `src/scripting/lua_runtime.h` | Create | `LuaRuntime` — wraps `sol::state`, lifecycle, sandbox hooks |
| `src/scripting/lua_runtime.cc` | Create | Implementation: `lua_sethook` callback, `lua_setallocf` handler |
| `src/scripting/script_instance.h` | Create | `ScriptInstance` — id, filepath, state, owned windows |
| `src/scripting/script_instance.cc` | Create | Load/execute/unload, error handling |
| `src/scripting/script_manager.h` | Create | `ScriptManager` — owns all instances, hot-reload, tick |
| `src/scripting/script_manager.cc` | Create | Lifecycle orchestration, file watcher registration |
| `src/scripting/api_bindings.h` | Create | sol2 binding declarations (`aria.*` namespace) |
| `src/scripting/api_bindings.cc` | Create | Transport, track, clip, timing, UI bindings |
| `src/scripting/macro_recorder.h` | Create | `MacroRecorder` + `ActionRegistry` |
| `src/scripting/macro_recorder.cc` | Create | Capture → serialize → emit Lua |
| `src/scripting/script_window.h` | Create | `ScriptWindow` proxy widget |
| `src/scripting/script_window.cc` | Create | Widget sub-tree creation, Lua callback dispatch |
| `src/scripting/CMakeLists.txt` | Create | `aria_scripting` STATIC library target |
| `CMakeLists.txt` | Modify | Add `add_subdirectory(src/scripting)` when `ARIA_FEATURE_SCRIPTING=ON` |
| `src/kernel/application.h` | Modify | Add `ScriptManager*` member (non-owning, registered via ServiceLocator) |
| `src/kernel/application.cc` | Modify | Call `script_manager_->tick()` in main loop |
| `src/graphics/file_watcher.cc` | Modify | Complete Win32 `ReadDirectoryChangesW`, add inotify (Linux), FSEvents (macOS) |
| `src/automation/macros.h` | Modify | Add `#include` guard note only — `MacroRecorder` lives in `src/scripting/` |
| `tests/CMakeLists.txt` | Modify | Add `aria_scripting_tests` test executable |

## Interfaces / Contracts

```cpp
// ActionRegistry — allows macro capture of named DAW actions
class ActionRegistry {
public:
    using Handler = std::function<void(const nlohmann::json& params)>;
    void register_action(std::string_view name, Handler handler);
    void unregister(std::string_view name);
    Handler* find(std::string_view name);
    std::vector<std::string> list() const;

    // Capture hook — wraps CommandQueue dispatch during recording
    void set_capture_hook(std::function<void(std::string_view, nlohmann::json)> hook);
};

// ScriptManager — lifecycle and tick
class ScriptManager {
public:
    ScriptId create_vm();                          // returns new VM id
    bool load(ScriptId id, const std::string& path);
    bool execute(ScriptId id);                     // runs loaded script
    void unload(ScriptId id);                      // resets VM + destroys windows
    void tick();                                   // hot-reload checks, pending callbacks
    void reload(const std::string& path);          // file watcher trigger
};

// LuaRuntime (internal, wraps sol::state)
class LuaRuntime {
    sol::state state_;
    void install_sandbox();
    void install_bindings(ActionRegistry& registry);
    bool load_script(const std::string& source);
};

// ScriptWindow (widget proxy, subclass of Widget)
class ScriptWindow : public Widget {
    ScriptId owner_id_;
    std::vector<std::unique_ptr<Widget>> controls_;
    void add_button(std::string_view label, sol::function on_click);
    void add_slider(std::string_view label, float min, float max,
                    float def, sol::function on_change);
    void cleanup();  // removes from tree, frees children
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|---|---|---|
| Unit | LuaRuntime sandbox (instr limit, mem limit, stripped libs) | Create VM, run malicious scripts, assert VM terminates with expected error |
| Unit | Script lifecycle (load, syntax error, runtime error, unload) | Load valid + invalid `.lua` strings, assert state transitions |
| Unit | API bindings (transport, tracks, clips, timing) | Register mock handlers in ActionRegistry, call via `sol::state`, verify Lua calls C++ |
| Unit | ActionRegistry (register, list, duplicate, missing) | Pure C++ — no Lua needed |
| Unit | MacroRecorder (start/stop, serialize, empty session) | Capture mock actions, verify Lua output string |
| Unit | ScriptWindow (create, add widgets, callbacks, cleanup) | Create widget, simulate click, verify Lua callback invoked |
| Unit | FileWatcher (debounce, multi-file, platform stubs) | Platform-specific tests or abstraction for unit-testable scheduling |
| Integration | Script → CommandQueue → DAW state mutation | Execute script that calls `aria.transport.play()`, verify mock transport handler invoked |

## Open Questions

- [ ] **sol2 v3.3.0 vs latest**: Which tag pins stably with Lua 5.4.6? Must test in CI.
- [ ] **ActionRegistry serialization format**: How do action params serialize to JSON for macro capture? Depends on each action's argument shape.
- [ ] **ScriptWindow parent**: Where in the widget tree do floating ScriptWindows attach? A top-level overlay container or a dedicated floating layer?
- [ ] **sol2 compile-time impact**: If `aria_scripting` STATIC lib still causes long builds, consider a unity build or precompiled header.
- [ ] **Lua vendoring**: Vendor real Lua 5.4 source or require system install? Exploration notes the stub is blocking — need concrete decision.
