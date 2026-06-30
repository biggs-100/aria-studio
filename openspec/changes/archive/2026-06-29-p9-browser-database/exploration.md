## Exploration: P9 — Browser & Database

### Current State

The ARIA DAW codebase already has **substantial scaffolding** for P9. The Rust workspace at `rust/` contains three crates directly relevant:

1. **`aria-database`** — SQLite-powered database with schema definitions, query types, and migration infrastructure. Schema already defines tables for `samples`, `plugins`, `projects`, `presets`, `tags`, `sample_tags`, `plugin_tags`, `watched_folders`, and `search_history`. All implementations are currently stubs (no real SQLite dependency added yet).

2. **`aria-filesystem`** — File scanner, file watcher, and file type detection (`FileInfo` with `is_audio()`, `is_plugin()`, `is_project()`). All implementations are stubs.

3. **`aria-networking`** — Sync and protocol modules for future collaboration features. Stubs only.

The **Rust C FFI bridge** at `rust/include/aria_rust.h` already exports:
- `aria_db_open`, `aria_db_close`, `aria_db_run_migrations`
- `aria_db_search_samples` with a `SampleResult` struct
- `aria_fs_is_audio`, `aria_fs_is_plugin`

**C++ source** (`src/`):
- `src/browser/` — **exists but is empty** (0 files). This is where the browser UI engine would go.
- `src/project/` — has `Project` model with stubs (`project.cc`/`project.h`) and `ProjectManager` with full track/clip/undo orchestration.
- `src/audio/` — fully implemented Audio Engine (P1) with `AudioEngine`, `AudioGraph`, transport, metering, export, etc.
- `src/model/` — `AudioClip` has `file_path()`, `file_hash()`, and `WaveformCache` struct. `Track` has `add_clip()`.
- `src/kernel/` — `ServiceLocator` and `EventBus` for service registration and inter-module communication.

**Integration points** already exist:
- `ServiceLocator` — browser engine would register as a service
- `EventBus` — filesystem events, search results, indexing progress
- `AudioClip` — references sample file paths
- `ProjectManager::create_audio_clip(TrackID, uint64_t, const std::string& file)` — direct API for drag-and-drop

**Openspec artifacts**: P0-P4 changes exist as completed SDD artifacts. P5-P8 are in the archive. The `openspec/config.yaml` confirms TDD is enabled with Catch2 v3.7.0.

### Affected Areas

| Path | Why Affected |
|------|-------------|
| `rust/aria-database/` | Core database: need real SQLite integration (rusqlite), FTS5 search, prepared statements |
| `rust/aria-database/src/schema.rs` | Schema is complete but may need minor refinements (BPM confidence, key confidence fields) |
| `rust/aria-database/src/queries.rs` | Needs real implementation: `search_samples()`, filtered search, tag-based search |
| `rust/aria-database/src/migration.rs` | Needs real SQLite migration runner (currently just lists migrations) |
| `rust/aria-database/Cargo.toml` | Needs `rusqlite` with `bundled`+`fts5` features, `walkdir`, `sha2`, `zstd` |
| `rust/aria-filesystem/src/scanner.rs` | Real recursive file scanning with `walkdir`, metadata extraction |
| `rust/aria-filesystem/src/watcher.rs` | Platform-specific file watching (inotify/FSEvents/ReadDirectoryChangesW) |
| `rust/aria-filesystem/Cargo.toml` | Needs `walkdir`, `notify` crate for file watching |
| `rust/aria-filesystem/src/lib.rs` | May need metadata extraction traits |
| `rust/include/aria_rust.h` | New FFI functions for scanner control, watcher events, waveform cache, metadata queries |
| `CMakeLists.txt` | Browser source group, Rust FFI updates, new dependencies |
| `src/browser/` | **Empty directory** — needs the browser engine: `browser_engine.h/cc`, `search_provider`, `preview_player`, `file_browser` |
| `src/audio/` | Audio preview playback — needs a `PreviewPlayer` that reads audio files and plays through audio graph |
| `src/project/` | Drag-and-drop creates clips — already has `create_audio_clip()` API |
| `src/model/` | `AudioClip` may need richer metadata fields for browser integration |
| `src/kernel/` | Service registration for browser engine in `ServiceLocator`; event types for filesystem events |
| `openspec/changes/p9-browser-database/` | New SDD change folder for this phase |
| `vendor/` | May need audio analysis library (e.g., `libsndfile` for metadata reading) |

### Approaches

#### 1. Database Layer: SQLite Integration

