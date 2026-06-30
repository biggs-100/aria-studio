# Search Engine Specification

## Purpose

FTS5-powered search across samples, plugins, projects, and presets. Supports full-text queries, filtered search (BPM, key, category, tags, rating), sorting by multiple criteria, and returns results within 50ms for 100k samples.

## Requirements

### Requirement: Full-Text Search

The engine MUST query FTS5 virtual tables using BM25 ranking.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Simple text | DB has 100k samples with varied names | Query "dark kick" | Results ranked by relevance (BM25); files matching both terms ranked highest |
| 2 | No matches | No samples match query "xyzzy" | Query runs | Empty result set with total_count=0 |
| 3 | Special characters | User searches "vocal + fx" | Query runs | Special chars sanitized; valid FTS5 query built |

### Requirement: Filtered Search

The engine SHALL filter by category, BPM range, musical key, format, tags, favorite status, and minimum rating.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Multi-filter | Samples exist at 80–200 BPM in various categories | Query: category=kick, bpm 110–130, favorite=true | Only kick samples 110-130 BPM marked favorite returned |
| 2 | Tag filter | Samples tagged "dark" AND "sub-bass" | Query: both tags required | Only samples with both tags (via sample_tags join) returned |
| 3 | No filter (browse) | User browses with no text query | category=bass, sort=bpm | All bass samples sorted by BPM |

### Requirement: Sorting

Results MUST sort by relevance (default), name, date, BPM, or duration, ascending or descending.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Sort by BPM ascending | Samples with BPM 60–180 | sort_by=bpm, ascending=true | Lowest BPM first |
| 2 | Sort by date | Samples indexed over 30 days | sort_by=date, ascending=false | Most recently indexed first |

### Requirement: Multi-Entity Search

The engine SHALL provide separate search methods for samples, plugins, projects, and presets.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Plugin search | 500 plugins in DB | `search_plugins(text="compressor", category="effect")` | Matching compressor plugins returned |
| 2 | Project search | 100 projects in DB | `search_projects(text="track", author="user")` | Projects matching query returned |

### Requirement: Performance

The engine MUST return results in under 50ms for 100k indexed samples.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Large result set | 100k samples, full-text query matching 80k | Query with LIMIT 100 | Response time < 50ms; total_count reflects 80k |
| 2 | Complex filter | 100k samples, multi-filter + FTS + tag join + sort | Query runs | Response time < 50ms |
