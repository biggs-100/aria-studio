# ARIA DAW — File System & Browser Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: Rust (database, indexing) + C++ (browser UI)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Sample Database](#3-sample-database)
4. [File Indexing](#4-file-indexing)
5. [Search Engine](#5-search-engine)
6. [File Browser](#6-file-browser)
7. [Plugin Database](#7-plugin-database)
8. [Waveform Cache](#8-waveform-cache)
9. [Project File Management](#9-project-file-management)
10. [API Reference](#10-api-reference)
11. [RFC Index](#11-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The File System & Browser subsystem manages all file-related operations in ARIA DAW. It provides a SQLite-powered database for indexing samples, plugins, projects, and presets, with full-text search, tag management, and auto-indexing of watched folders.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Full-text search | < 100ms for 100k samples |
| Initial indexing | < 1s per 10k files |
| Auto-indexing | Background, < 5% CPU |
| Waveform cache | Load in < 50ms (from cache) |
| Browser responsiveness | 60 FPS scrolling with 10k files |

### 1.3. Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    File System & Browser                      │
│                                                               │
│  ┌─────────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ File Scanner    │  │ Indexer      │  │ Watch Dog      │  │
│  │ (discovery)     │  │ (metadata)   │  │ (file watcher) │  │
│  └────────┬────────┘  └──────┬───────┘  └───────┬────────┘  │
│           │                  │                   │           │
│  ┌────────▼──────────────────▼───────────────────▼────────┐  │
│  │                    SQLite Database                       │  │
│  │  samples | plugins | projects | presets | waveforms     │  │
│  └────────────────────────────┬────────────────────────────┘  │
│                               │                                │
│  ┌────────────────────────────▼────────────────────────────┐  │
│  │                    Search Engine                          │  │
│  │  FTS5 full-text | tag filter | category | metadata       │  │
│  └────────────────────────────┬────────────────────────────┘  │
│                               │                                │
│  ┌────────────────────────────▼────────────────────────────┐  │
│  │                    File Browser UI                        │  │
│  │  Tree | List | Search | Preview | Drag & Drop            │  │
│  └─────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Architecture

### 2.1. Module Responsibilities

| Component | Language | Responsibility |
|---|---|---|
| Database schema | Rust | SQLite schema, migrations, queries |
| File scanner | Rust | Recursive directory scanning, file type detection |
| Metadata extractor | Rust | Audio file metadata (duration, BPM, key, etc.) |
| Indexer | Rust | Insert/update database records |
| File watcher | Rust | Monitor folders for changes (inotify/FSEvents/ReadDirectoryChanges) |
| Search engine | Rust | FTS5 queries, tag filtering, ranking |
| Waveform cache | Rust | Generate and cache waveform data |
| Browser UI | C++/TS | File tree, search results, preview panel, drag-drop |

### 2.2. SQLite Database

```cpp
// Database location:
//   Windows: %APPDATA%/ARIA/aria.db
//   macOS: ~/Library/Application Support/ARIA/aria.db
//   Linux: ~/.config/aria/aria.db
//
// Separate database per user (not per project).
// Contains indices for all user content across all projects.

class AriaDatabase {
public:
    static AriaDatabase& instance();
    
    bool open(const std::string& path);
    void close();
    bool is_open() const;
    
    // Migrations
    bool migrate();
    uint32_t current_version() const;
    
    // Access to sub-databases
    SampleDB& samples();
    PluginDB& plugins();
    ProjectDB& projects();
    PresetDB& presets();
    WaveformCache& waveforms();
    
    // Maintenance
    bool vacuum();
    bool backup(const std::string& path);
    bool integrity_check();

private:
    sqlite3* db_ = nullptr;
    
    // Sub-database managers
    std::unique_ptr<SampleDB> samples_;
    std::unique_ptr<PluginDB> plugins_;
    std::unique_ptr<ProjectDB> projects_;
    std::unique_ptr<PresetDB> presets_;
    std::unique_ptr<WaveformCache> waveforms_;
    
    // Schema version
    static constexpr uint32_t SCHEMA_VERSION = 1;
};
```

---

## 3. Sample Database

### 3.1. Schema

```sql
-- Main sample table
CREATE TABLE samples (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path   TEXT NOT NULL UNIQUE,
    file_hash   TEXT NOT NULL,            -- SHA-256
    file_size   INTEGER NOT NULL,
    file_format TEXT NOT NULL,            -- wav, aif, flac, mp3, ogg
    sample_rate INTEGER,
    bit_depth   INTEGER,
    channels    INTEGER,
    duration_ms REAL,
    
    -- Analysis data
    bpm         REAL,                     -- Detected BPM (0 = unknown)
    key         TEXT,                     -- Detected key ("Cm", "F#m", etc.)
    key_confidence REAL,                 -- Confidence 0.0-1.0
    loudness    REAL,                     -- Integrated LUFS
    peak        REAL,                     -- Peak level dB
    
    -- File metadata
    created_at  TEXT,                     -- File creation time
    modified_at TEXT,                     -- File modification time
    indexed_at  TEXT DEFAULT CURRENT_TIMESTAMP,
    
    -- User metadata
    name        TEXT,                     -- User-friendly name
    rating      INTEGER DEFAULT 0,       -- 0-5 stars
    tags        TEXT,                     -- Comma-separated tags
    favorite    INTEGER DEFAULT 0,        -- Boolean
    color       TEXT,                     -- User-assigned color
    notes       TEXT,                     -- User notes
    
    -- Category (auto-detected)
    category    TEXT,                     -- kick, snare, hihat, bass, pad, fx, etc.
    subcategory TEXT
);

-- FTS5 full-text search index
CREATE VIRTUAL TABLE samples_fts USING fts5(
    name, tags, notes, category, subcategory,
    content='samples',
    content_rowid='id'
);

-- Indexes for common queries
CREATE INDEX idx_samples_category ON samples(category);
CREATE INDEX idx_samples_bpm ON samples(bpm);
CREATE INDEX idx_samples_key ON samples(key);
CREATE INDEX idx_samples_favorite ON samples(favorite);
CREATE INDEX idx_samples_file_hash ON samples(file_hash);

-- Watched folders
CREATE TABLE watched_folders (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    path        TEXT NOT NULL UNIQUE,
    recursive   INTEGER DEFAULT 1,
    auto_index  INTEGER DEFAULT 1,
    last_scanned_at TEXT
);

-- Tags (normalized)
CREATE TABLE tags (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,
    color       TEXT
);

-- Sample-tag mapping
CREATE TABLE sample_tags (
    sample_id   INTEGER REFERENCES samples(id) ON DELETE CASCADE,
    tag_id      INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (sample_id, tag_id)
);
```

### 3.2. Sample Analysis

```cpp
class SampleAnalyzer {
public:
    struct AnalysisResult {
        float bpm = 0.0f;
        float bpm_confidence = 0.0f;
        std::string key;              // "Cm", "F", etc.
        float key_confidence = 0.0f;
        float loudness_lufs = 0.0f;
        float peak_db = 0.0f;
        float duration_ms = 0.0f;
        std::string category;         // Auto-detected category
        std::string subcategory;
        std::vector<float> waveform_peaks;  // Downsampled for preview
        std::vector<float> spectral_centroid;
    };
    
    // Analyze a single file
    std::optional<AnalysisResult> analyze(const std::string& path);
    
    // Category detection based on spectral/temporal features
    std::string detect_category(const AnalysisResult& result);
    
    // Key detection (Krumhansl-Schmuckler algorithm)
    std::string detect_key(const float* samples, uint32_t length, float sample_rate);
    
    // BPM detection (autocorrelation + comb filter)
    float detect_bpm(const float* samples, uint32_t length, float sample_rate);
    
    // Cancel running analysis
    void cancel();
    
private:
    std::atomic<bool> cancelled_{false};
    ThreadPool analysis_threads_;
};
```

---

## 4. File Indexing

### 4.1. Indexer

```cpp
class FileIndexer {
public:
    FileIndexer();
    
    // Start indexing watched folders
    void start_indexing();
    void stop_indexing();
    
    // Index a specific path
    void index_path(const std::string& path, bool recursive);
    
    // Remove deleted files from database
    void remove_missing_files();
    
    // Index status
    struct IndexStatus {
        bool is_indexing = false;
        uint32_t files_indexed = 0;
        uint32_t files_skipped = 0;
        uint32_t files_failed = 0;
        uint32_t total_discovered = 0;
        double progress = 0.0;          // 0.0-1.0
        std::string current_file;
        std::chrono::seconds elapsed;
        std::chrono::seconds estimated_remaining;
    };
    IndexStatus status() const;
    
    // Events
    std::function<void(const std::string& path)> on_file_indexed;
    std::function<void(const std::string& path, const std::string& error)> on_file_failed;
    std::function<void()> on_indexing_complete;

private:
    std::atomic<bool> indexing_{false};
    IndexStatus status_;
    
    // File discovery queue
    ThreadPool discovery_threads_;
    ThreadPool analysis_threads_;
    
    // Supported audio formats
    static const std::unordered_set<std::string> SUPPORTED_FORMATS;
};
```

### 4.2. File Watcher

```cpp
class FileWatcher {
public:
    FileWatcher();
    
    // Watch a folder for changes
    void add_watch(const std::string& path);
    void remove_watch(const std::string& path);
    void clear_watches();
    
    // Start/stop watching
    void start();
    void stop();
    
    // Events
    std::function<void(const std::string& path)> on_file_created;
    std::function<void(const std::string& path)> on_file_modified;
    std::function<void(const std::string& path)> on_file_deleted;
    std::function<void(const std::string& path)> on_file_renamed;

private:
    // Platform-specific watchers:
    // Windows: ReadDirectoryChangesW
    // macOS: FSEvents
    // Linux: inotify
    
    std::unique_ptr<PlatformWatcher> watcher_;
    std::vector<std::string> watched_paths_;
    bool running_ = false;
    
    // Debounce for rapid changes
    Debouncer debouncer_;
};
```

### 4.3. Supported File Types

```cpp
// Audio files:
const std::unordered_set<std::string> AUDIO_EXTENSIONS = {
    ".wav", ".aiff", ".aif", ".flac", ".mp3", ".ogg",
    ".m4a", ".wma", ".aac", ".wv", ".opus"
};

// Plugin files:
const std::unordered_set<std::string> PLUGIN_EXTENSIONS = {
    ".clap", ".vst3", ".vst", ".component", ".lv2"
};

// Project files:
const std::unordered_set<std::string> PROJECT_EXTENSIONS = {
    ".aria", ".aria.auto", ".aria.bak", ".aria.template"
};

// Preset files:
const std::unordered_set<std::string> PRESET_EXTENSIONS = {
    ".clap-preset", ".fxp", ".fxb", ".vstpreset", ".aupreset"
};
```

---

## 5. Search Engine

### 5.1. Full-Text Search

```cpp
class SearchEngine {
public:
    struct SearchQuery {
        std::string text;                // Free-text search
        std::vector<std::string> tags;   // Filter by tags
        std::vector<std::string> categories;
        std::vector<std::string> formats;
        double bpm_min = 0;
        double bpm_max = 999;
        std::string key;                 // Specific key
        bool favorite_only = false;
        int rating_min = 0;
        std::string sort_by = "relevance";  // relevance, name, date, bpm, duration
        bool sort_ascending = false;
        uint32_t limit = 100;
        uint32_t offset = 0;
    };
    
    struct SearchResult {
        std::vector<SampleEntry> samples;
        uint32_t total_count;
        double time_ms;
    };
    
    // Search samples
    SearchResult search_samples(const SearchQuery& query);
    
    // Search plugins
    SearchResult search_plugins(const SearchQuery& query);
    
    // Search projects
    SearchResult search_projects(const SearchQuery& query);
    
    // Search presets
    SearchResult search_presets(const SearchQuery& query);
    
    // Semantic search (via AI engine)
    struct SemanticQuery {
        std::string description;     // "dark kick with sub-bass"
        uint32_t limit = 20;
    };
    SearchResult semantic_search(const SemanticQuery& query);

private:
    // FTS5 query construction
    std::string build_fts_query(const std::string& text) const;
    
    // SQL query construction from SearchQuery
    std::pair<std::string, std::vector<sqlite3_value*>>
    build_sample_query(const SearchQuery& query) const;
};
```

### 5.2. Search Examples

```
Text search:
  Query: "dark kick"
  Matches: "dark_kick.wav", "deep_dark_kick.wav", "kick_dark_room.wav"
  (via FTS5 on name, tags, notes)

Filtered search:
  Query: "pad" + category:pad + bpm:80-120 + favorite:true
  Matches: warm_pad.wav, ambient_pad.wav

Semantic search (AI):
  Query: "bright snare with long reverb"
  Matches: snare_hall.wav, snare_plate.wav, snare_verb.wav
```

---

## 6. File Browser

### 6.1. Browser Panel

```
┌──────────────────────────────────────────────────────────────┐
│  Browser │ 🔍 Search...        |  [Samples]  [Files]  [Fav] │
├──────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ 🔉 My Samples                         ▶ 2,340 files   │ │
│  │  ├── Kicks                           ▶ 234 files      │ │
│  │  ├── Snares                          ▶ 156 files      │ │
│  │  ├── HiHats                          ▶ 312 files      │ │
│  │  ├── Percussion                      ▶ 89 files       │ │
│  │  ├── Bass                            ▶ 167 files      │ │
│  │  ├── Synth                           │                 │ │
│  │  │  ├── Pads                         │ 89 files        │ │
│  │  │  ├── Leads                        │ 67 files        │ │
│  │  │  └── Plucks                       │ 45 files        │ │
│  │  ├── FX                              ▶ 234 files      │ │
│  │  └── Loops                           ▶ 156 files      │ │
│  ├── 🎹 Plugins                         ▶ 89 installed   │ │
│  ├── 💿 Projects                        ▶ 23 projects    │ │
│  └── ⭐ Favorites                       ▶ 45 files       │ │
├──────────────────────────────────────────────────────────────┤
│  Preview:                                                    │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  ╱╲    ╱╲    ╱╲                                        │  │
│  │ ╱  ╲  ╱  ╲  ╱  ╲                                       │  │
│  │╱    ╲╱    ╲╱    ╲                                      │  │
│  └────────────────────────────────────────────────────────┘  │
│  Kick_Deep_01.wav                     ⭐ ★★★★☆              │
│  Size: 1.2 MB | 44.1kHz | 24-bit | Stereo                  │
│  BPM: 120 | Key: Cm | Duration: 2.4s | -14.2 LUFS          │
│  Tags: dark, sub-bass, punchy, electronic                   │
│                                                              │
│  [Add Tag ▼] [Play ▶■] [Drag to Track]                     │
└──────────────────────────────────────────────────────────────┘
```

### 6.2. Browser Features

```cpp
class FileBrowser {
public:
    // Navigation
    void set_root(const std::string& path);
    void navigate_to(const std::string& path);
    void go_up();
    void go_back();
    void go_forward();
    
    // View modes
    enum class ViewMode {
        Tree,           // Hierarchical tree
        List,           // Flat list
        Grid,           // Thumbnail grid
        Details         // Table with columns
    };
    void set_view_mode(ViewMode mode);
    
    // Sorting
    void set_sort(SortField field, bool ascending);
    
    // Filtering
    void set_filter(const BrowserFilter& filter);
    void set_search_query(const std::string& query);
    
    // Selection
    std::vector<FileEntry> selected_files() const;
    void select_all();
    void clear_selection();
    
    // Drag and drop
    bool start_drag(const FileEntry& entry);
    
    // Context menu
    void show_context_menu(const FileEntry& entry);
    
    // Preview
    void preview_file(const FileEntry& entry);
    void play_preview();           // Play audio preview
    void stop_preview();
    bool is_preview_playing() const;

private:
    // Current state
    std::string current_path_;
    ViewMode view_mode_ = ViewMode::Tree;
    BrowserFilter filter_;
    std::vector<FileEntry> current_entries_;
    std::unordered_set<std::string> selection_;
    
    // Preview
    AudioPreviewPlayer preview_player_;
    
    // File operations
    void rename_file(const FileEntry& entry, const std::string& new_name);
    void delete_file(const FileEntry& entry);
    void reveal_in_explorer(const FileEntry& entry);
};
```

### 6.3. Audio Preview

```cpp
class AudioPreviewPlayer {
public:
    // Play a preview of an audio file
    void play(const std::string& path, double start_seconds = 0);
    void stop();
    void pause();
    void resume();
    
    // Preview state
    bool is_playing() const;
    bool is_paused() const;
    double current_position() const;       // Seconds
    double duration() const;
    
    // Playback mode
    enum class Mode {
        Normal,         // Play once
        Loop,           // Loop the file
        Sync,           // Sync to project tempo (warp)
        Trigger         // Trigger on mouse down, stop on up
    };
    void set_mode(Mode mode);
    
    // Volume
    void set_volume(double db);
    double volume() const;
    
    // Output routing
    void set_output(AudioOutput output);  // Preview can go to any output
};
```

---

## 7. Plugin Database

### 7.1. Schema

```sql
CREATE TABLE plugins (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id       TEXT NOT NULL UNIQUE,   -- "vendor.name"
    name            TEXT NOT NULL,
    vendor          TEXT,
    format          TEXT NOT NULL,           -- "CLAP", "VST3", "AU", "LV2"
    version         TEXT,
    file_path       TEXT NOT NULL,
    category        TEXT,
    features        TEXT,                    -- JSON array: ["equalizer", "dynamics"]
    num_inputs      INTEGER,
    num_outputs     INTEGER,
    has_editor      INTEGER DEFAULT 0,
    is_synth        INTEGER DEFAULT 0,
    
    last_scanned    TEXT,
    last_modified   TEXT,
    scan_error      TEXT,                   -- NULL if scan succeeded
    blacklisted     INTEGER DEFAULT 0,
    favorite        INTEGER DEFAULT 0
);

CREATE VIRTUAL TABLE plugins_fts USING fts5(
    name, vendor, category, features,
    content='plugins',
    content_rowid='id'
);
```

---

## 8. Waveform Cache

### 8.1. Waveform Generation

```cpp
class WaveformCache {
public:
    // Generate waveform for a file (cached on disk)
    struct WaveformData {
        uint32_t sample_rate;
        uint32_t bits_per_sample;
        uint32_t channels;
        uint64_t total_samples;
        std::vector<float> peaks;       // Downsampled max values
        std::vector<float> minima;      // Downsampled min values
        uint32_t resolution;            // Samples per pixel
    };
    
    // Get waveform at a specific resolution
    std::optional<WaveformData> get_waveform(const std::string& file_path,
                                              uint32_t resolution = 1024);
    
    // Generate waveform data
    WaveformData generate(const std::string& file_path, uint32_t resolution);
    
    // Cache location:
    //   [ProjectDir]/.cache/waveforms/[hash].wvc
    //   or
    //   %APPDATA%/ARIA/waveform_cache/[hash].wvc
    
    struct CacheStats {
        uint64_t cached_files;
        uint64_t cache_size_bytes;
        uint64_t cache_hits;
        uint64_t cache_misses;
    };
    CacheStats stats() const;
    
    // Maintenance
    void clear_cache();
    void cleanup_orphaned();

private:
    std::string cache_directory_;
    std::unordered_map<std::string, WaveformData> memory_cache_;
    static constexpr size_t MEMORY_CACHE_SIZE = 100;  // Keep 100 in memory
    
    // Cache file format
    bool save_to_disk(const std::string& hash, const WaveformData& data);
    std::optional<WaveformData> load_from_disk(const std::string& hash);
};
```

---

## 9. Project File Management

### 9.1. Project Manager

```cpp
class ProjectManager {
public:
    // Recent projects
    std::vector<ProjectEntry> recent_projects() const;
    void add_recent_project(const std::string& path);
    void clear_recent_projects();
    
    // Templates
    std::vector<ProjectEntry> templates() const;
    bool create_from_template(const std::string& template_name,
                               const std::string& output_path);
    bool save_as_template(const std::string& name, const Project& project);
    
    // Project discovery
    std::vector<ProjectEntry> find_projects(const std::string& directory);
    
    // Default paths
    std::string default_project_directory() const;
    void set_default_project_directory(const std::string& path);
    std::string default_template_directory() const;
    
    // Project operations
    bool create_project_directory(const std::string& path,
                                   const std::string& name);
    bool duplicate_project(const std::string& source, const std::string& dest);
    bool archive_project(const std::string& path, const std::string& archive_path);

private:
    // Recent projects file (JSON)
    std::string recent_file_path() const;
    void save_recent_projects(const std::vector<ProjectEntry>& projects);
};

struct ProjectEntry {
    std::string name;
    std::string path;
    uint64_t last_opened;
    uint64_t created;
    std::string thumbnail_path;      // Screenshot
    double duration_seconds;         // Project length
};
```

---

## 10. API Reference

### 10.1. Public API

```cpp
class FileSystemAPI {
public:
    // Database
    AriaDatabase& database();
    
    // Indexing
    void index_folders(const std::vector<std::string>& paths);
    void index_folder(const std::string& path);
    void stop_indexing();
    FileIndexer::IndexStatus indexing_status() const;
    
    // Watched folders
    void add_watched_folder(const std::string& path);
    void remove_watched_folder(const std::string& path);
    std::vector<std::string> watched_folders() const;
    
    // Search
    SearchEngine& search();
    
    // Waveform cache
    WaveformCache& waveforms();
    
    // Project manager
    ProjectManager& projects();
    
    // Browser
    FileBrowser& browser();
    
    // Audio preview
    AudioPreviewPlayer& preview();
    
    // File operations
    bool copy_file(const std::string& src, const std::string& dst);
    bool move_file(const std::string& src, const std::string& dst);
    bool delete_file(const std::string& path);
    bool rename_file(const std::string& path, const std::string& new_name);
    
    // Path utilities
    bool is_supported_audio(const std::string& path) const;
    bool is_supported_plugin(const std::string& path) const;
    std::string file_hash(const std::string& path) const;
};
```

---

## 11. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-FS-001 | SQLite Schema & Migrations | 🔲 Pending |
| RFC-FS-002 | File Scanner & Discovery | 🔲 Pending |
| RFC-FS-003 | Audio Metadata Extraction | 🔲 Pending |
| RFC-FS-004 | BPM Detection Algorithm | 🔲 Pending |
| RFC-FS-005 | Key Detection Algorithm | 🔲 Pending |
| RFC-FS-006 | Category Classification | 🔲 Pending |
| RFC-FS-007 | Background Indexer | 🔲 Pending |
| RFC-FS-008 | Platform File Watcher (Win/macOS/Linux) | 🔲 Pending |
| RFC-FS-009 | FTS5 Full-Text Search | 🔲 Pending |
| RFC-FS-010 | Semantic Search Integration (AI) | 🔲 Pending |
| RFC-FS-011 | File Browser UI | 🔲 Pending |
| RFC-FS-012 | Audio Preview Player | 🔲 Pending |
| RFC-FS-013 | Waveform Cache Format | 🔲 Pending |
| RFC-FS-014 | Plugin Database & Scan Cache | 🔲 Pending |
| RFC-FS-015 | Recent Projects & Templates | 🔲 Pending |
| RFC-FS-016 | Tag System & Management | 🔲 Pending |
| RFC-FS-017 | Drag & Drop Integration | 🔲 Pending |
| RFC-FS-018 | Project File Management | 🔲 Pending |

---

## Appendix A: Database File Locations

```
Database:         %APPDATA%/ARIA/aria.db          (Windows)
                  ~/Library/Application Support/ARIA/aria.db (macOS)
                  ~/.config/aria/aria.db          (Linux)

Waveform Cache:   %APPDATA%/ARIA/waveform_cache/  (Windows)
                  ~/Library/Caches/ARIA/waveforms/ (macOS)
                  ~/.cache/aria/waveforms/         (Linux)

Project Cache:    %APPDATA%/ARIA/project_cache/   (Windows)
                  ~/Library/Caches/ARIA/projects/  (macOS)
                  ~/.cache/aria/projects/          (Linux)

Config:           %APPDATA%/ARIA/config.json      (Windows)
                  ~/Library/Application Support/ARIA/config.json (macOS)
                  ~/.config/aria/config.json      (Linux)
```

## Appendix B: Database Size Estimates

| Content | Files | DB Size |
|---|---|---|
| Small library | 10,000 samples | ~5MB |
| Medium library | 100,000 samples | ~50MB |
| Large library | 1,000,000 samples | ~500MB |
| Plus plugin DB | 500 plugins | ~1MB |
| Plus project DB | 100 projects | ~500KB |
| **Total (large)** | | **~500MB** |