- **Approach A: Incremental real crate** — Add `rusqlite` with bundled SQLite + FTS5 to the `aria-database` crate. Implement migration runner, prepared statement cache, and query builder incrementally.
  - Pros: Self-contained, standard Rust SQLite, FTS5 built-in, well-documented
  - Cons: Bundled SQLite adds ~2MB to binary
  - Effort: **Medium**

- **Approach B: Use C++ SQLite directly** — Skip Rust for the database layer entirely. Use the vendored SQLite in `vendor/sqlite` from C++ directly.
  - Pros: No FFI overhead for database operations, single language
  - Cons: Violates architecture decision (Rust for database), loses Rust safety guarantees for schema management, no FTS5 via vendored SQLite without recompile
  - Effort: **Low**

- **Approach C: Hybrid — Rust for indexing/scanner, C++ for query** — Rust handles file scanning + metadata extraction via FFI; C++ manages the SQLite db directly.
  - Pros: Keeps compute-heavy scanning in Rust, query latency is direct
  - Cons: Schema duplicated across languages, migration sync complexity
  - Effort: **Medium**

**Recommendation**: **Approach A** — aligns with architecture spec, matches existing stubs, and FTS5 via rusqlite is battle-tested.

#### 2. File Scanner

- **Approach A: walkdir in Rust** — Use the `walkdir` crate for recursive directory traversal. Runs as a background thread in Rust, calls back to C++ via FFI callbacks.
  - Pros: Cross-platform, efficient, async-capable
  - Effort: **Low**

- **Approach B: std::filesystem in C++** — Use C++17 `std::filesystem::recursive_directory_iterator`.
  - Pros: No FFI needed for the scan loop
  - Cons: Loses Rust memory safety, blocks the calling thread, harder to report progress
  - Effort: **Low**

**Recommendation**: **Approach A** — `walkdir` is already planned in the spec comments in `scanner.rs`. Natural fit.

#### 3. File Watcher

- **Approach A: notify crate** — Use the `notify` Rust crate which wraps inotify/FSEvents/ReadDirectoryChangesW.
  - Pros: Cross-platform, debounced events, battle-tested in file managers and editors
  - Effort: **Low**

- **Approach B: Platform-specific C++** — Use `CFRunLoopSource` (macOS), `inotify` (Linux), `ReadDirectoryChangesW` (Windows) directly.
  - Pros: Full control, no FFI
  - Cons: Triple the implementation, easy to get edge cases wrong
  - Effort: **High**

**Recommendation**: **Approach A** — `notify` crate handles platform complexity. Events bridge to C++ via FFI callbacks or EventBus publication.

#### 4. Metadata Extraction / Audio Analysis

- **Approach A: Rust with symphonia** — Use the `symphonia` Rust crate for audio file decoding and metadata extraction (duration, sample rate, bit depth, channels). Implement BPM/key/loudness analysis in Rust.
  - Pros: Pure Rust, no external C deps, wide format support
  - Cons: Symphonia is relatively young, may have format gaps
  - Effort: **High**

- **Approach B: C++ with libsndfile** — Use `libsndfile` (C library) for basic metadata and decoding, implement BPM/key analysis in C++.
  - Pros: Mature, widely used in audio software
  - Cons: External C dependency, licensing (LGPL)
  - Effort: **Medium**

- **Approach C: Hybrid — libsndfile FFI from Rust** — Use `libsndfile-sys` rust bindings for decoding, pure Rust for analysis (autocorrelation BPM, Krumhansl key detection).
  - Pros: Best of both — mature decoder + safe analysis
  - Cons: FFI complexity, libsndfile LGPL concerns
  - Effort: **Medium**

**Recommendation**: **Approach A** initially (symphonia for basic metadata), then **C** if analysis quality requires libsndfile.

#### 5. Audio Preview Player

- **Approach A: Browser-owned decoder** — The browser engine opens the audio file, decodes it, and feeds samples to a dedicated preview audio output (not through the main mix).
  - Pros: Independent of main audio graph, can preview while project is playing
  - Cons: Duplicates audio file I/O, needs own audio output path
  - Effort: **Medium**

- **Approach B: Audio Engine integration** — Route preview through the main AudioEngine via a special "preview track" or dedicated node that plays decoded audio.
  - Pros: Shares audio engine infrastructure, can route to any output
  - Cons: Couples browser to audio graph, potential latency
  - Effort: **Medium**

**Recommendation**: **A** (independent preview) for simplicity, with optional routing to main engine for advanced use.

