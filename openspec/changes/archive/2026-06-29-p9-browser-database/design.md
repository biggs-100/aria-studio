# Design: P9 — Browser & Database

## Technical Approach

Three-layer architecture: Rust domain crates → FFI bridge (`aria-bridge` cdylib) → C++ BrowserEngine. DB, scanner, watcher, search, and waveform cache live in Rust for memory safety and cross-platform portability. C++ `BrowserEngine` owns `SearchProvider` + `PreviewPlayer`, registers via `ServiceLocator`, publishes events on `EventBus`. Provisional ImGui browser panel. Drag-and-drop calls `ProjectManager::create_audio_clip()`.

## Architecture Decisions

| Decision | Choice | Alternatives | Rationale |
|----------|--------|-------------|-----------|
| SQLite binding | rusqlite 0.31 (bundled + fts5) | System SQLite, SQLite3 C API via FFI | Bundled gives guaranteed FTS5; no system dep; one `cargo build` |
| Audio metadata | symphonia (pure Rust) | libsndfile (C dep), ffmpeg | Zero C deps, cross-platform, decodes + analyzes in one crate |
| File watching | notify 6 (Rust) | inotify/FSEvents/ReadDirChangesW direct | Already abstracts all 3 platforms; debounce + event filter built in |
| Browser UI | ImGui (provisional) | Skia direct, native widgets | Already vendored; fast to iterate; P10 replaces with Skia/WebGPU |
| BrowserEngine registration | ServiceLocator service | Singleton, direct injection | Matches existing pattern (see `src/kernel/service_locator.h`) |
| Preview audio | Browser-owned symphonia decoder | Main audio engine routing | Independent of transport; no engine state coupling |
| Bridge crate | New `aria-bridge` cdylib | Merge into aria-database crate | Separation: domain crates stay pure Rust; bridge owns `#[no_mangle]` + `#[repr(C)]` types |
| Waveform cache | Rust generation + C++ two-tier LRU | Pure C++, FFI-only | Rust reads file for hash+waveform; C++ manages LRU memory + disk cache |

## Data Flow

