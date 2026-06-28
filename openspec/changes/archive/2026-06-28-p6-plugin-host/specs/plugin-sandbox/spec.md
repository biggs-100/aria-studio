# Plugin Sandbox Specification

## Purpose

Provide Level 1 sandbox isolation: each plugin runs on its own thread with a watchdog timeout. A crash kills only that plugin's thread, not the DAW. The blacklist tracks crashes and supports manual disable/ban.

## Requirements

### Requirement: Per-Plugin Thread Isolation

Every instance MUST be processed on a dedicated thread from a managed pool. A crash or hang in one plugin MUST NOT block the main audio thread or other plugin threads.

#### Scenario: Plugin hang does not stall others
- GIVEN three plugins (A, B, C) on separate sandbox threads
- WHEN plugin B loops infinitely in `process()`
- THEN A and C continue without interruption AND the DAW transport stays responsive

#### Scenario: Plugin crash kills only its thread
- GIVEN a plugin that segfaults during `activate()`
- WHEN the crash occurs on its sandbox thread
- THEN the DAW main thread is unaffected AND the crash handler reports the PluginID

### Requirement: Watchdog Timeout

The watchdog MUST monitor each thread's `process()` duration. If it exceeds the timeout, the watchdog MUST terminate the thread, bypass the plugin, and mark it crashed. The timeout MUST be configurable per plugin and per category.

#### Scenario: Watchdog terminates hung plugin
- GIVEN a watchdog timeout of 100 ms
- WHEN a plugin's `process()` blocks for 500 ms
- THEN the thread is terminated after 100 ms AND the plugin is bypassed and marked crashed

#### Scenario: Per-category timeout override works
- GIVEN a reverb with a 500 ms watchdog override
- WHEN it processes normally under 500 ms
- THEN the watchdog does not fire AND the plugin continues

### Requirement: Crash Recovery

After a crash, the plugin MUST be bypassed with its last parameter state preserved. The host MAY attempt restart on the next play-stop cycle. State MUST reload the last saved values.

#### Scenario: Crashed plugin restarts on next cycle
- GIVEN a plugin in bypass state after crashing
- WHEN the user stops and restarts transport
- THEN the host re-initializes the plugin AND if successful, it resumes with last known parameters

### Requirement: Plugin Blacklist Levels

`PluginBlacklist` MUST support `Warning` (show on load), `Disabled` (skip in scan), and `Banned` (never load). Crash reporting MUST escalate from Warning to Disabled after 3 crashes. The blacklist MUST persist to and load from JSON.

#### Scenario: Three crashes escalate to Disabled
- GIVEN a plugin with no prior crashes
- WHEN it crashes 3 times in one session
- THEN the level escalates to Disabled after the 3rd crash
- AND subsequent scans skip it

#### Scenario: Manual ban overrides auto escalation
- GIVEN a plugin at Warning level (1 crash)
- WHEN the user manually bans it
- THEN the level is Banned regardless of crash count AND it never loads

### Requirement: Blacklist Persistence

`save()` MUST write `~/.aria/blacklist.json` with crash count, level, reason, and timestamp. `load()` MUST read on startup. Corrupt or missing files MUST initialize empty without crashing.

#### Scenario: Corrupt file initializes empty
- GIVEN a `blacklist.json` with invalid JSON
- WHEN `load()` runs during startup
- THEN the blacklist initializes empty AND a warning is logged

### Requirement: Manual Management

Users MUST view all entries, clear individual entries, and clear all. Clearing resets the crash counter and level.

#### Scenario: Cleared entry allows reload
- GIVEN a plugin at Disabled due to 3 crashes
- WHEN the user clears the entry and rescans
- THEN the plugin appears in available plugins AND loading shows no warning
