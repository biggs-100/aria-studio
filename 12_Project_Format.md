# ARIA DAW — Project Format Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Format**: Custom binary (.aria)

---

## Table of Contents

1. [Overview](#1-overview)
2. [File Structure](#2-file-structure)
3. [Block Types](#3-block-types)
4. [Version Compatibility](#4-version-compatibility)
5. [Serialization](#5-serialization)
6. [Non-Destructive Editing](#6-non-destructive-editing)
7. [Auto-Save & Recovery](#7-auto-save--recovery)
8. [Backwards Compatibility](#8-backwards-compatibility)
9. [API Reference](#9-api-reference)
10. [RFC Index](#10-rfc-index)

---

## 1. Overview

### 1.1. Design Goals

| Goal | Target |
|---|---|
| Load time | < 3s for 100-track project |
| File size | < 1MB for typical project (excluding audio) |
| Crash recovery | Automatic, no data loss |
| Backwards compat | Projects from v1 open in v10 |
| Streaming | Memory-mappable for fast partial loads |
| Integrity | CRC32 checksums on all blocks |

### 1.2. File Extension

- **`.aria`** — Main project file
- **`.aria.auto`** — Auto-save file
- **`.aria.bak`** — Backup (before overwrite)
- **`.aria.template`** — Project template

---

## 2. File Structure

### 2.1. Binary Layout

```
┌──────────────────────────────────────────────────────────────────┐
│  ┌────────────────────────────────────────────────────────┐     │
│  │                        Header                          │     │
│  │  Magic: [0x41, 0x52, 0x49, 0x41] = "ARIA"             │     │
│  │  Endian: 0x00000001 (little-endian test)               │     │
│  │  Format Version: uint32 (major.minor.patch)             │     │
│  │  App Version: uint32 (major.minor.patch)                │     │
│  │  Created: Unix timestamp (uint64)                       │     │
│  │  Modified: Unix timestamp (uint64)                      │     │
│  │  Block Count: uint32                                    │     │
│  │  Block Table Offset: uint64                             │     │
│  │  Header CRC32: uint32                                   │     │
│  └────────────────────────────────────────────────────────┘     │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐     │
│  │                 Block Table                             │     │
│  │  [0]: type, offset, size, decompressed_size, CRC32     │     │
│  │  [1]: type, offset, size, decompressed_size, CRC32     │     │
│  │  ...                                                    │     │
│  │  ...                                                    │     │
│  └────────────────────────────────────────────────────────┘     │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  Block 0: Project Metadata                             │     │
│  │  ┌────────────────────────────────────────────────┐    │     │
│  │  │  FlatBuffers / Protocol Buffers serialized data │    │     │
│  │  │  (compressed with ZSTD if > threshold)         │    │     │
│  │  └────────────────────────────────────────────────┘    │     │
│  └────────────────────────────────────────────────────────┘     │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  Block 1: Track List                                   │     │
│  │  ┌────────────────────────────────────────────────┐    │     │
│  │  │  Track 0: type, name, color, state, clips...   │    │     │
│  │  │  Track 1: ...                                  │    │     │
│  │  └────────────────────────────────────────────────┘    │     │
│  └────────────────────────────────────────────────────────┘     │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  Block 2: Arrangement                                  │     │
│  │  ┌────────────────────────────────────────────────┐    │     │
│  │  │  Clip placements, markers, tempo map, time sig │    │     │
│  │  └────────────────────────────────────────────────┘    │     │
│  └────────────────────────────────────────────────────────┘     │
│                                                                  │
│  [Additional blocks...]                                         │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐     │
│  │                   Footer                                │     │
│  │  File CRC32 (uint32) — entire file except header + crc │     │
│  │  End marker: [0x41, 0x52, 0x49, 0x41, 0x45, 0x4F,    │     │
│  │               0x46, 0xAE] = "ARIAEOF\xAE"              │     │
│  └────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2. Header Structure

```cpp
#pragma pack(push, 1)
struct ProjectHeader {
    // Magic bytes: "ARIA"
    uint8_t magic[4] = {0x41, 0x52, 0x49, 0x41};
    
    // Endianness detection
    uint32_t endian_check = 0x00000001;
    
    // Format version (major.minor.patch)
    uint32_t format_version_major = 1;
    uint32_t format_version_minor = 0;
    uint32_t format_version_patch = 0;
    
    // Application version that created this file
    uint32_t app_version_major = 0;
    uint32_t app_version_minor = 1;
    uint32_t app_version_patch = 0;
    
    // Timestamps
    uint64_t created_timestamp;      // Unix timestamp (seconds)
    uint64_t modified_timestamp;     // Unix timestamp (seconds)
    
    // Block table
    uint32_t block_count = 0;
    uint64_t block_table_offset;     // File offset to block table
    
    // Header integrity
    uint32_t header_crc32;           // CRC32 of everything above this field
};
#pragma pack(pop)
```

### 2.3. Block Table Entry

```cpp
#pragma pack(push, 1)
struct BlockTableEntry {
    BlockType type;              // Block type ID
    uint64_t file_offset;        // Offset in file
    uint64_t compressed_size;    // Size on disk (0 if uncompressed)
    uint64_t decompressed_size;  // Size after decompression
    uint32_t data_crc32;         // CRC32 of decompressed data
    uint32_t flags;              // Compression flags, encryption, etc.
    
    // Block type identifier
    static constexpr uint32_t ENTRY_SIZE = 40;
};

enum class BlockType : uint32_t {
    Invalid         = 0,
    ProjectMeta     = 1,   // Project metadata
    TrackList       = 2,   // Track definitions
    Arrangement     = 3,   // Clip arrangement timeline
    SessionData     = 4,   // Session view data (clips, scenes)
    MixerState      = 5,   // Mixer state (faders, routing)
    AutomationData  = 6,   // Automation clip data
    PluginState     = 7,   // Plugin state data
    TempoMap        = 8,   // Tempo and time signature events
    MarkerList      = 9,   // Project markers
    UndoHistory     = 10,  // Undo history entries
    UserData        = 11,  // Custom user data (Lua scripts, etc.)
    PluginCache     = 12,  // Cached plugin information
    WindowState     = 13,  // Window layout state
    PreviewAudio    = 14,  // Embedded audio preview
    Custom          = 255   // Third-party extension data
};
#pragma pack(pop)
```

---

## 3. Block Types

### 3.1. Project Metadata Block

```cpp
// Serialized via FlatBuffers (efficient binary serialization)
// BlockType: ProjectMeta

table ProjectMetadata {
    name: string;                   // Project name
    author: string;                 // Author name
    description: string;            // Project description
    
    // Audio configuration
    sample_rate: uint = 48000;
    bit_depth: uint = 32;           // 16, 24, 32
    time_signature_num: uint = 4;
    time_signature_den: uint = 4;
    tempo: double = 120.0;
    
    // Colors
    track_color_palette: [Color];
    
    // Tags
    tags: [string];
    
    // Custom metadata
    custom_fields: [KeyValue];
}
```

### 3.2. Track List Block

```cpp
table TrackList {
    tracks: [TrackDefinition];
    master_track: TrackDefinition;
    return_tracks: [TrackDefinition];
}

table TrackDefinition {
    id: uint64;
    name: string;
    type: TrackType;        // Audio, MIDI, Group, VCA, Return, Master
    color: Color;
    muted: bool = false;
    soloed: bool = false;
    armed: bool = false;
    
    volume: double = 0.0;      // dB
    pan: double = 0.0;          // -1 to 1
    phase_invert: bool = false;
    width: double = 1.0;
    
    input: AudioInput;
    output: AudioOutput;
    sends: [SendDefinition];
    
    fx_chain: [PluginSlot];
    
    automation_lanes: [AutomationLaneRef];
    
    parent_group: uint64 = 0;   // 0 = no group
    vca_track: uint64 = 0;      // 0 = no VCA
    
    height: double = 80.0;
    minimized: bool = false;
}
```

### 3.3. Arrangement Block

```cpp
table ArrangementData {
    length: uint64;              // Total project length in PPQN
    loop_start: uint64;
    loop_end: uint64;
    loop_enabled: bool = false;
    
    clips: [ClipPlacement];      // All clips on all tracks
    markers: [Marker];
    
    time_signature_events: [TimeSignatureEvent];
    tempo_events: [TempoEvent];
}

table ClipPlacement {
    track_id: uint64;
    clip_id: uint64;
    start_ppqn: uint64;
    clip_type: ClipType;       // Audio, MIDI, Automation
    
    // Clip data (inline or reference)
    audio_clip: AudioClipData;      // If audio
    midi_clip: MidiClipData;        // If MIDI
    automation_ref: AutomationRef;  // If automation
    
    // Common properties
    muted: bool = false;
    gain: double = 0.0;
    fade_in: uint64;
    fade_out: uint64;
    fade_in_shape: FadeShape;
    fade_out_shape: FadeShape;
    
    // Time stretch
    time_stretched: bool = false;
    stretch_ratio: double = 1.0;
    pitch_shift_cents: int = 0;
    reversed: bool = false;
}

table AudioClipData {
    file_path: string;             // Relative to project
    file_hash: string;             // SHA-256 for change detection
    
    sample_offset: uint64;         // Start position in source file
    sample_length: uint64;         // How many samples to use (0 = all)
    
    warp_markers: [WarpMarker];
    transients: [uint64];
    
    gain_envelope: [GainPoint];
}

table MidiClipData {
    length_ppqn: uint64;
    notes: [MidiNote];              // Inline MIDI data
    events: [MidiEvent];            // CC, pitch bend, etc.
    
    drum_map: DrumMap;
    
    // Compressed representation for large clips
    compressed: bool = false;
}
```

### 3.4. Mixer State Block

```cpp
table MixerState {
    channels: [MixerChannel];
    buses: [BusDefinition];
    
    master_volume: double = 0.0;
    master_output: AudioOutputConfig;
    master_dither: DitherConfig;
    master_limiter: LimiterConfig;
}

table MixerChannel {
    track_id: uint64;
    volume: double = 0.0;
    pan: double = 0.0;
    mute: bool = false;
    solo: bool = false;
    phase_invert: bool = false;
    width: double = 1.0;
    
    eq_state: EQState;
    fx_chain: [PluginSlot];
    sends: [SendSlot];
    
    bus_assignment: uint64 = 0;      // 0 = master
}
```

### 3.5. Plugin State Block

```cpp
table PluginState {
    entries: [PluginStateEntry];
}

table PluginStateEntry {
    plugin_id: string;              // "aria.compressor", "vendor.reverb"
    instance_id: uint64;
    format: string;                 // "CLAP", "VST3", "AU", "LV2"
    
    parameters: [ParameterValue];
    state_data: [uint8];            // Opaque plugin state blob
    preset_path: string;            // Reference to preset file
    
    // UI state
    editor_open: bool = false;
    editor_x: int = 0;
    editor_y: int = 0;
    editor_width: uint = 0;
    editor_height: uint = 0;
}

table ParameterValue {
    param_id: uint32;
    value: double;
    automated: bool = false;
}
```

---

## 4. Version Compatibility

### 4.1. Version Policy

```
ARIA Project Format Version Policy:

Major version: Breaking changes (blocks removed/restructured)
Minor version: Additive changes (new block types, new fields)
Patch version: Bug fixes (no structural changes)

Backward compatibility:
  - v1.x opens v1.0 files
  - v2.0 opens v1.x files (migration path)
  - Old blocks are read, unknown blocks are skipped
  - Missing blocks get default values

Forward compatibility:
  - v1.0 files opened in v2.0 lose new features
  - Unknown block types are preserved verbatim
  - New fields not present in old files default to safe values
```

### 4.2. Version Migration

```cpp
class ProjectMigration {
public:
    // Migrate project to current format version
    bool migrate(const std::string& path);
    
    // Check if migration is needed
    bool needs_migration(const ProjectHeader& header) const;
    
    // Migration path:
    // v1.0 → v1.1: Added markers block, no data change
    // v1.0 → v2.0: Block table format change
    // v2.0 → v2.1: Added preview audio block
    
    struct MigrationStep {
        uint32_t from_version;
        uint32_t to_version;
        std::function<bool(ProjectFile&)> migrate_fn;
    };
    
    static const std::vector<MigrationStep>& steps();

private:
    static bool migrate_1_0_to_1_1(ProjectFile& file);
    static bool migrate_1_x_to_2_0(ProjectFile& file);
    
    // Migration registry
    static std::vector<MigrationStep> registered_steps_;
};
```

---

## 5. Serialization

### 5.1. Serialization Format

```cpp
// ARIA uses FlatBuffers for block serialization.
// Benefits:
//   - Zero-copy access (memory-mappable)
//   - Forward/backward compatible (new fields are optional)
//   - Compact binary format
//   - Strong typing via schema
//   - No parsing step for read
//
// Blocks are optionally compressed with ZSTD:
//   - Blocks < 4KB: no compression (fast access)
//   - Blocks > 4KB: ZSTD level 3 (fast compression/decompression)
//   - Audio/MIDI data blocks: always compressed

class ProjectSerializer {
public:
    // Serialize project to file
    bool save(const std::string& path, const Project& project);
    
    // Deserialize project from file
    std::unique_ptr<Project> load(const std::string& path);
    
    // Validate file integrity
    bool validate(const std::string& path);
    
    // Export to JSON (debug/diff)
    bool export_json(const std::string& path, const std::string& json_path);

private:
    // Block-level operations
    bool write_block(std::ofstream& file, BlockTableEntry& entry,
                     const std::vector<uint8_t>& data);
    std::vector<uint8_t> read_block(std::ifstream& file,
                                     const BlockTableEntry& entry);
    
    // Compression
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data,
                                     uint64_t decompressed_size);
    
    // Integrity
    void update_block_table_crc(std::vector<BlockTableEntry>& table);
    bool verify_block_crc(const BlockTableEntry& entry,
                           const std::vector<uint8_t>& data);
};
```

### 5.2. ZSTD Compression

```cpp
// Compression settings:
// - Algorithm: ZSTD (zstandard)
// - Level: 3 (balanced speed/ratio)
// - Window: 4MB
//
// Benchmarks (100-track project):
//   Uncompressed: ~850KB
//   ZSTD level 3: ~120KB (7x reduction)
//   Compress time: ~5ms
//   Decompress time: ~2ms
```

---

## 6. Non-Destructive Editing

### 6.1. Copy-on-Write Model

```cpp
// ARIA uses a copy-on-write (COW) model for all project data.
// This means:
//   1. Source audio/MIDI files are NEVER modified
//   2. Edits create delta data stored in the project file
//   3. Undo stores reverse deltas
//   4. Zero data loss even on crash

class COWManager {
public:
    // Snapshot current state
    uint64_t snapshot(const std::string& label);
    
    // Undo to previous snapshot
    bool undo(uint64_t snapshot_id);
    
    // Redo
    bool redo(uint64_t snapshot_id);
    
    // Get undo history
    const std::vector<Snapshot>& history() const;
    
    // Memory management
    void set_max_undo_memory(uint64_t bytes);
    uint64_t undo_memory_usage() const;

private:
    struct Snapshot {
        uint64_t id;
        std::string label;
        uint64_t timestamp;
        std::vector<uint8_t> delta;  // Forward delta
        std::vector<uint8_t> reverse_delta;
        uint64_t memory_size;
    };
    
    std::vector<Snapshot> history_;
    uint64_t current_snapshot_ = 0;
    uint64_t max_memory_ = 1024 * 1024 * 1024;  // 1GB default
    
    // LRU eviction for old snapshots
    void evict_old_snapshots();
};
```

### 6.2. External File References

```cpp
// Audio files are referenced by path + hash, NOT embedded:
//
// Project.aria
// ──────────────────────
// AudioClipData {
//     file_path: "audio/kick.wav"     // Relative to project file
//     file_hash: "sha256:abc123..."    // Verify file integrity
//     sample_offset: 44100             // Start 1 second in
//     sample_length: 441000            // Use 10 seconds
// }
//
// If the file moves, ARIA uses the hash to find it.
// If the hash doesn't match, ARIA shows a "file not found" dialog
// with options to search/locate.

// Project folder structure:
// MyProject/
// ├── MyProject.aria
// ├── Audio/
// │   ├── kick.wav
// │   ├── snare.wav
// │   └── vox_take1.wav
// ├── Presets/
// │   └── serum_bass.fxp
// └── .cache/
//     ├── waveform_cache/
//     └── analysis_cache/
```

---

## 7. Auto-Save & Recovery

### 7.1. Auto-Save Strategy

```cpp
class AutoSaveManager {
public:
    // Save triggers
    enum class Trigger {
        Timer,          // Every 60 seconds
        ProjectSwitch,  // Loading another project
        PluginLoad,     // Before loading a plugin
        BeforeExport,   // Before export/bounce
        Idle,           // 5 seconds of no input + dirty
        Crash           // Emergency save on crash
    };
    
    // Auto-save project
    void save(Trigger trigger);
    
    // Check for auto-save file on startup
    bool has_auto_save() const;
    
    // Recover from auto-save
    std::unique_ptr<Project> recover();
    
    // Clear auto-save (on successful manual save)
    void clear_auto_save();
    
    // Configure
    void set_interval_seconds(uint32_t seconds);
    void set_max_auto_saves(uint32_t count);

private:
    std::string project_path_;
    uint32_t interval_seconds_ = 60;
    uint32_t max_auto_saves_ = 5;
    
    // File management
    std::string auto_save_path() const;
    void rotate_auto_saves();        // Keep last N auto-saves
    bool is_project_dirty() const;
};
```

### 7.2. Crash Recovery Flow

```
Application startup:
    │
    ├──► Check for .aria.auto file in project directory
    │
    ├──► If auto-save exists:
    │       ├── Show recovery dialog:
    │       │   "ARIA recovered an auto-saved project from [time].
    │       │    The last manual save was at [time].
    │       │    
    │       │    [Recover auto-save] [Open last manual save]
    │       │    [Discard both]"
    │       │
    │       ├── If recover:
    │       │   ├── Load auto-save
    │       │   ├── Rename .aria.auto → .aria.recovered
    │       │   └── Show "Recovered" banner
    │       │
    │       └── If discard:
    │           └── Delete .aria.auto
    │
    └──► Normal startup
```

### 7.3. Backup Strategy

```cpp
// Before overwriting the project file on save:
// 1. Rename current .aria → .aria.bak
// 2. Write new .aria file
// 3. If write succeeds: delete .bak
// 4. If write fails: restore .bak → .aria
// 5. Keep last 3 backups in backup folder
```

---

## 8. Backwards Compatibility

### 8.1. Compatibility Guarantee

```
ARIA guarantees that:
  ✓ Projects created in ARIA v1.0 open in v1.x, v2.x, v3.x, ..., v10.x
  ✓ All audio data remains intact across versions
  ✓ Mixer automation is preserved
  ✓ Plugin states are preserved (same plugin format)
  ✓ MIDI data is lossless across versions
  ✓ Tempo maps are preserved
  ✓ Markers and arrangement data is preserved

Limitations (documented):
  ⚠ Features not present in old versions are skipped
  ⚠ Plugin states depend on the same plugin being available
  ⚠ Routing may change if new routing types are introduced
```

### 8.2. Deprecation Policy

```cpp
// Deprecation timeline:
//
// v1.0: Format introduced
// v2.0: Old blocks marked deprecated (still readable)
// v3.0: Deprecated blocks produce a warning on load
// v4.0: Deprecated blocks no longer written (read-only)
// v5.0: Deprecated blocks removed (error on load, migration required)
//
// Each deprecation is documented in the migration path.
```

---

## 9. API Reference

### 9.1. Public API

```cpp
class ProjectFile {
public:
    // Open/Create/Save
    static std::unique_ptr<ProjectFile> create(const std::string& path);
    static std::unique_ptr<ProjectFile> open(const std::string& path);
    bool save();
    bool save_as(const std::string& path);
    void close();
    
    // File info
    std::string path() const;
    std::string name() const;
    uint64_t file_size() const;
    bool is_modified() const;
    bool is_read_only() const;
    
    // Version
    uint32_t format_version() const;
    uint32_t app_version() const;
    bool needs_upgrade() const;
    bool upgrade();
    
    // Validation
    bool validate_integrity() const;
    std::vector<std::string> integrity_errors() const;
    
    // Auto-save
    AutoSaveManager& autosave();
    
    // Undo
    void push_undo(const std::string& label);
    bool undo();
    bool redo();
    uint32_t undo_count() const;
    uint32_t redo_count() const;
    
    // Project data access
    ProjectMetadata& metadata();
    TrackList& tracks();
    Arrangement& arrangement();
    MixerState& mixer();
    PluginState& plugins();
    
    // File references
    std::vector<std::string> referenced_files() const;
    bool all_files_available() const;
    std::vector<std::string> missing_files() const;
};
```

---

## 10. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-PF-001 | Binary File Format & Header | 🔲 Pending |
| RFC-PF-002 | Block Table & Block Types | 🔲 Pending |
| RFC-PF-003 | FlatBuffers Schema Design | 🔲 Pending |
| RFC-PF-004 | ZSTD Compression Strategy | 🔲 Pending |
| RFC-PF-005 | CRC32 Integrity Checking | 🔲 Pending |
| RFC-PF-006 | Project Metadata Block | 🔲 Pending |
| RFC-PF-007 | Track Serialization | 🔲 Pending |
| RFC-PF-008 | Arrangement & Clip Serialization | 🔲 Pending |
| RFC-PF-009 | MIDI Clip Compression | 🔲 Pending |
| RFC-PF-010 | Mixer State Serialization | 🔲 Pending |
| RFC-PF-011 | Plugin State & Parameter Serialization | 🔲 Pending |
| RFC-PF-012 | Version Migration Path | 🔲 Pending |
| RFC-PF-013 | Auto-Save & Crash Recovery | 🔲 Pending |
| RFC-PF-014 | Copy-on-Write Undo Model | 🔲 Pending |
| RFC-PF-015 | External File Reference Resolution | 🔲 Pending |
| RFC-PF-016 | Project Template System | 🔲 Pending |
| RFC-PF-017 | Forward Compatibility (Unknown Blocks) | 🔲 Pending |
| RFC-PF-018 | JSON Export/Import (Debug) | 🔲 Pending |

---

## Appendix A: File Size Benchmarks

| Project Type | Tracks | Clips | File Size | Load Time |
|---|---|---|---|---|
| Empty project | 0 | 0 | ~4KB | < 50ms |
| Beat (16 tracks) | 16 | 48 | ~32KB | < 100ms |
| Song (32 tracks) | 32 | 120 | ~85KB | < 200ms |
| Full production (100 tracks) | 100 | 450 | ~250KB | < 500ms |
| Orchestral (200 tracks) | 200 | 800 | ~500KB | < 1s |
| Max test (1000 tracks) | 1000 | 3000 | ~2MB | < 3s |

*File sizes exclude audio file references (which are not embedded).*
*Load time measured from SSD (NVMe).*

## Appendix B: Constants

| Constant | Value | Description |
|---|---|---|
| MAX_BLOCKS | 65536 | Maximum blocks per project |
| MAX_BLOCK_SIZE | 256MB | Maximum uncompressed block size |
| MIN_COMPRESSION_SIZE | 4096 | Blocks smaller than this are not compressed |
| AUTO_SAVE_INTERVAL | 60s | Default auto-save interval |
| MAX_UNDO_MEMORY | 1GB | Default undo memory limit |
| MAX_BACKUPS | 3 | Number of backup files to keep |
| FILE_MAGIC | "ARIA" | File format magic bytes |
| FILE_EXTENSION | ".aria" | Primary file extension |
