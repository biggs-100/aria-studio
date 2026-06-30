# Lua Runtime Specification

## Purpose

Manage Lua 5.4 virtual machines with sandboxing, script lifecycle, and resource limits for executing user-authored automation scripts.

## Requirements

### Requirement: VM Lifecycle

The system MUST create, reset, and destroy Lua 5.4 VM instances via sol2 state.

#### Scenario: Create VM

- GIVEN The system starts
- WHEN `ScriptManager::create_vm()` is called
- THEN A new `sol::state` with Lua 5.4 open libraries is created and ready

#### Scenario: Reset VM state

- GIVEN A VM has executed one or more scripts
- WHEN `vm.reset()` is called
- THEN All globals are cleared and a fresh state without residual state is returned

#### Scenario: Destroy orphaned VM

- GIVEN A script instance is destroyed without explicit VM cleanup
- WHEN The `ScriptInstance` destructor runs
- THEN The associated `sol::state` is released and memory freed

### Requirement: Sandbox

The system MUST sandbox scripts to prevent dangerous operations (file, network, OS access) and enforce resource limits.

#### Scenario: Restricted file access

- GIVEN A script attempts `io.open("/etc/passwd")`
- WHEN The script executes in sandbox mode
- THEN Access is denied and a sandbox violation error is returned

#### Scenario: Memory limit exceeded

- GIVEN A script allocates memory beyond the 16 MB limit
- WHEN `lua_setallocf` reports allocation failure
- THEN The VM is terminated with a "memory limit exceeded" error

#### Scenario: Instruction limit exceeded

- GIVEN A script runs an infinite loop
- WHEN `lua_sethook` fires after 1M instructions
- THEN Execution is interrupted and the VM is halted with a watchdog error

#### Scenario: Network access blocked

- GIVEN A script calls `socket.connect()`
- WHEN The sandbox is active
- THEN The call is blocked; `os` and `io` libraries are stripped from the environment

### Requirement: Script Lifecycle

The system SHALL support loading, executing, and unloading `.lua` scripts from a user script directory.

#### Scenario: Load and execute valid script

- GIVEN A `.lua` file exists in the user script directory
- WHEN `ScriptManager::execute("script.lua")` is called
- THEN The script compiles, runs, and output is forwarded to the log

#### Scenario: Syntax error in script

- GIVEN A `.lua` file contains invalid syntax
- WHEN The script is loaded
- THEN A `sol::error` is returned; the VM remains usable for subsequent scripts

#### Scenario: Runtime error during execution

- GIVEN A running script throws a Lua error
- WHEN The error propagates to sol2
- THEN The error message is captured via `lua_pcall` and logged; the script is terminated

#### Scenario: Unload script

- GIVEN A script is actively running
- WHEN `ScriptManager::unload(scriptId)` is called
- THEN The VM is reset and all script resources are freed
