# Macro Recorder Specification

## Purpose

Capture user actions dispatched through `CommandQueue` during a recording session, serialize them into Lua scripts, and support replay through the ScriptManager.

## Requirements

### Requirement: Action Capture

The system MUST record all user actions dispatched through `CommandQueue` while a macro recording session is active.

#### Scenario: Start and stop recording

- GIVEN No recording is active
- WHEN `MacroRecorder::start()` is called
- THEN A new recording session begins; all subsequent actions are timestamped and appended to the capture buffer

#### Scenario: Stop recording with captured actions

- GIVEN A recording session is active and 5 actions have been captured
- WHEN `MacroRecorder::stop()` is called
- THEN Recording ends; the captured action list is available via `MacroRecorder::actions()`

#### Scenario: Empty recording session

- GIVEN Recording is started but no actions occur
- WHEN `MacroRecorder::stop()` is called
- THEN An empty action list is returned

#### Scenario: Double-start recording

- GIVEN A recording is already active
- WHEN `MacroRecorder::start()` is called again
- THEN The existing session is discarded and a new empty session begins

### Requirement: Action Registry

The system MUST provide an `ActionRegistry` to register and enumerate callable DAW actions for macro capture.

#### Scenario: Register action handler

- GIVEN A handler function exists for `"track.create"`
- WHEN `ActionRegistry::register("track.create", handler)` is called
- THEN The action becomes available for macro capture and replay

#### Scenario: Enumerate registered actions

- GIVEN Five actions are registered in the registry
- WHEN `ActionRegistry::list()` is called
- THEN An array of all five action name strings is returned

#### Scenario: Register duplicate action

- GIVEN `"track.create"` is already registered
- WHEN `ActionRegistry::register("track.create", newHandler)` is called
- THEN The existing handler is replaced; no error is thrown

### Requirement: Serialization

The system MUST serialize captured actions into a valid Lua script string via `MacroRecorder::to_lua_script()`.

#### Scenario: Serialize action sequence without timing

- GIVEN Three recorded actions: play, stop, export
- WHEN `MacroRecorder::to_lua_script()` is called
- THEN A Lua string with three sequential `aria.*` function calls is returned

#### Scenario: Serialize action sequence with timing

- GIVEN Actions have interleaved wait durations (e.g., 2s between play and stop)
- WHEN serialized
- THEN `aria.timing.wait(2000)` calls are inserted between the corresponding actions

#### Scenario: Serialize empty session

- GIVEN No actions were captured
- WHEN `MacroRecorder::to_lua_script()` is called
- THEN An empty string is returned

### Requirement: Replay

The system MUST replay captured macros by loading the emitted Lua script via ScriptManager.

#### Scenario: Replay recorded macro

- GIVEN A macro has been serialized to a `.lua` file
- WHEN The script is executed via `ScriptManager::execute()`
- THEN All actions replay in order with the same interleaved timing

### Requirement: Storage

The system SHOULD persist named macros to disk as `.lua` files.

#### Scenario: Save named macro

- GIVEN A recording session has captured 3 actions
- WHEN `MacroRecorder::save("MyMacro")` is called
- THEN A file `MyMacro.lua` is written to the user macros directory

#### Scenario: Load existing macro

- GIVEN `MyMacro.lua` exists in the user macros directory
- WHEN `MacroRecorder::load("MyMacro")` is called
- THEN The `.lua` file content is returned as a string

#### Scenario: Load nonexistent macro

- GIVEN No `Unknown.lua` exists
- WHEN `MacroRecorder::load("Unknown")` is called
- THEN An error is returned
