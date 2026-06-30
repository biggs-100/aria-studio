# Database Indexing Specification

## Purpose

SQLite-backed persistent metadata store for samples, plugins, projects, presets, and tags. Provides automatic migrations, WAL-mode concurrency, and FTS5 full-text search indexes.

## Requirements

### Requirement: Schema Management

The system MUST apply all migrations in order on database open, tracking versions via `schema_version` table.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Fresh DB | No `schema_version` table | Database opens | All migrations run in order |
| 2 | Up-to-date DB | `schema_version` at v3 with MIGRATIONS containing v1–v5 | Database opens | Only v4–v5 applied |
| 3 | Migration failure | A migration SQL statement is malformed | Applying migrations | Error returned, DB unchanged |
| 4 | Destructive backup | Migration has non-None `down` SQL | Before applying | Backup file created at `[config_dir]/backups/` |

### Requirement: Concurrency

The database MUST operate in WAL mode with `PRAGMA synchronous=NORMAL`.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Concurrent reads | DB open with WAL, two reader connections | Both query simultaneously | Both return results without SQLITE_BUSY |
| 2 | Read during write | Indexer writing batch inserts | Another connection queries | Reader sees pre-transaction snapshot |
| 3 | Write contention | Two writers attempt simultaneous transactions | Both call COMMIT | Second writer blocks or fails with SQLITE_BUSY |

### Requirement: CRUD Operations

The system SHALL provide insert, update, delete, and query APIs for samples, plugins, projects, presets, and tags.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Insert sample | A valid `SampleEntry` with unique file_path | `insert_sample()` called | Returns new row ID; FTS index updated |
| 2 | Duplicate path | Sample with file_path already exists | `insert_sample()` called | Returns error or upserts |
| 3 | Tag assignment | Existing sample and tag with IDs 1 and 2 | `tag_sample(1, 2)` called | Row added to `sample_tags` |
| 4 | Delete cascade | Sample with ID 1 has tag assignments | `delete_sample(1)` called | Sample + related sample_tags deleted |

### Requirement: Batch Performance

Batch inserts MUST use explicit transactions for indexing speed.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Batch insert | 1000 sample entries | Inserted in single transaction | All committed; FTS triggers fire per row |
| 2 | Batch rollback | 500 of 1000 inserts fail unique constraint | Transaction rolls back | Zero rows inserted |

### Requirement: Maintenance

The system SHOULD expose `vacuum()`, `backup()`, and `integrity_check()` operations.

| # | Scenario | GIVEN | WHEN | THEN |
|---|----------|-------|------|------|
| 1 | Integrity check | DB with valid data | `integrity_check()` called | Returns empty error list |
| 2 | Remove missing | 3 files no longer exist on disk | `remove_missing_files()` called | 3 sample rows deleted |
