# Delta for File Watcher

## ADDED Requirements

### Requirement: Script Hot-Reload

The system SHALL detect modifications to `.lua` script files and trigger re-execution via ScriptManager to enable responsive script hot-reload.

#### Scenario: Script file modified triggers reload

- GIVEN A `.lua` file is loaded and actively running
- WHEN The file is saved with changes
- THEN The script is re-executed via `ScriptManager::reload()` within 500ms

#### Scenario: New script file created

- GIVEN The file watcher is monitoring the user script directory
- WHEN A new `.lua` file is created
- THEN A `on_file_created` event is dispatched; the script is available for execution but NOT auto-loaded

#### Scenario: Script file deleted during execution

- GIVEN A `.lua` file is loaded and running
- WHEN The file is deleted from disk
- THEN A warning is logged; the running script continues until manually unloaded

#### Scenario: Multiple `.lua` saves in rapid succession

- GIVEN The user rapidly saves a script file 5 times in 1 second
- WHEN Debounced events are processed
- THEN A single reload event fires after the debounce window

## MODIFIED Requirements

### Requirement: Debounced Updates

The watcher MUST debounce rapid consecutive events to avoid redundant re-indexing. Script files (`.lua`) SHOULD use a shorter debounce window (200ms) than media files (2s) to support responsive hot-reload.
(Previously: uniform 2s debounce across all file types)

#### Scenario: Bulk copy (unchanged)

- GIVEN 500 files copied into watched folder within 1 second
- WHEN Watcher accumulates events
- THEN Single debounced event fires after settling period (≤2s)

#### Scenario: Single save media file (unchanged)

- GIVEN User saves one `.wav` file
- WHEN Watcher receives one event
- THEN Event fires after 2s debounce window

#### Scenario: DAW project save (unchanged)

- GIVEN DAW writes temp files, replaces project file, deletes temp
- WHEN Watcher receives 10+ events in 500ms
- THEN Single debounced "project modified" event fires

#### Scenario: Script file hot-reload (modified — shorter debounce)

- GIVEN File watcher is monitoring the user script directory
- WHEN A `.lua` file is saved
- THEN A debounced event fires after 200ms and triggers `ScriptManager::reload()`
