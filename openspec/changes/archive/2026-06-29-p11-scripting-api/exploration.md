## Exploration: P11 — Scripting API (Lua) in ARIA DAW

### Current State

The codebase is **prepared but unimplemented** for scripting. Here's what exists today:

**Architecture (Application Loop):**
- `src/main.cc` → `aria::Application::instance().init(argc, argv)` → `app.run()` → `app.shutdown()`
- Inside `run()`: `event_bus_->dispatch_pending()` → `command_queue_->process_pending()` → `render_loop_->tick()`
- `src/kernel/event_bus.h` — typed publish/subscribe event bus (`EventType` as `uint32_t`, templated `publish<T>()`/`queue<T>()`)
- `src/kernel/command_queue.h` — thread-safe `std::function<void()>` command queue with mutex
- `src/kernel/service_locator.h` — type-based service registration (`register_service<T>()`, `get_service<T>()`)
- `src/kernel/undo_stack.h` — undo/redo with `UndoFunc`/`RedoFunc` pairs, max 256 entries

**No Action/Command Registry exists yet.** The `CommandQueue` is generic — there is no typed `ActionManager`, `CommandRegistry`, or dispatch-by-ID system that a macro recorder could hook into.

**Scripting Status:**
- `src/scripting/` — **directory exists but is EMPTY**. No `.h` or `.cc` files.
- `18_Scripting.md` — 868-line detailed draft spec covering LuaRuntime, ScriptInstance, all API categories, sandbox, hot-reload, macro recording. **Not implemented.**
- `19_API.md` — 746-line draft C API spec covering 164 functions across 13 modules. **Not implemented.**
- Both specs have all RFCs as "Pending" status.

**Vendored Lua is a STUB:**
- `vendor/lua/include/lua.h` — minimal stub: only `lua_State` typedef, `luaL_newstate()` returning `nullptr`, `lua_close()` as no-op
- `vendor/lua/luastub.c` — no-op function returning 0
- `vendor/lua/lua.lib` — likely empty/skeleton
- **No real Lua 5.4 headers or library are vendored.** The CMake build would fall through to `find_package(Lua 5.4)` which requires system-installed Lua.

**Build System:**
- CMake 3.28, **C++23** (`CMAKE_CXX_STANDARD 23`)
- `ARIA_FEATURE_SCRIPTING` option (ON by default)
- `FindDependencies.cmake`: `find_package(Lua 5.4 QUIET)` with vendored fallback to `${CMAKE_SOURCE_DIR}/vendor/lua/`
- `${LUA_LIBRARIES}` linked to `aria_core` (currently resolves to the stub)
- Vendor pattern: check `vendor/{lib}/` first, fallback to FetchContent or system find_package

**UI Widget System (for script UI panels):**
- Widget tree: `Widget` (abstract) → `ThemedWidget` → `ButtonWidget`, `ContainerWidget`, etc.
- Rendered via SkiaCanvas on Dawn WebGPU. Widgets are C++ owned (unique_ptr parent-child tree).
- The existing `ContainerWidget` supports clipping, scroll, and transform offsets.
- No existing mechanism for script-created widgets in the tree.

**File Watcher (for hot-reload):**
- `src/graphics/file_watcher.h/cc` — exists with Win32 skeleton
- Current implementation fires callback once at startup; full ReadDirectoryChangesW is stubbed
- Non-Win32 platforms: fires one-shot then stops
- Full spec exists in `openspec/specs/file-watcher/spec.md` but actual platform implementation is incomplete
- Debounce support is designed (200ms) but not fully functional

**Macro System:**
- `src/automation/macros.h/cc` — `MacroSource` is a **parameter modulation source** (knob → parameter mapping), NOT an action recorder
- The spec in 18_Scripting.md §9 describes a `MacroRecorder` class that captures transport/track/clip/automation/plugin actions, but this class does NOT exist yet

**Existing Binding Patterns (FFI Bridge):**
- `src/graphics/ffi/bridge.cc` — draws via JSON command buffer parsed into `DrawCommand` structs
- Pattern: command struct → switch dispatch → `SkiaCanvas` draw calls
- This JSON-command pattern could inform how Lua scripts talk to the canvas

### Affected Areas

