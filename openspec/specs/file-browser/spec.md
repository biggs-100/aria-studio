# File Browser Specification

## Purpose

C++ BrowserEngine providing file tree (by folder), category tree (by type), search results view, preview panel with waveform thumbnail and metadata, audio preview player, and drag-and-drop to tracks. Provisional ImGui UI.

## Requirements

### Requirement: Navigation Modes

The browser SHALL support three navigation modes: folder tree, category tree, and search results.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Folder tree | Watched folder `/samples` with subdirectories | Browser loads folder tree | Tree displays `/samples/Kicks`, `/samples/Snares` with file counts |
| 2 | Category tree | DB has categorized samples (kicks=50, snares=30) | User selects category view | `Kicks (50)`, `Snares (30)` displayed; clicking shows samples |
| 3 | Search results | User types "dark pad" in search field | Results returned | Search results replace current file list |

### Requirement: File Preview

Selecting a file SHALL display its waveform thumbnail, metadata (duration, sample rate, BPM, key, loudness), tags, and rating in a preview panel.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | File selected | User clicks `kick.wav` in file list | Preview loads | Waveform thumbnail, "BPM: 120", "Key: Cm", "Duration: 2.4s", tags shown |
| 2 | Unanalyzed file | File has no analysis data (BPM=0, key=null) | User selects it | Metadata shows "BPM: --", "Key: --"; waveform still renders |

### Requirement: Audio Preview Playback

The browser SHALL play audio previews independently of the main transport. The preview player MUST support play, stop, pause, resume, and volume control.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Play preview | User selects file and clicks Play | Audio begins | Preview audio plays through preview output; transport unaffected |
| 2 | Stop preview | Preview is playing | User clicks Stop | Audio stops; playback position resets |
| 3 | Preview during transport | Main transport playing a project | User clicks preview Play | Preview plays simultaneously; no audio conflicts |

### Requirement: Drag-and-Drop

Dragging a file from the browser and dropping onto a track SHALL create an AudioClip via `ProjectManager::create_audio_clip()` at the correct timeline position.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Drop on audio track | Audio track at arrangement position bar 3 | User drags `kick.wav` from browser onto track | AudioClip created at bar 3 referencing `kick.wav` |
| 2 | Drop on empty area | No track at drop coordinates | User drops file | No clip created; no crash |
| 3 | Drop multiple files | User selects 5 files | User drag-and-drops all onto a track | 5 AudioClips created sequentially |

### Requirement: UI Responsiveness

The browser list MUST scroll smoothly at 60 FPS with up to 10,000 entries visible.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Large list | 10,000 files in current view | User scrolls rapidly | Frames render without stutter; no input lag >16ms |
| 2 | Incremental load | 50,000 total entries | View renders | Virtual scrolling loads only visible rows |
