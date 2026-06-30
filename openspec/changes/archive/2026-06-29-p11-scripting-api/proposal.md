# Proposal: P11 — Scripting API (Lua + Macros)

## Intent

Empower users to automate ARIA DAW via Lua scripts — chain commands, create custom controls, and extend the DAW without C++.

## Scope

### In Scope
- Lua 5.4 runtime + sol2 3.x C++ bindings
- Script execution: `.lua` files loaded from user script folder
- Core DAW API bindings (transport, track, clip ops)
- Macro recorder — capture and replay named action sequences
- File watcher completion for hot-reload
- Script UI windows (floating panels with widgets)

### Out of Scope
- Script marketplace or distribution
- Audio thread scripting (real-time safety)
- Plugin scripting API (VST parameter binding)
- Non-Lua scripting languages

## Capabilities

### New Capabilities
- `lua-runtime`: Lua 5.4 VM management, sandbox, script lifecycle
- `script-api-bindings`: Sol2 bindings exposing DAW actions (transport, tracks, clips, timing)
- `macro-recorder`: Action capture → serialize → emit Lua script
- `script-ui`: Script-owned floating windows with widget creation API

### Modified Capabilities
- `file-watcher`: Hot-reload requires completing platform-native watching (inotify/FSEvents/ReadDirectoryChangesW) beyond current skeleton

## Approach

Six phases, each independently testable:

1. **Infrastructure**: Vendor real Lua 5.4 + sol2 3.x; update CMake targets
2. **Runtime**: `LuaRuntime` + `ScriptInstance` + sandbox (instruction/memory limits via `lua_sethook`/`lua_setallocf`)
3. **Bindings**: Expose core DAW actions via sol2 `set_function()` / `new_usertype()`
4. **Macros**: Build `MacroRecorder` + lightweight `ActionRegistry` on top of `CommandQueue`
5. **Hot-reload**: Complete `FileWatcher` Win32 impl; add inotify/Linux + FSEvents/macOS
6. **Script UI**: `ScriptWindow` proxy widget; API for script-created controls

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `vendor/lua/` | Replace | Stub → real Lua 5.4 headers + lib |
| `vendor/sol2/` | New | sol2 3.x header-only bindings |
| `src/scripting/` | New | All runtime, bindings, sandbox, macro code |
| `src/kernel/` | Modify | Register ScriptManager in ServiceLocator |
| `src/graphics/file_watcher.*` | Modify | Complete platform backends |
| `src/automation/macros.*` | Modify | Add MacroRecorder alongside MacroSource |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Lua not vendored (stub only) | High | Vendor real Lua 5.4 source or require system install |
| File watcher incomplete | High | Complete platform impl in Phase 5; fallback to polling |
| No CommandRegistry exists | High | Build lightweight ActionRegistry in Phase 4 |
| sol2 compile-time impact | Med | Isolate scripting in separate STATIC library target |

## Rollback Plan

- **Per-phase**: Each phase lands as a separate commit/PR. Rollback = revert that commit.
- **Feature flag**: `ARIA_FEATURE_SCRIPTING` (already defined, ON by default) gates all scripting code. Set OFF → scripting completely removed from build.
- **Hot-reload**: File watcher changes are additive — revert to polling-only if platform impls cause regressions.

## Dependencies

- Lua 5.4 (system install or vendored source)
- sol2 3.x (header-only, pin v3.3.0)
- CMake: `ARIA_FEATURE_SCRIPTING` option (exists)
- No external action registry needed — built in-house

## Success Criteria

- [ ] Script loads and executes `print("hello")` — verified in test
- [ ] Script calls `aria.transport.play()` — DAW starts playback
- [ ] Macro records 3 actions and replays them identically
- [ ] Edit `.lua` file → hot-reload triggers re-execution within 500ms
- [ ] Script creates a window with a button that triggers a DAW action
- [ ] All existing tests still pass with `ARIA_FEATURE_SCRIPTING=OFF`
