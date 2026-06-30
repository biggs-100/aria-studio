# Proposal: P9 — Browser & Database

## Intent

Fast, searchable file browser inside the DAW. Users need sample indexing, metadata analysis (BPM, key, loudness, duration), file system watching, and drag-and-drop to tracks. Currently no in-DAW sample management exists — users rely on external file managers.

## Scope

### In Scope
- SQLite DB (rusqlite bundled+FTS5): full schema for samples, plugins, projects, presets, tags
- File scanner (walkdir, Rust): recursive scan, metadata extraction (symphonia), file hash
- File watcher (notify, Rust): platform-native change events, debounced re-scan
- Search engine (FTS5, Rust): full-text + filtered (tag, BPM, key, category) — <50ms for 100k samples
- Browser engine (C++): BrowserEngine, SearchProvider, FileBrowserModel, PreviewPlayer
- Provisional ImGui UI: browser panel, search, results, waveform thumbnail preview
- Waveform preview cache: peak/minima for browser thumbnails (independent of AudioClip's internal cache)
- Audio preview: browser-owned decoder via symphonia
- Drag-and-drop: target track from screen coords → create AudioClip via ProjectManager::create_audio_clip()

### Out of Scope
- GPU-accelerated UI framework (P10)
- Cloud/network sample browsing
- Sample editing or audition chains
- Plugin preset browser UI
- Batch background re-analysis on file changes

## Capabilities

### New Capabilities
- `database-indexing`: SQLite schema, migrations, connection pool, query API for samples/plugins/projects/presets/tags
- `file-scanner`: Recursive scanning, audio detection, metadata extraction (BPM, key, loudness, duration, sample rate)
- `file-watcher`: Platform-native FS watching, debounced events, automatic re-scan trigger
- `search-engine`: FTS5 full-text + filtered queries, BM25 ranking, <50ms latency target
- `file-browser`: C++ BrowserEngine, file tree, search results model, drag source, ImGui provisional UI
- `waveform-cache`: Peak/minima waveform data for browser preview thumbnails

### Modified Capabilities
None

## Approach

Rust crates (rusqlite+FTS5, walkdir, notify, symphonia) exposed via FFI (`aria_rust.h`). C++ `BrowserEngine` registers in `ServiceLocator`, owns `SearchProvider` + `PreviewPlayer`, publishes via `EventBus`. Provisional ImGui UI. Drag-and-drop: arrangement view hit-test + `ProjectManager::create_audio_clip()`.

**Delivery**: 5 chained PRs — (1) DB + migrations, (2) scanner + watcher, (3) search engine, (4) browser UI + preview, (5) drag-and-drop integration.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `rust/aria-database/` | Modified | Real rusqlite integration, queries, migrations |
| `rust/aria-filesystem/` | Modified | scanner + watcher implementations |
| `rust/include/aria_rust.h` | Modified | New FFI exports (scan, search, watcher) |
| `src/browser/` | New | BrowserEngine, SearchProvider, PreviewPlayer |
| `src/audio/` | Modified | Preview playback integration point |
| `src/kernel/` | Modified | ServiceLocator reg, EventBus event types |
| `CMakeLists.txt` | Modified | Browser source group, Rust deps |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| FFI ABI mismatch | Med | `#[repr(C)]`, versioned structs, integration tests |
| BPM/key accuracy | Med | Header tags first, documented bounds, analysis fallback |
| Platform watcher edge cases | Med | `notify` handles most; fallback periodic scan |
| Concurrent indexing vs audio | Low | Background threads, OS thread priority isolation |
| Search perf at 100k samples | Low | FTS5 + composite indexes; CI benchmark |

## Rollback Plan

Revert Cargo.toml (remove rusqlite/walkdir/notify). Delete `src/browser/`. Revert `aria_rust.h` FFI exports. No persistent schema risk — SQLite db is per-user on disk only.

## Dependencies

- `rusqlite = "0.31"` (bundled, fts5), `walkdir = "2"`, `notify = "6"`, `symphonia` (pure Rust)
- ImGui (already vendored in C++ deps)

## Success Criteria

- [ ] DB opens, migrations run, `PRAGMA schema_version` matches expected
- [ ] Scanner indexes 1,000+ files with metadata (BPM, key, duration, sample rate)
- [ ] Watcher detects file add/modify/delete within 2s
- [ ] Search returns results <50ms for 100k indexed samples
- [ ] Browser UI renders file tree + search results + waveform thumbnail
- [ ] Drag sample from browser → drop on track creates AudioClip at correct position
- [ ] All existing CI tests pass
