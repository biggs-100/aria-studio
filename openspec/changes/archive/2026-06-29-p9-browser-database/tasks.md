# Tasks: P9 — Browser & Database

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 2,000–3,000 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | DB → scanner+watcher → search → browser+preview → drag-drop |
| Delivery strategy | ask-on-risk |
| Chain strategy | feature-branch-chain |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | SQLite DB + schema + migrations + CRUD | PR 1 | Base = feature/p9-tracker; Cargo.toml deps, aria-database, bridge db_ffi, FFI header, DB tests |
| 2 | Scanner + watcher + metadata extraction | PR 2 | Base = PR 1; aria-filesystem, bridge fs_ffi, scan_progress, FFI additions, watcher manual test |
| 3 | FTS5 search engine | PR 3 | Base = PR 2; search.rs, bridge search FFI, integrated test |
| 4 | BrowserEngine + SearchProvider + PreviewPlayer + WaveformCache + ImGui panel | PR 4 | Base = PR 3; src/browser/*, event_types.h, CMake, test_browser.cpp |
| 5 | Drag-and-drop integration | PR 5 | Base = PR 4; project_manager.cc create_audio_clip, browser panel DnD wiring |

## Phase 1: Foundation / Infrastructure

- [x] 1.1 Update Cargo.toml deps: aria-database (rusqlite bundled, zstd), workspace resolver v2, aria-bridge member
- [x] 1.2 Create `rust/aria-bridge/` cdylib crate depending on aria-database, aria-filesystem, symphonia
- [x] 1.3 Create `rust/aria-database/src/errors.rs` — typed `DbError` (Sqlite, Migration, NotFound, Duplicate)
- [x] 1.4 Create `src/kernel/event_types.h` — `BrowserEvent` enum for scanned/changed/index/search/preview events
- [x] 1.5 Modify `rust/include/aria_rust.h` — FFI structs (SampleEntry, DbSearchQuery, SampleResult, TagResult, ScanProgressCallback, DB function declarations)

## Phase 2: Core Implementation

- [x] 2.1 Modify `rust/aria-database/schema.rs` + `migration.rs` — FTS5 tables, migration runner, schema_version tracking
- [x] 2.2 Modify `rust/aria-database/lib.rs` + `queries.rs` — real Database with WAL, prepared statements, batch inserts, tag joins
- [x] 2.3 Create `rust/aria-database/src/search.rs` — FTS5 MATCH + BM25 ranking + multi-filter (BPM, key, category, tags) + sort
- [x] 2.4 Create `rust/aria-filesystem/src/metadata.rs` — symphonia: duration, sample rate, bit depth, LUFS
- [x] 2.5 Modify `rust/aria-filesystem/scanner.rs` + `watcher.rs` — walkdir, notify debounce, SHA-256 hash

## Phase 3: Integration / Wiring

- [x] 3.1 Create `rust/aria-bridge/src/db_ffi.rs` + `lib.rs` — `aria_db_*` FFI exports (open/close/migrate/CRUD/search/tags/folders/maintenance)
- [x] 3.2 Create `rust/aria-bridge/src/fs_ffi.rs` + `scan_progress.rs` — scanner create/run/destroy, watcher start/stop
- [x] 3.3 Create `src/browser/browser_engine.h/.cc` — init DB, migrations, watchers, ServiceLocator reg
- [x] 3.4 Create `src/browser/search_provider.h/.cc` — wraps search FFI, thread pool, result cache
- [x] 3.5 Create `src/browser/preview_player.h/.cc` — symphonia decode, ring buffer, play/stop/volume
- [x] 3.6 Create `src/browser/waveform_cache.h/.cc` — two-tier LRU (mem 100 + .wvc disk), FFI waveform gen
- [x] 3.7 Create `src/browser/browser_panel.h/.cc` — ImGui tree/category/search views, preview panel, drag source
- [x] 3.8 Modify CMakeLists.txt — browser source group, aria-bridge staticlib, test targets
- [x] 3.9 Modify `src/project/project_manager.cc` — implement `create_audio_clip()`  *(PR 5)*
- [x] 3.10 Wire browser DnD → arrangement hit-test → `ProjectManager::create_audio_clip()`  *(PR 5)*

## Phase 4: Testing

- [x] 4.1 Create `tests/test_db_integration.cpp` — Catch2: DB open, migration, CRUD, FTS5, tag joins
- [x] 4.2 Create `tests/test_browser.cpp` — Catch2: engine init, search query, preview lifecycle, cache hit/miss
- [x] 4.3 Create `tests/test_watcher_manual.cpp` — manual: file create/modify/delete detection per platform
- [x] 4.4 Run `cargo test` — all Rust crates: 132 tests pass across aria-database (78), aria-filesystem (41), aria-bridge (13)
- [x] 4.5 Add multi-entity search (plugins, projects, presets) + FFI exports + tests
- [x] 4.6 Add search performance benchmark (<50ms for 1000 samples)