#### 6. Browser UI (Provisional)

- **Approach A: ImGui window** — Render the file browser, search results, and preview panel using ImGui (already available in the build).
  - Pros: Immediate-mode, fast to prototype, no GPU framework dependency
  - Cons: Not the final UI (will be replaced in P10 with GPU framework)
  - Effort: **Low**

- **Approach B: Standalone widget library** — Build provisional widgets over Skia/WebGPU directly.
  - Pros: Might reuse some in P10
  - Cons: Premature — P10 will define the actual widget system
  - Effort: **High**

**Recommendation**: **Approach A** — per user decision, provisional UI via imgui. Refined in P10.

### Recommendation

**Adopt all first-listed approaches** as they form a coherent, architecture-aligned strategy:

| Component | Approach | Key Crate/Tool |
|-----------|----------|---------------|
| Database | Incremental real crate (rusqlite + FTS5) | `rusqlite` (bundled, fts5) |
| File Scanner | walkdir in Rust | `walkdir` |
| File Watcher | notify crate | `notify` |
| Metadata Extraction | symphonia (basic) → hybrid (advanced) | `symphonia` or `libsndfile-sys` |
| Audio Preview | Browser-owned decoder | `symphonia` / `libsndfile` |
| Browser UI | ImGui provisional | `imgui` (already in C++ deps) |
| BPM Analysis | Autocorrelation + comb filter in Rust | Custom algorithm |
| Key Detection | Krumhansl-Schmuckler in Rust | Custom algorithm |

**Build integration**: The Rust workspace already exists. The `aria_rust` CMake INTERFACE target is ready. Adding `rusqlite` to `aria-database/Cargo.toml` and linking it is straightforward. The `src/browser/` engine will register via `ServiceLocator`.

### Risks

1. **Rust → C++ FFI evolution** — The current FFI header (`aria_rust.h`) has primitive types. Adding scanner progress callbacks, watcher events, and waveform data will require richer FFI types (callback function pointers, complex struct serialization). Risk of ABI mismatch.

2. **rusqlite bundled SQLite** — Using `rusqlite` with `bundled` feature compiles SQLite from source. This is fine but adds ~2MB per platform and increases compile time. If the project already has a vendored SQLite used by C++, there could be version skew.

3. **Analysis accuracy** — BPM detection and key detection are hard. Autocorrelation works for drum loops but fails for polyphonic material. Need a tiered approach: use embedded metadata tags first (`bpm`, `key` in file headers), fall back to analysis.

4. **Platform file watching quirks** — `ReadDirectoryChangesW` on Windows has well-known limitations (network drives, buffer overflow). `notify` crate handles most but not all edge cases. Need fallback to periodic scanning.

5. **Large library performance** — The spec targets <50ms search for 100k samples. FTS5 with BM25 ranking is fast, but joined queries (tags + FTS + category + BPM range) need careful indexing. Schema already has the right indexes (`idx_samples_category`, `idx_samples_bpm`, `idx_samples_key`).

6. **Concurrent indexing** — File scanning and real-time audio processing share the same thread pool. The scanner runs in Rust background threads; the audio engine runs on its own high-priority thread. Need to ensure scanner threads don't steal resources from audio threads. Use OS thread priority control.

7. **Database migration conflicts** — Migration system must handle concurrent access (WAL mode). If two instances of ARIA open the same database, migrations must be idempotent and lock-protected.

8. **Drag-and-drop protocol** — Dropping a sample from the browser onto a track requires: (a) identifying the target track from screen coordinates, (b) computing the clip position from the timeline position, (c) creating an AudioClip with the file path, (d) adding it to the track. The `ProjectManager::create_audio_clip()` API handles (c) and (d), but (a) and (b) need arrangement view integration.

### Ready for Proposal

**Yes**. The codebase is well-positioned with existing scaffolding. Key clarity points for the proposal:

1. **SQLite dependency strategy**: Bundled in Rust crate vs system SQLite
2. **FFI evolution plan**: How to handle growing Rust→C++ API surface (callbacks for progress, events)
3. **Analysis algorithms**: Which specific algorithms for BPM/key/category detection, and accuracy targets
4. **ImGui vs mini-browser**: Confirm provisional UI approach and integration with existing arrangement view
5. **Delivery strategy**: The workload is substantial but modular — natural splits: (a) database + migration, (b) scanner + watcher, (c) search engine, (d) browser UI + preview, (e) drag-and-drop. Each is a clean PR slice.