- `src/scripting/` — **New files go here** (currently empty). Will host `lua_runtime.h/cc`, `script_instance.h/cc`, `script_manager.h/cc`, `macro_recorder.h/cc`, `api_bindings.h/cc`, `lua_sandbox.h/cc`
- `vendor/lua/` — Must be replaced with **real Lua 5.4** headers and compiled library, OR Lua must be installed system-wide
- `vendor/sol2/` — **New directory needed** if using sol2 (header-only, easy to vendor)
- `cmake/FindDependencies.cmake` — Already references `${LUA_LIBRARIES}`, may need sol2 include path
- `src/kernel/application.h/cc` — May need to register `ScriptingService` or hook script processing into the main loop
- `src/kernel/service_locator.h` — `LuaRuntime` / `ScriptManager` registered as services
- `src/kernel/command_queue.h` — Thread-safe dispatch for script-initiated actions (already fits the pattern)
- `src/kernel/event_bus.h` — Script events need registered event types (must pick a range, e.g., 4000+)
- `src/kernel/undo_stack.h` — Script commands must push undo/redo pairs for undoable actions
- `src/graphics/file_watcher.h/cc` — Must be completed for **hot-reload** (currently skeleton)
- `src/graphics/widget.h` + `widget_button.h` + `widget_container.h` — Script-created UI widgets need either native widget creation or a script-specific overlay canvas
- `src/graphics/skia_canvas.h` — Drawing API callable from Lua for `aria.ui.canvas_draw_*()` functions
- `src/graphics/widget_rect.h` + `widget_text.h` — Additional reusable widgets that script UIs could leverage
- `src/automation/macros.h/cc` — `MacroRecorder` class needs to be built (separate from `MacroSource`)
- `src/automation/recorder.cc` — Existing automation recorder may inform macro recorder design
- `tests/CMakeLists.txt` — New test executable needed: `aria_scripting_tests` linking against `aria_core`
- `CMakeLists.txt` — `ARIA_FEATURE_SCRIPTING` already exists; may need `src/scripting/` sources added to `aria_core`

### Approaches

1. **sol2 3.x (C++ header-only Lua binding)** — Recommended
   - sol2 wraps `lua_State*` with modern C++ templates for clean `set_function()`, `new_usertype()`, `table` access
   - Header-only: just drop `include/` into `vendor/sol2/` per existing vendoring pattern
   - Compatible with C++17/20 — we already use C++23
   - **Pros:**
     - Dramatically less boilerplate than raw C API (10× fewer lines for 164+ functions)
     - Type-safe: `sol::function` handles push/pop/type checking automatically
     - Supports binding C++ classes as Lua userdata: `sol::usertype<Widget>`
     - Mature ecosystem, used in game engines (LÖVE, etc.)
     - Lua 5.4 compatible (sol2 3.x supports Lua 5.1–5.4)
   - **Cons:**
     - Template-heavy compile time impact
     - Debugging template errors can be difficult
     - Must pin exact version to avoid API drift
   - **Effort: Medium** (fast binding authoring, more setup initially)

2. **Raw Lua C API (lua_CFunction)** — Fallback
   - Bind each API function manually: `lua_pushcfunction()`, `lua_pushstring()`, `lua_pcall()`, etc.
   - Direct Lua 5.4 C API — no dependencies beyond Lua itself
   - **Pros:**
     - Zero dependencies beyond Lua 5.4
     - Full control over error handling and sandboxing
     - Minimal compile-time impact
     - Matches the existing spec (18_Scripting.md explicitly says "C-API via lua.h")
   - **Cons:**
     - Extremely verbose — each of 164+ functions needs push args, call, check results, pop
     - Prone to stack leaks and type errors
     - No automatic C++ class binding — must write proxy tables manually
     - Slow to develop and maintain
   - **Effort: Very High** (164+ functions × ~30 lines each ≈ 5000+ lines of glue code)

3. **LuaBridge (lua-bridge)** — Lightweight alternative
   - Lightweight single-header binding library
   - Similar to sol2 but simpler template approach
   - **Pros:**
     - Lighter compile-time overhead than sol2
     - Simpler mental model
     - Header-only possible
   - **Cons:**
     - Less actively maintained
     - Limited documentation and community support
     - Less flexible for complex bindings (nested tables, multi-return)
     - May not support Lua 5.4 edge cases well
   - **Effort: Medium** (but higher risk than sol2)

### Recommendation

