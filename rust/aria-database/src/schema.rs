// ARIA DAW — Database Schema Definitions
// Full SQL schema matching 22_Database.md.

/// Current schema version.
pub const SCHEMA_VERSION: u32 = 1;

/// Complete V1 schema SQL — creates all tables, indexes, FTS5 virtual tables,
/// and content-sync triggers.
pub const CREATE_SCHEMA_SQL: &str = r#"
-- Schema version tracking (ensured by migration runner)
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT
);

-- ── SAMPLES ─────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS samples (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path       TEXT NOT NULL UNIQUE,
    file_hash       TEXT NOT NULL,
    file_size       INTEGER NOT NULL,
    file_format     TEXT NOT NULL,
    sample_rate     INTEGER,
    bit_depth       INTEGER,
    channels        INTEGER,
    duration_ms     INTEGER,
    bpm             REAL,
    bpm_confidence  REAL,
    musical_key     TEXT,
    key_confidence  REAL,
    loudness_lufs   REAL,
    peak_db         REAL,
    category        TEXT,
    subcategory     TEXT,
    mood            TEXT,
    name            TEXT,
    rating          INTEGER DEFAULT 0 CHECK(rating >= 0 AND rating <= 5),
    favorite        INTEGER DEFAULT 0,
    color           TEXT,
    notes           TEXT,
    file_created    TEXT,
    file_modified   TEXT,
    tags            TEXT,                    -- Denormalized tag names for FTS5 search (space-separated)
    first_indexed   TEXT DEFAULT (datetime('now')),
    last_indexed    TEXT DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_samples_category ON samples(category);
CREATE INDEX IF NOT EXISTS idx_samples_bpm ON samples(bpm);
CREATE INDEX IF NOT EXISTS idx_samples_musical_key ON samples(musical_key);
CREATE INDEX IF NOT EXISTS idx_samples_favorite ON samples(favorite);
CREATE INDEX IF NOT EXISTS idx_samples_rating ON samples(rating);
CREATE INDEX IF NOT EXISTS idx_samples_file_hash ON samples(file_hash);
CREATE INDEX IF NOT EXISTS idx_samples_file_modified ON samples(file_modified);

-- FTS5 full-text search for samples
CREATE VIRTUAL TABLE IF NOT EXISTS samples_fts USING fts5(
    name, category, subcategory, mood, notes, tags,
    content='samples',
    content_rowid='id',
    tokenize='unicode61'
);

-- Triggers to keep FTS in sync with samples table
CREATE TRIGGER IF NOT EXISTS samples_ai AFTER INSERT ON samples BEGIN
    INSERT INTO samples_fts(rowid, name, category, subcategory, mood, notes, tags)
    VALUES (new.id, new.name, new.category, new.subcategory, new.mood, new.notes, new.tags);
END;

CREATE TRIGGER IF NOT EXISTS samples_ad AFTER DELETE ON samples BEGIN
    INSERT INTO samples_fts(samples_fts, rowid, name, category, subcategory, mood, notes, tags)
    VALUES ('delete', old.id, old.name, old.category, old.subcategory, old.mood, old.notes, old.tags);
END;

CREATE TRIGGER IF NOT EXISTS samples_au AFTER UPDATE ON samples BEGIN
    INSERT INTO samples_fts(samples_fts, rowid, name, category, subcategory, mood, notes, tags)
    VALUES ('delete', old.id, old.name, old.category, old.subcategory, old.mood, old.notes, old.tags);
    INSERT INTO samples_fts(rowid, name, category, subcategory, mood, notes, tags)
    VALUES (new.id, new.name, new.category, new.subcategory, new.mood, new.notes, new.tags);
END;

-- ── PLUGINS ─────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS plugins (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id       TEXT NOT NULL UNIQUE,
    name            TEXT NOT NULL,
    vendor          TEXT,
    format          TEXT NOT NULL,
    version         TEXT,
    file_path       TEXT NOT NULL,
    file_hash       TEXT,
    file_size       INTEGER,
    category        TEXT,
    subcategory     TEXT,
    features        TEXT,
    num_inputs      INTEGER DEFAULT 2,
    num_outputs     INTEGER DEFAULT 2,
    has_editor      INTEGER DEFAULT 0,
    is_synth        INTEGER DEFAULT 0,
    is_instrument   INTEGER DEFAULT 0,
    last_scanned    TEXT,
    scan_error      TEXT,
    blacklisted     INTEGER DEFAULT 0,
    blacklist_reason TEXT,
    favorite        INTEGER DEFAULT 0,
    rating          INTEGER DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_plugins_category ON plugins(category);
CREATE INDEX IF NOT EXISTS idx_plugins_format ON plugins(format);
CREATE INDEX IF NOT EXISTS idx_plugins_vendor ON plugins(vendor);

CREATE VIRTUAL TABLE IF NOT EXISTS plugins_fts USING fts5(
    name, vendor, category, subcategory, features,
    content='plugins',
    content_rowid='id'
);

-- ── PROJECTS ─────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS projects (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    file_path       TEXT NOT NULL UNIQUE,
    file_hash       TEXT,
    file_size       INTEGER,
    author          TEXT,
    tempo           REAL,
    time_signature  TEXT,
    duration_ppqn   INTEGER,
    track_count     INTEGER,
    created_at      TEXT,
    last_opened     TEXT,
    last_modified   TEXT,
    thumbnail_path  TEXT,
    favorite        INTEGER DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_projects_last_opened ON projects(last_opened);
CREATE INDEX IF NOT EXISTS idx_projects_name ON projects(name);

CREATE VIRTUAL TABLE IF NOT EXISTS projects_fts USING fts5(
    name, author,
    content='projects',
    content_rowid='id'
);

-- ── PRESETS ─────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS presets (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    file_path       TEXT NOT NULL UNIQUE,
    file_hash       TEXT,
    file_size       INTEGER,
    plugin_id       TEXT,
    plugin_name     TEXT,
    category        TEXT,
    author          TEXT,
    tags            TEXT,
    rating          INTEGER DEFAULT 0,
    favorite        INTEGER DEFAULT 0,
    is_factory      INTEGER DEFAULT 0,
    created_at      TEXT,
    modified_at     TEXT
);

CREATE INDEX IF NOT EXISTS idx_presets_plugin ON presets(plugin_id);
CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category);

CREATE VIRTUAL TABLE IF NOT EXISTS presets_fts USING fts5(
    name, plugin_name, category, tags, author,
    content='presets',
    content_rowid='id'
);

-- ── TAGS ────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS tags (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,
    color           TEXT,
    icon            TEXT
);

CREATE TABLE IF NOT EXISTS sample_tags (
    sample_id       INTEGER REFERENCES samples(id) ON DELETE CASCADE,
    tag_id          INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (sample_id, tag_id)
);

CREATE TABLE IF NOT EXISTS plugin_tags (
    plugin_id       INTEGER REFERENCES plugins(id) ON DELETE CASCADE,
    tag_id          INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (plugin_id, tag_id)
);

-- ── WATCHED FOLDERS ─────────────────────────────────────────

CREATE TABLE IF NOT EXISTS watched_folders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    path            TEXT NOT NULL UNIQUE,
    recursive       INTEGER DEFAULT 1,
    auto_index      INTEGER DEFAULT 1,
    last_scanned    TEXT,
    file_count      INTEGER DEFAULT 0
);

-- ── SEARCH HISTORY ──────────────────────────────────────────

CREATE TABLE IF NOT EXISTS search_history (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    query           TEXT NOT NULL,
    category        TEXT,
    result_count    INTEGER,
    searched_at     TEXT DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_search_history_time ON search_history(searched_at);

-- ── SAMPLE ANALYSIS (extensible JSON) ───────────────────────

CREATE TABLE IF NOT EXISTS sample_analysis (
    sample_id       INTEGER PRIMARY KEY REFERENCES samples(id) ON DELETE CASCADE,
    analysis_data   TEXT NOT NULL
);
"#;