```
User Input
    │
    ▼
┌──────────────────────┐
│  ImGui Browser Panel  │  (src/browser/browser_panel.cc)
│  (subscribe EventBus) │
└──────┬───────────────┘
       │ query / select / drag
       ▼
┌──────────────────────┐
│  C++ BrowserEngine    │  (src/browser/browser_engine.cc)
│  ┌─────────────────┐  │
│  │ SearchProvider   │──┼──→ EventBus: kSearchResult, kIndexProgress
│  │ PreviewPlayer    │──┼──→ Audio output (separate from transport)
│  │ WaveformCache    │  │
│  └────────┬────────┘  │
└───────────┼───────────┘
            │ FFI call (C ABI)
            ▼
┌──────────────────────────────┐
│  aria-bridge cdylib (Rust)   │  (rust/aria-bridge/src/)
│  #[no_mangle] extern "C"     │
│  ┌──────────┬─────────────┐  │
│  │ DB FFI   │ Filesystem  │  │
│  │ (rusqlite│ FFI         │  │
│  │  +FTS5)  │ (walkdir,   │  │
│  │          │  notify,    │  │
│  │          │  symphonia) │  │
│  └──────────┴─────────────┘  │
└──────────────────────────────┘
            │
            ▼
┌──────────────────────┐
│  SQLite DB (WAL)      │  (config_dir/aria.db)
│  samples_fts FTS5     │
└──────────────────────┘

Drag-and-drop path:
BrowserEngine → hit-test screen coords → Track from arrangement
→ ProjectManager::create_audio_clip(track, start_ppqn, file_path)
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `rust/aria-bridge/Cargo.toml` | Create | New cdylib crate: cdylib+staticlib, depends on aria-database, aria-filesystem, symphonia, sha2 |
| `rust/aria-bridge/src/lib.rs` | Create | `#[no_mangle] extern "C"` exports: DB open/close/migrate/CRUD/search, scanner, watcher |
| `rust/aria-bridge/src/db_ffi.rs` | Create | DB FFI: `aria_db_*` functions wrapping Database struct |
| `rust/aria-bridge/src/fs_ffi.rs` | Create | Filesystem FFI: scanner create/scan (with progress callback), watcher start/stop |
| `rust/aria-bridge/src/scan_progress.rs` | Create | Progress callback typedef + scanner progress reporting |
| `rust/aria-database/Cargo.toml` | Modify | Add rusqlite 0.31 (bundled+fts5), zstd 0.13 |
| `rust/aria-database/src/lib.rs` | Modify | Replace stub: real `Database` struct with rusqlite `Connection`, WAL pragma |
| `rust/aria-database/src/schema.rs` | Modify | Add FTS5 tables, indexes, sample_analysis, presets, search_history |
| `rust/aria-database/src/migration.rs` | Modify | Real migration runner: ordered up/down, backup before destructive, schema_version tracking |
| `rust/aria-database/src/queries.rs` | Modify | Real CRUD: prepared statements, batch inserts, tag join queries |
| `rust/aria-database/src/search.rs` | Create | SearchEngine: FTS5 MATCH, BM25 ranking, multi-filter (BPM, key, category, tags), sort |
| `rust/aria-database/src/errors.rs` | Create | Typed `DbError` enum: Sqlite, Migration, NotFound, Duplicate |
| `rust/aria-filesystem/Cargo.toml` | Modify | Add symphonia (all features), walkdir 2, sha2 0.10, crossbeam-channel |
| `rust/aria-filesystem/src/lib.rs` | Modify | Add `AudioMetadata` struct, `ScannedFile` result type |
| `rust/aria-filesystem/src/scanner.rs` | Modify | Real walkdir traversal, file type by extension, symphonia metadata extraction, SHA-256 hash |
| `rust/aria-filesystem/src/watcher.rs` | Modify | Real notify integration, debounce (crossbeam-channel + timer), event types |
| `rust/aria-filesystem/src/metadata.rs` | Create | symphonia-based: duration, sample rate, bit depth, channels, LUFS peak estimation |
| `rust/include/aria_rust.h` | Modify | New types: `DbSearchQuery`, `ScannedFileResult`, `ScanProgressCallback`, `WatchEvent`; new functions: `aria_db_*`, `aria_fs_scanner_*`, `aria_fs_watcher_*` |
| `src/browser/browser_engine.h` | Create | `BrowserEngine` : owns SearchProvider, PreviewPlayer, WaveformCache; registers in ServiceLocator |
| `src/browser/browser_engine.cc` | Create | Init: open DB, run migrations, start watchers. Publish events via EventBus |
| `src/browser/search_provider.h` | Create | Wraps `aria_db_search_samples` FFI, builds `SearchQuery` structs, parses `SampleResult` arrays |
| `src/browser/search_provider.cc` | Create | Async query execution via thread pool, result caching |
| `src/browser/preview_player.h` | Create | Browser-owned audio preview: play/stop/pause/volume, symphonia decoder output |
| `src/browser/preview_player.cc` | Create | Audio output ring buffer, transport-independent playback |
| `src/browser/waveform_cache.h` | Create | Two-tier: LRU memory (100 entries) + disk cache (`[config_dir]/waveform_cache/`) |
| `src/browser/waveform_cache.cc` | Create | Calls `aria_fs_get_waveform` FFI; binary format `.wvc` keyed by SHA-256 |
| `src/browser/browser_panel.h` | Create | ImGui panel: tree/category/search views, preview panel, drag source |
| `src/browser/browser_panel.cc` | Create | ImGui draw calls, event subscriptions, drag-drop payload |
| `src/kernel/event_types.h` | Create | `BrowserEvent` enum: `kFileScanned`, `kFileChanged`, `kIndexProgress`, `kSearchResult`, `kPreviewLoaded` |
| `src/project/project_manager.cc` | Modify | Implement `create_audio_clip()` — was stub |
| `CMakeLists.txt` | Modify | Add `src/browser/*` source group, link `aria-bridge` staticlib |
| `tests/CMakeLists.txt` | Modify | Add `test_browser` and `test_db_integration` test targets |
| `tests/test_browser.cpp` | Create | Catch2: BrowserEngine init, SearchProvider query, PreviewPlayer lifecycle, WaveformCache hit/miss |
| `tests/test_db_integration.cpp` | Create | Catch2: DB open, migration, CRUD, FTS5 query, tag join, batch insert |
| `tests/test_watcher_manual.cpp` | Create | Manual test: file create/modify/delete detection (requires fs access) |