**Use sol2 3.x as the binding layer, layered on raw Lua 5.4 C-API underneath.**

Rationale:
1. **C++23 is already the standard** — sol2 3.x thrives on modern C++ with `std::function`, variadic templates, and `auto`
2. **164+ API functions demand maintainable binding code** — sol2 reduces this from ~5000 lines of raw C glue to ~500 lines of declarative bindings
3. **The vendoring pattern is proven** — like `vendor/json/` and `vendor/spdlog/`, sol2 is header-only and can be added to `vendor/sol2/` with a simple INTERFACE CMake target
4. **Script UI will benefit** — `sol::usertype<Widget>` enables natural Lua manipulation of widget objects (`button:set_label("Hi")`)
5. **No migration needed later** — the spec already targets Lua 5.4; adding sol2 now avoids rewriting bindings when the full API goes in

The **Lua 5.4 C-API** is still there underneath for sandboxing hooks (`lua_sethook` for instruction limits, `lua_setallocf` for memory limits) and any low-level needs.

**Phased approach for P11 (macros scope):**
1. Phase 1: Install real Lua 5.4 (system or vendored), add `vendor/sol2/`, verify build
2. Phase 2: Build `LuaRuntime` + `ScriptInstance` classes with sandbox
3. Phase 3: Bind core DAW actions via sol2 (transport, track basic ops)
4. Phase 4: Implement `MacroRecorder` — capture CommandQueue actions → serialize → emit Lua
5. Phase 5: File watcher + hot-reload for `.lua` files
6. Phase 6: Script UI window creation (floating SkiaCanvas overlay or ContainerWidget wrapper)

### Risks

| # | Risk | Impact | Mitigation |
|---|------|--------|------------|
| 1 | **Lua 5.4 is not vendored** — `vendor/lua/` is a build stub. Builds will fail without system Lua. | Blocking | Install Lua 5.4 system-wide, OR vendoring real Lua 5.4 source into `vendor/lua/` with proper CMake target |
| 2 | **File watcher is incomplete** — hot-reload requires platform-native watching (Win32 skeleton, inotify/kqueue missing) | High for hot-reload | Complete `file_watcher.cc` platform impls as part of this P11. Non-Win32 stubs fire only once. |
| 3 | **No Action/Command registry** — `CommandQueue` is a generic `std::function` queue with no introspection. Macro recording needs to intercept, enumerate, serialize commands. | High | Design a lightweight `ActionRegistry` or `CommandID` system alongside the scripting API so macros can record/replay named actions |
| 4 | **sol2 compile-time impact** — adding a heavy template library to a large monolithic `aria_core` target | Medium | Isolate scripting sources into a separate `aria_scripting` STATIC library, or use precompiled headers |
| 5 | **Thread safety** — scripts running on non-main threads must use `CommandQueue::dispatch()` to mutate state, never direct model access | Medium | Document thread safety per API function. Use `CommandQueue` pattern already in place. Audio thread scripts need special care (real-time safe, no GC). |
| 6 | **UI from scripts** — widget tree is C++ owned. Script-created widgets need a container or script-window overlay, not deep integration | Medium | Start with a `ScriptWindow` proxy widget per script that owns a sub-tree. Scripts create widgets via API → C++ creates native Widget objects. |
| 7 | **sol2/Lua version compatibility** — sol2 3.x may have specific Lua 5.4 version requirements | Low | Pin specific sol2 tag (e.g., v3.3.0) and Lua version (5.4.6) during implementation and test |

### Ready for Proposal

**Yes** — the exploration is complete. The codebase is architecturally prepared (EventBus, CommandQueue, ServiceLocator, CMake scaffolding, empty `src/scripting/` directory). The critical gap is that `vendor/lua/` is a stub, and the file watcher needs implementation before hot-reload can work.

The orchestrator should tell the user:
1. **Architecture is ready** — EventBus, CommandQueue, ServiceLocator provide the right hooks
2. **Lua is not actually vendored** — `vendor/lua/` is a build stub; real Lua 5.4 must be added
3. **sol2 is recommended** — for maintainable bindings of the 164+ planned API functions
4. **File watcher needs work** — hot-reload won't work until file_watcher platform backends are implemented
5. **No CommandRegistry exists** — macro recording will need a lightweight action-tracking system built alongside the scripting API
