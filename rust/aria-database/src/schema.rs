// ARIA DAW — Database Schema Definitions

pub const SCHEMA_VERSION: u32 = 1;

/// Full SQL schema for the ARIA database.
pub const CREATE_SCHEMA_SQL: &str = r#"
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA foreign_keys=ON;

CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT
);

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
    musical_key     TEXT,
    loudness_lufs   REAL,
    peak_db         REAL,
    category        TEXT,
    name            TEXT,
    rating          INTEGER DEFAULT 0,
    favorite        INTEGER DEFAULT 0,
    first_indexed   TEXT DEFAULT (datetime('now')),
    last_indexed    TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS plugins (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id       TEXT NOT NULL UNIQUE,
    name            TEXT NOT NULL,
    vendor          TEXT,
    format          TEXT NOT NULL,
    version         TEXT,
    file_path       TEXT NOT NULL,
    category        TEXT,
    has_editor      INTEGER DEFAULT 0,
    is_synth        INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS projects (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    file_path       TEXT NOT NULL UNIQUE,
    author          TEXT,
    tempo           REAL,
    last_opened     TEXT,
    favorite        INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS tags (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,
    color           TEXT
);

CREATE TABLE IF NOT EXISTS sample_tags (
    sample_id       INTEGER REFERENCES samples(id) ON DELETE CASCADE,
    tag_id          INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (sample_id, tag_id)
);

CREATE TABLE IF NOT EXISTS watched_folders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    path            TEXT NOT NULL UNIQUE,
    recursive       INTEGER DEFAULT 1,
    last_scanned    TEXT
);
"#;
