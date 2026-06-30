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

The watcher MUST debounce rapid consecutive events to avoid redundant re-indexing.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Bulk copy | 500 files copied into watched folder within 1 second | Watcher accumulates events | Single debounced event fires after settling period (≤2s) |
| 2 | Single save | User saves one file | Watcher receives one event | Event fires immediately or after short debounce |
| 3 | DAW project save | DAW writes temp files, replaces project file, deletes temp | Watcher receives 10+ events in 500ms | Single debounced "project modified" event fires |

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
