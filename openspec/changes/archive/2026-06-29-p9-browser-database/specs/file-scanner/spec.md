# File Scanner Specification

## Purpose

Recursively scans watched folders, detects audio/plugin/project files, extracts metadata (duration, sample rate, BPM, key, loudness), classifies by category, and writes results to the database.

## Requirements

### Requirement: Recursive Scanning

The scanner MUST recursively traverse directories, identifying supported file types by extension.

| Supported Type | Extensions |
|---|---|
| Audio | `.wav`, `.aiff`, `.aif`, `.flac`, `.mp3`, `.ogg`, `.m4a`, `.wma`, `.aac`, `.wv`, `.opus` |
| Plugin | `.clap`, `.vst3`, `.vst`, `.component`, `.lv2` |
| Project | `.aria`, `.aria.auto`, `.aria.bak`, `.aria.template` |
| Preset | `.clap-preset`, `.fxp`, `.fxb`, `.vstpreset`, `.aupreset` |

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Full scan | Watched folder with 100 files (80 audio, 10 plugins, 10 unsupported) | Scanner runs recursively | 90 entries inserted; unsupported skipped |
| 2 | Empty folder | Watched folder with no files | Scanner runs | Zero entries; indexing complete |
| 3 | Symlink loop | Folder contains a symlink pointing to parent | Scanner traverses | Symlink detected and skipped; no infinite loop |

### Requirement: Metadata Extraction

For audio files, the scanner SHALL extract duration, sample rate, bit depth, channels, BPM, musical key, loudness (LUFS), and peak (dBFS).

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Full metadata | Valid 44.1kHz 24-bit stereo WAV file | Scanner analyzes file | Duration, sample_rate=44100, bit_depth=24, channels=2, BPM, key, loudness populated |
| 2 | Unreadable file | Corrupted or truncated audio file | Scanner attempts analysis | File inserted with `bpm=0`, `key=null`; error logged |
| 3 | BPM confidence | File yields BPM=120.0 with confidence 0.85 | Scanner completes analysis | `bpm=120.0`, `bpm_confidence=0.85` stored |

### Requirement: File Hashing

Each scanned file MUST be identified by SHA-256 hash to detect moves, duplicates, and modifications.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Existing file unchanged | File scanned previously with hash X | Same file with same hash rescanned | Skipped (hash matches, modified time unchanged) |
| 2 | File modified | File content changed (new hash) | Scanner encounters it | Existing row updated; FTS index refreshed |

### Requirement: Category Classification

The scanner SHOULD assign a category (kick, snare, pad, bass, fx, loop, etc.) based on spectral/temporal analysis.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Drum detection | Short transient-rich audio file | Scanner analyzes | Category assigned to `kick` or `snare` |
| 2 | Unknown content | Ambiguous audio with no clear profile | Scanner analyzes | Category defaults to `Other` |

### Requirement: Progress Reporting

The scanner MUST expose status including files indexed, skipped, failed, and estimated remaining time.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | In-progress | Scanner processing 1000 files | Status polled mid-scan | `files_indexed`, `total_discovered`, `progress` (0.0–1.0) returned |
| 2 | Completion | All files processed | Scanning finishes | `on_indexing_complete` event fires |
