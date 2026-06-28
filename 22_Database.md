# ARIA DAW — Database & Indexing Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Engine**: SQLite 3.45
> **Language**: Rust

---

## Table of Contents

1. [Overview](#1-overview)
2. [Schema Overview](#2-schema-overview)
3. [Samples Table](#3-samples-table)
4. [Plugins Table](#4-plugins-table)
5. [Projects Table](#5-projects-table)
6. [Presets Table](#6-presets-table)
7. [Tags & Metadata](#7-tags--metadata)
8. [Full-Text Search](#8-full-text-search)
9. [Migrations](#9-migrations)
10. [Performance](#10-performance)
11. [API Reference](#11-api-reference)
12. [RFC Index](#12-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Database subsystem manages all persistent metadata for ARIA DAW. It provides fast indexing, search, and retrieval of samples, plugins, projects, and presets using SQLite with FTS5 full-text search.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Search latency | < 50ms for 100k samples |
| Indexing speed | 1000 files/second |
| Database size | < 500MB for 1M samples |
| Migration | Automatic, zero-data-loss |
| Concurrency | WAL mode, multiple readers |

### 1.3. Database Location

```
Windows:   %APPDATA%/ARIA/aria.db
macOS:     ~/Library/Application Support/ARIA/aria.db
Linux:     ~/.config/aria/aria.db

Backup:    %APPDATA%/ARIA/aria.db.backup (auto on migration)
Cache:     %APPDATA%/ARIA/cache/ (waveforms, analysis)
```

---

## 2. Schema Overview

### 2.1. Entity Relationship

```
samples ──── sample_tags ──── tags
  │                              │
  │                              │
plugins ──── plugin_tags ───────┘
  │
  │
projects ─── project_files
  │
  │
presets ───── preset_plugins
              │
              └──── plugins

watched_folders (standalone)
search_history (standalone)
```

### 2.2. Complete Schema

```sql
-- Enable WAL mode for concurrent access
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA cache_size=-65536;  -- 64MB cache

-- Schema version tracking
CREATE TABLE schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT
);

-- ──────────────────────────────────────────────
-- SAMPLES
-- ──────────────────────────────────────────────

CREATE TABLE samples (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path       TEXT NOT NULL UNIQUE,
    file_hash       TEXT NOT NULL,            -- SHA-256 hex
    file_size       INTEGER NOT NULL,
    file_format     TEXT NOT NULL,            -- wav, aiff, flac, mp3, ogg, m4a
    
    -- Audio properties
    sample_rate     INTEGER,
    bit_depth       INTEGER,
    channels        INTEGER,
    duration_ms     INTEGER,
    
    -- Analysis
    bpm             REAL,                    -- 0 = unknown
    bpm_confidence  REAL,
    musical_key     TEXT,                    -- "Cm", "F#", etc.
    key_confidence  REAL,
    loudness_lufs   REAL,                    -- Integrated LUFS
    peak_db         REAL,                    -- Peak level
    
    -- Classification
    category        TEXT,                    -- kick, snare, pad, fx, etc.
    subcategory     TEXT,
    mood            TEXT,                    -- dark, bright, warm, etc.
    
    -- User metadata
    name            TEXT,                    -- User-friendly display name
    rating          INTEGER DEFAULT 0 CHECK(rating >= 0 AND rating <= 5),
    favorite        INTEGER DEFAULT 0,
    color           TEXT,                    -- Hex color
    notes           TEXT,                    -- User notes
    
    -- Timestamps
    file_created    TEXT,
    file_modified   TEXT,
    first_indexed   TEXT DEFAULT (datetime('now')),
    last_indexed    TEXT DEFAULT (datetime('now'))
);

CREATE INDEX idx_samples_category ON samples(category);
CREATE INDEX idx_samples_bpm ON samples(bpm);
CREATE INDEX idx_samples_key ON samples(musical_key);
CREATE INDEX idx_samples_favorite ON samples(favorite);
CREATE INDEX idx_samples_rating ON samples(rating);
CREATE INDEX idx_samples_file_hash ON samples(file_hash);
CREATE INDEX idx_samples_file_modified ON samples(file_modified);

-- FTS5 full-text search
CREATE VIRTUAL TABLE samples_fts USING fts5(
    name, category, subcategory, mood, notes, tags,
    content='samples',
    content_rowid='id',
    tokenize='unicode61'
);

-- Triggers to keep FTS in sync
CREATE TRIGGER samples_ai AFTER INSERT ON samples BEGIN
    INSERT INTO samples_fts(rowid, name, category, subcategory, mood, notes)
    VALUES (new.id, new.name, new.category, new.subcategory, new.mood, new.notes);
END;

CREATE TRIGGER samples_ad AFTER DELETE ON samples BEGIN
    INSERT INTO samples_fts(samples_fts, rowid, name, category, subcategory, mood, notes)
    VALUES ('delete', old.id, old.name, old.category, old.subcategory, old.mood, old.notes);
END;

CREATE TRIGGER samples_au AFTER UPDATE ON samples BEGIN
    INSERT INTO samples_fts(samples_fts, rowid, name, category, subcategory, mood, notes)
    VALUES ('delete', old.id, old.name, old.category, old.subcategory, old.mood, old.notes);
    INSERT INTO samples_fts(rowid, name, category, subcategory, mood, notes)
    VALUES (new.id, new.name, new.category, new.subcategory, new.mood, new.notes);
END;

-- ──────────────────────────────────────────────
-- PLUGINS
-- ──────────────────────────────────────────────

CREATE TABLE plugins (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id       TEXT NOT NULL UNIQUE,    -- "vendor.name"
    name            TEXT NOT NULL,
    vendor          TEXT,
    format          TEXT NOT NULL,           -- "CLAP", "VST3", "AU", "LV2"
    version         TEXT,
    file_path       TEXT NOT NULL,
    file_hash       TEXT,
    file_size       INTEGER,
    category        TEXT,                    -- synth, effect, analyzer, utility
    subcategory     TEXT,                    -- reverb, delay, compressor, etc.
    features        TEXT,                    -- JSON array
    num_inputs      INTEGER DEFAULT 2,
    num_outputs     INTEGER DEFAULT 2,
    has_editor      INTEGER DEFAULT 0,
    is_synth        INTEGER DEFAULT 0,
    is_instrument   INTEGER DEFAULT 0,
    
    -- Status
    last_scanned    TEXT,
    scan_error      TEXT,                    -- NULL if OK
    blacklisted     INTEGER DEFAULT 0,
    blacklist_reason TEXT,
    
    favorite        INTEGER DEFAULT 0,
    rating          INTEGER DEFAULT 0
);

CREATE INDEX idx_plugins_category ON plugins(category);
CREATE INDEX idx_plugins_format ON plugins(format);
CREATE INDEX idx_plugins_vendor ON plugins(vendor);

CREATE VIRTUAL TABLE plugins_fts USING fts5(
    name, vendor, category, subcategory, features,
    content='plugins',
    content_rowid='id'
);

-- ──────────────────────────────────────────────
-- PROJECTS
-- ──────────────────────────────────────────────

CREATE TABLE projects (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    file_path       TEXT NOT NULL UNIQUE,
    file_hash       TEXT,
    file_size       INTEGER,
    
    -- Project metadata
    author          TEXT,
    tempo           REAL,
    time_signature  TEXT,                    -- "4/4"
    duration_ppqn   INTEGER,
    track_count     INTEGER,
    
    -- Timestamps
    created_at      TEXT,
    last_opened     TEXT,
    last_modified   TEXT,
    
    -- Thumbnail
    thumbnail_path  TEXT,
    
    favorite        INTEGER DEFAULT 0
);

CREATE INDEX idx_projects_last_opened ON projects(last_opened);
CREATE INDEX idx_projects_name ON projects(name);

CREATE VIRTUAL TABLE projects_fts USING fts5(
    name, author,
    content='projects',
    content_rowid='id'
);

-- ──────────────────────────────────────────────
-- PRESETS
-- ──────────────────────────────────────────────

CREATE TABLE presets (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    file_path       TEXT NOT NULL UNIQUE,
    file_hash       TEXT,
    file_size       INTEGER,
    plugin_id       TEXT,                    -- Associated plugin
    plugin_name     TEXT,
    category        TEXT,                    -- bass, lead, pad, fx
    author          TEXT,
    tags            TEXT,
    rating          INTEGER DEFAULT 0,
    favorite        INTEGER DEFAULT 0,
    is_factory      INTEGER DEFAULT 0,      -- Shipped with ARIA
    created_at      TEXT,
    modified_at     TEXT
);

CREATE INDEX idx_presets_plugin ON presets(plugin_id);
CREATE INDEX idx_presets_category ON presets(category);

CREATE VIRTUAL TABLE presets_fts USING fts5(
    name, plugin_name, category, tags, author,
    content='presets',
    content_rowid='id'
);

-- ──────────────────────────────────────────────
-- TAGS
-- ──────────────────────────────────────────────

CREATE TABLE tags (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,
    color           TEXT,                    -- Hex color
    icon            TEXT                     -- Material icon name
);

CREATE TABLE sample_tags (
    sample_id       INTEGER REFERENCES samples(id) ON DELETE CASCADE,
    tag_id          INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (sample_id, tag_id)
);

CREATE TABLE plugin_tags (
    plugin_id       INTEGER REFERENCES plugins(id) ON DELETE CASCADE,
    tag_id          INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (plugin_id, tag_id)
);

-- ──────────────────────────────────────────────
-- WATCHED FOLDERS
-- ──────────────────────────────────────────────

CREATE TABLE watched_folders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    path            TEXT NOT NULL UNIQUE,
    recursive       INTEGER DEFAULT 1,
    auto_index      INTEGER DEFAULT 1,
    last_scanned    TEXT,
    file_count      INTEGER DEFAULT 0
);

-- ──────────────────────────────────────────────
-- SEARCH HISTORY
-- ──────────────────────────────────────────────

CREATE TABLE search_history (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    query           TEXT NOT NULL,
    category        TEXT,                    -- samples, plugins, projects, presets
    result_count    INTEGER,
    searched_at     TEXT DEFAULT (datetime('now'))
);

CREATE INDEX idx_search_history_time ON search_history(searched_at);
```

---

## 3. Samples Table

### 3.1. Sample Categories

```sql
-- Auto-detected categories (via spectral/temporal analysis):
-- Drums:     kick, snare, hihat, tom, percussion, clap, cymbal, rim
-- Melodic:   bass, pad, lead, pluck, chord, arp, bell, organ
-- FX:        riser, sweep, impact, glitch, noise, texture, ambiance
-- Voice:     vocal, choir, spoken, fx_vocal
-- Loops:     drum_loop, bass_loop, chord_loop, full_mix
-- Other:     synth, guitar, piano, strings, brass, woodwind, ethnic
```

### 3.2. Sample Analysis Data

```sql
-- Per-sample analysis stored as JSON in a separate table
-- for extensibility:

CREATE TABLE sample_analysis (
    sample_id       INTEGER PRIMARY KEY REFERENCES samples(id) ON DELETE CASCADE,
    analysis_data   TEXT NOT NULL,           -- JSON
    
    -- Example JSON structure:
    -- {
    --   "spectral": {
    --     "centroid": 2400.5,
    --     "rolloff": 12000,
    --     "flux": 0.45,
    --     "mfcc": [2.3, -1.5, 0.8, ...]
    --   },
    --   "temporal": {
    --     "attack_ms": 5.2,
    --     "decay_ms": 120.0,
    --     "transient_count": 3
    --   },
    --   "tonal": {
    --     "pitch_salience": 0.85,
    --     "chroma": [0.9, 0.1, 0.3, ...]
    --   }
    -- }
);
```

---

## 4. Plugins Table

### 4.1. Plugin Features

```sql
-- The `features` column stores a JSON array of standardized tags:

-- Effects: equalizer, compressor, limiter, gate, reverb, delay,
--          chorus, flanger, phaser, distortion, saturation,
--          filter, modulator, pitch, stereo, meter, analyzer
--
-- Synths:  subtractive, fm, wavetable, granular, additive,
--          physical, sampler, drum, rompler
--
-- Utility: gain, pan, crossover, tuner, metronome, tool
```

### 4.2. Plugin Blacklist

```sql
CREATE TABLE plugin_blacklist (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id       TEXT NOT NULL,
    format          TEXT NOT NULL,
    file_path       TEXT,
    crash_count     INTEGER DEFAULT 0,
    last_crash      TEXT,
    reason          TEXT,
    auto_blacklisted INTEGER DEFAULT 0,
    manually_added  INTEGER DEFAULT 0,
    UNIQUE(plugin_id, format)
);
```

---

## 5. Projects Table

### 5.1. Recent Projects

```sql
-- Recent projects are tracked separately for fast access:
CREATE VIEW recent_projects AS
SELECT * FROM projects
WHERE last_opened IS NOT NULL
ORDER BY last_opened DESC
LIMIT 50;
```

---

## 6. Presets Table

### 6.1. Preset Categories

```sql
-- Standard preset categories:
-- Synth:    bass, lead, pad, pluck, bell, organ, brass, string,
--           synth_bass, synth_lead, synth_pad, fx_synth
-- Effect:   reverb, delay, distortion, modulation, dynamics,
--           filter, spatial, utility
-- Drums:    kick, snare, hat, percussion, drum_kit
-- Voice:    vocal_processor, vocal_effect
-- Mastering: eq, compressor, limiter, exciter, imager
```

---

## 7. Tags & Metadata

### 7.1. Tag Management

```sql
-- Tags are user-defined and apply to samples, plugins, and presets.
-- Common built-in tags:
--   mood: dark, bright, warm, cold, aggressive, soft, ambient
--   color: red, blue, green, orange, purple
--   usage: intro, verse, chorus, bridge, fill, transition
--   quality: favorite, usable, needs_work, archive
```

---

## 8. Full-Text Search

### 8.1. FTS5 Queries

```sql
-- Sample searches:

-- Simple text search
SELECT s.* FROM samples s
JOIN samples_fts fts ON s.id = fts.rowid
WHERE samples_fts MATCH 'dark kick'
ORDER BY rank
LIMIT 20;

-- Filtered search with FTS
SELECT s.* FROM samples s
JOIN samples_fts fts ON s.id = fts.rowid
WHERE samples_fts MATCH 'pad'
  AND s.category = 'pad'
  AND s.bpm BETWEEN 80 AND 120
  AND s.favorite = 1
ORDER BY rank
LIMIT 20;

-- Tag-based search
SELECT s.* FROM samples s
JOIN sample_tags st ON s.id = st.sample_id
JOIN tags t ON st.tag_id = t.id
WHERE t.name IN ('dark', 'sub-bass')
GROUP BY s.id
HAVING COUNT(DISTINCT t.name) = 2;

-- Combined search
SELECT s.* FROM samples s
LEFT JOIN sample_tags st ON s.id = st.sample_id
LEFT JOIN tags t ON st.tag_id = t.id
WHERE (
  s.name LIKE '%kick%'
  OR s.category = 'kick'
  OR t.name = 'kick'
)
AND s.bpm BETWEEN 100 AND 140
ORDER BY s.rating DESC, s.bpm
LIMIT 30;
```

### 8.2. Search Ranking

```sql
-- FTS5 ranks results by relevance using BM25 algorithm.
-- Additional ranking factors:
--   1. FTS5 relevance (BM25)
--   2. User rating (0-5)
--   3. Favorite status
--   4. Last used date
--   5. Category match precision

-- Custom ranking function:
SELECT s.*, 
       bm25(samples_fts, 0, 1, 2) * 1.0 +
       s.rating * 0.1 +
       s.favorite * 0.5 AS score
FROM samples s
JOIN samples_fts fts ON s.id = fts.rowid
WHERE samples_fts MATCH 'kick'
ORDER BY score DESC
LIMIT 20;
```

---

## 9. Migrations

### 9.1. Migration System

```rust
struct Migration {
    version: u32,
    description: &'static str,
    up: &'static str,      // SQL to apply
    down: Option<&'static str>,  // SQL to revert
}

// Migrations are applied in order on database open.
// A lock file prevents concurrent migrations.
// Backups are created before destructive migrations.

static MIGRATIONS: &[Migration] = &[
    Migration {
        version: 1,
        description: "Initial schema",
        up: include_str!("migrations/v1_initial.sql"),
        down: None,
    },
    Migration {
        version: 2,
        description: "Add waveform cache table",
        up: include_str!("migrations/v2_waveform_cache.sql"),
        down: Some("DROP TABLE waveform_cache"),
    },
    Migration {
        version: 3,
        description: "Add FTS5 search indexes",
        up: include_str!("migrations/v3_fts5.sql"),
        down: Some("DROP TABLE IF EXISTS samples_fts; ..."),
    },
];
```

### 9.2. Migration Strategy

```rust
fn run_migrations(db: &Connection) -> Result<()> {
    let current: u32 = db.query_row(
        "SELECT COALESCE(MAX(version), 0) FROM schema_version",
        [], |row| row.get(0)
    )?;
    
    for migration in MIGRATIONS {
        if migration.version > current {
            println!("Applying migration v{}: {}", 
                     migration.version, migration.description);
            
            // Backup before destructive migration
            if migration.down.is_some() {
                create_backup(db)?;
            }
            
            db.execute_batch(migration.up)?;
            db.execute(
                "INSERT INTO schema_version (version, description) VALUES (?1, ?2)",
                params![migration.version, migration.description]
            )?;
        }
    }
    
    Ok(())
}
```

---

## 10. Performance

### 10.1. Query Performance

| Query | 10k samples | 100k samples | 1M samples |
|---|---|---|---|
| Exact match by path | < 1ms | < 1ms | < 2ms |
| FTS5 text search | < 5ms | < 15ms | < 50ms |
| Category browse | < 2ms | < 5ms | < 20ms |
| Tag filter (2 tags) | < 5ms | < 10ms | < 30ms |
| Combined search | < 10ms | < 25ms | < 80ms |

### 10.2. Database Size

| Content | Entries | DB Size |
|---|---|---|
| Samples (minimal) | 10,000 | 3 MB |
| Samples (full analysis) | 10,000 | 15 MB |
| Samples (full analysis) | 100,000 | 150 MB |
| Samples (full analysis) | 1,000,000 | 1.5 GB |
| Plugins | 500 | 500 KB |
| Projects | 100 | 200 KB |
| Presets | 5,000 | 2 MB |
| FTS indexes | — | ~50% of data size |

### 10.3. Optimization Strategies

```sql
-- 1. WAL mode for concurrent reads during indexing
PRAGMA journal_mode=WAL;

-- 2. Prepared statements for frequent queries
--    (all queries use prepared statements, not raw SQL strings)

-- 3. Batch inserts during indexing
BEGIN TRANSACTION;
INSERT INTO samples VALUES (...);  -- 1000 inserts
INSERT INTO samples VALUES (...);
COMMIT;

-- 4. Incremental FTS index rebuild
--    FTS is kept in sync via triggers (not rebuilt from scratch)

-- 5. Periodic VACUUM
VACUUM;  -- After large deletes

-- 6. ANALYZE for query planner
ANALYZE;

-- 7. Connection pooling
--    Single writer, multiple readers via WAL mode
```

---

## 11. API Reference

### 11.1. Rust API

```rust
pub struct Database {
    conn: Connection,
}

impl Database {
    pub fn open(path: &str) -> Result<Self>;
    pub fn open_in_memory() -> Result<Self>;
    
    // Samples
    pub fn insert_sample(&self, sample: SampleEntry) -> Result<i64>;
    pub fn update_sample(&self, id: i64, sample: SampleEntry) -> Result<()>;
    pub fn delete_sample(&self, id: i64) -> Result<()>;
    pub fn get_sample(&self, id: i64) -> Result<SampleEntry>;
    pub fn search_samples(&self, query: &SearchQuery) -> Result<SearchResults>;
    pub fn get_samples_by_category(&self, category: &str) -> Result<Vec<SampleEntry>>;
    
    // Tags
    pub fn add_tag(&self, name: &str, color: Option<&str>) -> Result<i64>;
    pub fn tag_sample(&self, sample_id: i64, tag_id: i64) -> Result<()>;
    pub fn untag_sample(&self, sample_id: i64, tag_id: i64) -> Result<()>;
    pub fn get_sample_tags(&self, sample_id: i64) -> Result<Vec<Tag>>;
    
    // Watched folders
    pub fn add_watched_folder(&self, path: &str) -> Result<()>;
    pub fn remove_watched_folder(&self, path: &str) -> Result<()>;
    pub fn get_watched_folders(&self) -> Result<Vec<String>>;
    
    // Plugins
    pub fn insert_plugin(&self, plugin: PluginEntry) -> Result<i64>;
    pub fn search_plugins(&self, query: &SearchQuery) -> Result<SearchResults>;
    
    // Projects
    pub fn record_project_open(&self, path: &str) -> Result<()>;
    pub fn get_recent_projects(&self, limit: u32) -> Result<Vec<ProjectEntry>>;
    
    // Maintenance
    pub fn remove_missing_files(&self) -> Result<u32>;
    pub fn vacuum(&self) -> Result<()>;
    pub fn backup(&self, path: &str) -> Result<()>;
    pub fn integrity_check(&self) -> Result<Vec<String>>;
}
```

---

## 12. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-DB-001 | SQLite Schema Design | 🔲 Pending |
| RFC-DB-002 | Samples Table & Analysis | 🔲 Pending |
| RFC-DB-003 | Plugins Table & Features | 🔲 Pending |
| RFC-DB-004 | Projects Table & Recent | 🔲 Pending |
| RFC-DB-005 | Presets Table & Categories | 🔲 Pending |
| RFC-DB-006 | Tags System | 🔲 Pending |
| RFC-DB-007 | FTS5 Full-Text Search | 🔲 Pending |
| RFC-DB-008 | Search Ranking (BM25 + custom) | 🔲 Pending |
| RFC-DB-009 | Migration System | 🔲 Pending |
| RFC-DB-010 | WAL Mode & Concurrency | 🔲 Pending |
| RFC-DB-011 | Batch Insert Performance | 🔲 Pending |
| RFC-DB-012 | Database Backup & Restore | 🔲 Pending |
| RFC-DB-013 | Missing File Cleanup | 🔲 Pending |
| RFC-DB-014 | Database Integrity Check | 🔲 Pending |
| RFC-DB-015 | Rust → C FFI API | 🔲 Pending |

---

## Appendix A: Database File Management

```
Database lifecycle:
  - Created on first launch
  - Migrated automatically on version change
  - Backed up before destructive migrations
  - VACUUM'd monthly (or when > 30% deleted)
  - Integrity checked on startup (background)

File locations:
  Main DB:    [config_dir]/aria.db
  Cache:      [config_dir]/cache/
  Backups:    [config_dir]/backups/
  Temp:       [temp_dir]/aria/

Backup rotation:
  - Keep last 3 automatic backups
  - Manual backups preserved indefinitely
  - Backup format: aria.db.v{major}.{minor}.{timestamp}.bak
```

## Appendix B: Query Examples

```sql
-- Find all drum samples between 110-130 BPM, rated 4+:
SELECT name, file_path, bpm, rating
FROM samples
WHERE category IN ('kick', 'snare', 'hihat')
  AND bpm BETWEEN 110 AND 130
  AND rating >= 4
ORDER BY rating DESC, bpm;

-- Find samples by mood tag:
SELECT s.name, t.name as tag
FROM samples s
JOIN sample_tags st ON s.id = st.sample_id
JOIN tags t ON st.tag_id = t.id
WHERE t.name = 'dark'
ORDER BY s.rating DESC;

-- Recently added plugins:
SELECT name, vendor, format
FROM plugins
ORDER BY last_scanned DESC
LIMIT 10;

-- Database stats:
SELECT 'samples' as type, COUNT(*) as count FROM samples
UNION ALL
SELECT 'plugins', COUNT(*) FROM plugins
UNION ALL
SELECT 'projects', COUNT(*) FROM projects
UNION ALL
SELECT 'presets', COUNT(*) FROM presets
UNION ALL
SELECT 'tags', COUNT(*) FROM tags;
```