## Interfaces / Contracts

```c
// Extended FFI (aria_rust.h additions):
typedef void (*ScanProgressCallback)(int64_t indexed, int64_t total, void* userdata);

DbConnection* aria_db_open(const char* path);
bool aria_db_run_migrations(DbConnection* conn);
int64_t aria_db_insert_sample(DbConnection* conn, const SampleEntry* entry);
int32_t aria_db_search_samples(DbConnection* conn, const DbSearchQuery* query,
                               SampleResult* results, int32_t max);
void aria_db_tag_sample(DbConnection* conn, int64_t sample_id, const char* tag);

void* aria_fs_scanner_create(const char* const* paths, int32_t count);
void  aria_fs_scanner_run(void* scanner, ScanProgressCallback cb, void* userdata);
void  aria_fs_scanner_destroy(void* scanner);

void* aria_fs_watcher_create(const char* const* paths, int32_t count);
bool  aria_fs_watcher_start(void* watcher, WatchEventCallback cb, void* userdata);
void  aria_fs_watcher_stop(void* watcher);
bool  aria_fs_get_waveform(const char* path, uint32_t resolution,
                           float* out_peaks, float* out_minima, int32_t* out_len);
```

```cpp
// C++ BrowserEngine API:
class BrowserEngine {
    bool init(const std::string& db_path);      // Open DB, run migrations
    void add_watched_folder(const std::string& path);
    void remove_watched_folder(const std::string& path);
    void start_scan();                           // Background scan + index
    SearchProvider* search();                    // FTS5 query builder
    PreviewPlayer*  preview();                   // Audio preview player
    WaveformCache*  waveforms();                 // Waveform thumbnail cache
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Rust unit | DB CRUD, FTS5 queries, migration ordering, scanner file type detection, hash correctness | `cargo test` — rusqlite `:memory:`, walkdir temp dirs |
| Rust unit | symphonia metadata extraction (duration, sample rate, BPM header) | `cargo test` — known test WAV files in `rust/aria-filesystem/tests/fixtures/` |
| Rust integration | Scanner + DB pipeline: scan dir → insert → search → verify | `cargo test` — real temp dir with 10-100 files |
| C++ unit (Catch2) | BrowserEngine init lifecycle, SearchProvider query building, PreviewPlayer play/stop/volume, WaveformCache LRU eviction + disk hit/miss | `tests/test_browser.cpp` — mock FFI for isolated C++ tests |
| C++ integration (Catch2) | FFI round-trip: C++ calls `aria_db_*` → Rust processes → results back to C++ structs | `tests/test_db_integration.cpp` — real Rust lib linked |
| Manual | File watcher create/modify/delete on all 3 platforms | `tests/test_watcher_manual.cpp` — requires fs events |

## Migration / Rollout

**No persistent data migration** — the SQLite DB is created fresh on first launch. Schema is additive only in v1. Future versions add migrations. 5 chained PRs: (1) DB + migrations, (2) scanner + watcher + metadata, (3) search engine, (4) browser UI + preview, (5) drag-and-drop.
