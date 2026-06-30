# File Watcher Specification

## Purpose

Real-time file system monitoring using platform-native APIs (inotify, FSEvents, ReadDirectoryChangesW). Detects file creation, modification, deletion, and renaming, and triggers debounced re-indexing.

## Requirements

### Requirement: Platform-Native Watching

The watcher MUST use the platform's native file change notification API.

| Platform | API |
|---|---|
| Linux | inotify (via `notify` crate) |
| macOS | FSEvents (via `notify` crate) |
| Windows | ReadDirectoryChangesW (via `notify` crate) |

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | File created | Watcher running on folder `/samples` | New file `kick.wav` saved to `/samples` | `on_file_created` fires with path |
| 2 | File deleted | Watcher running on folder `/samples` | User deletes `snare.wav` | `on_file_deleted` fires with path |
| 3 | File renamed | Watcher running on folder `/samples` | User renames `old.wav` → `new.wav` | `on_file_renamed` fires with both old and new paths |
| 4 | Unsupported system | Platform filesystem does not support native watching | Watcher starts | Returns error; caller falls back to periodic scan |

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

### Requirement: Watched Folder Management

The system SHALL support adding, removing, and listing watched folders, persisted in the `watched_folders` table.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Add watch | Valid readable directory path | `add_watch(path)` called | Directory added to `watched_folders`; watcher starts |
| 2 | Remove watch | Folder currently being watched | `remove_watch(path)` called | Folder removed; watcher stops watching it |
| 3 | Add nonexistent path | Path does not exist on disk | `add_watch(path)` called | Error returned; no entry added to DB |

### Requirement: Fallback Strategy

If the watcher dies or misses events, a periodic full scan SHOULD run as fallback.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Missed creation | Watcher missed an event (e.g., USB drive mounted) | Periodic scan runs (every 300s) | New file detected and indexed |
| 2 | Watcher failure | Watcher thread crashes | System detects watcher stopped | Fallback scan starts; watcher restarted |
