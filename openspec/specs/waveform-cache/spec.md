# Waveform Cache Specification

## Purpose

Generates peak/minima waveform data for browser preview thumbnails. Caches results per-file on disk (keyed by file hash) and keeps an LRU memory cache for recently accessed files. Independent of the AudioClip's internal waveform cache.

## Requirements

### Requirement: Waveform Generation

The system MUST generate peak and minima arrays at a requested resolution (samples-per-pixel).

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Standard resolution | Audio file with 441000 samples | `generate(path, resolution=1024)` called | Returns peaks and minima arrays each of length ≤1024 |
| 2 | Short file | File with 100 samples | `generate(path, resolution=1024)` called | Returns waveform arrays with fewer than 1024 values |
| 3 | Corrupted file | File is not valid audio | `generate(path, 1024)` called | Returns nullopt/error |

### Requirement: Disk Cache

Generated waveform data MUST be cached on disk, keyed by the file's SHA-256 hash. Cache location: `[config_dir]/waveform_cache/[hash].wvc`.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Cache hit | Previously generated file with same hash | `get_waveform(path, 1024)` called | Loads from disk cache; no decode needed |
| 2 | Cache miss | New file with different hash | `get_waveform(path, 1024)` called | Generates from scratch; saves to disk cache |
| 3 | File modified | File content changed (new hash) | `get_waveform(path, 1024)` called | Old cache entry (old hash) not found; new waveform generated and cached |

### Requirement: Memory Cache (LRU)

A fixed-size in-memory LRU cache SHALL hold the most recently accessed waveform data (default: 100 entries).

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Repeated access | User scrolls through same 50 files repeatedly | Browser loads waveforms | 50 entries stay in memory cache; no disk reads after first load |
| 2 | LRU eviction | User views 150 unique files (limit=100) | Waveform loaded for file #150 | Oldest entry evicted from memory cache |
| 3 | Memory pressure | System low memory | Cache grows | Entries evicted; never exceeds configured capacity |

### Requirement: Cache Maintenance

The system SHALL expose `clear_cache()` and `cleanup_orphaned()` for cache maintenance.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Clear cache | Disk cache has 500 entries | `clear_cache()` called | All cache files deleted; memory cache cleared |
| 2 | Cleanup orphans | 20 cache files whose source files no longer exist on disk | `cleanup_orphaned()` called | 20 orphaned cache files deleted |

### Requirement: Cache Statistics

The system SHALL expose hit/miss counts, cached file count, and total cache size.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Track stats | 100 cache hits, 5 misses, 300MB disk usage | `stats()` called | Returns `cache_hits=100, cache_misses=5, cached_files=N, cache_size_bytes=300000000` |
