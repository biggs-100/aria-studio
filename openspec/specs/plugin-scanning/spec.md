# Plugin Scanning Specification

## Purpose

Discover CLAP and VST3 plugins by incremental file scanning using modification times. Cache results in JSON to avoid full rescans on startup. Provide format scanners for `.clap` and `.vst3` files.

## Requirements

### Requirement: Incremental Scan by mtime

On startup, the scanner MUST compare each cached file's current mtime against the stored value and only scan files whose mtime changed. Deleted files MUST be removed from cache. New files MUST be discovered. A full rescan MUST clear the cache and scan everything.

#### Scenario: New plugin discovered without rescan
- GIVEN a cache with 10 known plugins
- WHEN a new `.clap` file appears in a scan path
- THEN only that file is opened and parsed AND the cache now has 11 entries

#### Scenario: Deleted plugin removed from cache
- GIVEN a cached plugin whose file was deleted
- WHEN an incremental scan runs
- THEN the plugin is removed from the registry AND unavailable in `available_plugins()`

#### Scenario: Updated plugin re-scanned
- GIVEN a cached plugin whose mtime changed after an update
- WHEN an incremental scan runs
- THEN the plugin is fully re-scanned AND the descriptor replaces the old entry

### Requirement: JSON Cache

The JSON cache at `~/.aria/plugin-cache.json` MUST store `file_path`, `file_size`, `modified_time`, and `PluginDescriptor` array. It MUST be written atomically (temp + rename). Corrupt or missing cache MUST trigger a full scan.

#### Scenario: Corrupt cache triggers full rescan
- GIVEN a truncated `plugin-cache.json`
- WHEN the scanner loads it on startup
- THEN parsing fails AND the scanner falls back to a full scan AND a warning is logged

#### Scenario: Atomic write prevents corruption
- GIVEN a power loss during cache write
- WHEN the system restarts
- THEN the cache is either the complete old file or the complete new file, never half-written

### Requirement: Format Scanners

CLAP scanner MUST load the `.clap` library, call `clap_plugin_entry`, enumerate the factory, and extract descriptors. VST3 scanner MUST load the `.vst3` bundle, query `IPluginFactory`, and enumerate classes. Both MUST report multiple plugins per file.

#### Scenario: CLAP scanner enumerates all factory plugins
- GIVEN a `.clap` file with 3 plugins
- WHEN the CLAP scanner enumerates the factory
- THEN it returns 3 PluginDescriptor entries each with a unique PluginID

#### Scenario: VST3 single effect scans correctly
- GIVEN a `.vst3` bundle with one audio effect
- WHEN the VST3 scanner queries `IPluginFactory`
- THEN it returns 1 PluginDescriptor with category `Effect`

### Requirement: Default Scan Paths

The scanner MUST include platform defaults: Windows — `%PROGRAMFILES%\Common Files\CLAP\`, `%PROGRAMFILES%\Common Files\VST3\`, `%APPDATA%\ARIA\Plugins\`. Users MUST add and remove custom paths.

#### Scenario: Custom path adds plugins
- GIVEN plugins in `D:\MyVSTs\`
- WHEN `add_scan_path("D:\\MyVSTs")` is called and a scan runs
- THEN plugins there are discovered AND the path persists for subsequent scans

### Requirement: Scan Cancellation

A running scan MUST be cancellable. The cache MUST contain only fully scanned entries. Progress MUST report as 0.0–1.0.

#### Scenario: Partial scan preserves prior results
- GIVEN a scan of 50 plugins with 20 completed
- WHEN `cancel_scan()` is called
- THEN scanning stops after the current file AND only the 20 completed entries are cached
