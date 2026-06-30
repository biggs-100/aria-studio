// ARIA DAW — Database Migration System
// Ordered migrations with schema_version tracking.
// Migration runner applies pending migrations on database open.

use crate::errors::{DbError, DbResult};
use rusqlite::{params, Connection};

/// A single migration step.
pub struct Migration {
    pub version: u32,
    pub description: &'static str,
    pub up: &'static str,
    pub down: Option<&'static str>,
}

/// Ordered list of database migrations. Applied in order on DB open.
pub const MIGRATIONS: &[Migration] = &[
    Migration {
        version: 1,
        description: "Initial schema: samples, plugins, projects, presets, tags, FTS5, watched_folders, search_history",
        up: crate::schema::CREATE_SCHEMA_SQL,
        down: None,
    },
];

/// Apply all pending migrations in order.
///
/// 1. Reads current version from `schema_version` (0 if table doesn't exist).
/// 2. Applies each migration with version > current in ascending order.
/// 3. Before applying a migration with `down = Some(...)`, creates a backup
///    at `[db_dir]/backups/aria.db.v{major}.{minor}.bak`.
/// 4. Records each migration version in `schema_version` on success.
pub fn run_migrations(conn: &Connection) -> DbResult<()> {
    // Ensure schema_version tracking table exists
    conn.execute_batch(
        "CREATE TABLE IF NOT EXISTS schema_version (
            version     INTEGER PRIMARY KEY,
            applied_at  TEXT NOT NULL DEFAULT (datetime('now')),
            description TEXT
        );"
    ).map_err(|e| DbError::Migration(format!("Failed to create schema_version table: {}", e)))?;

    // Read current version
    let current: u32 = conn.query_row(
        "SELECT COALESCE(MAX(version), 0) FROM schema_version",
        [],
        |row| row.get(0),
    ).map_err(|e| DbError::Migration(format!("Failed to read current schema version: {}", e)))?;

    // Apply pending migrations
    for migration in MIGRATIONS {
        if migration.version > current {
            // Backup before destructive migrations
            if migration.down.is_some() {
                if let Ok(db_path) = conn.query_row(
                    "PRAGMA database_list",
                    [],
                    |row| row.get::<_, String>(2),
                ) {
                    if !db_path.is_empty() && db_path != ":memory:" {
                        backup_database(conn, &db_path, migration.version)?;
                    }
                }
            }

            // Apply the migration SQL
            conn.execute_batch(migration.up).map_err(|e| {
                DbError::Migration(format!(
                    "Migration v{} ({}) failed: {}",
                    migration.version, migration.description, e
                ))
            })?;

            // Record the migration
            conn.execute(
                "INSERT INTO schema_version (version, description) VALUES (?1, ?2)",
                params![migration.version, migration.description],
            ).map_err(|e| {
                DbError::Migration(format!(
                    "Failed to record schema_version v{}: {}",
                    migration.version, e
                ))
            })?;
        }
    }

    Ok(())
}

/// Get the current schema version (highest applied migration).
pub fn get_current_version(conn: &Connection) -> DbResult<u32> {
    conn.query_row(
        "SELECT COALESCE(MAX(version), 0) FROM schema_version",
        [],
        |row| row.get(0),
    ).map_err(|e| DbError::Migration(format!("Failed to get schema version: {}", e)))
}

/// Create a backup of the database before applying a destructive migration.
fn backup_database(conn: &Connection, db_path: &str, version: u32) -> DbResult<()> {
    use std::path::Path;

    let db_path = Path::new(db_path);
    let backup_dir = db_path.parent()
        .map(|p| p.join("backups"))
        .unwrap_or_else(|| Path::new("backups").to_path_buf());

    std::fs::create_dir_all(&backup_dir)
        .map_err(|e| DbError::Migration(format!("Failed to create backup dir: {}", e)))?;

    let timestamp = chrono_now_fallback();
    let backup_name = format!(
        "aria.db.v{}.{}.bak",
        version, timestamp
    );
    let backup_path = backup_dir.join(backup_name);

    // Use SQLite backup API via rusqlite
    conn.backup(rusqlite::DatabaseName::Main, &backup_path, None)
        .map_err(|e| DbError::Migration(format!("Backup failed: {}", e)))?;

    Ok(())
}

/// Fallback timestamp for backup naming (no chrono dep).
fn chrono_now_fallback() -> String {
    // Simple timestamp: YYYYMMDD_HHMMSS
    use std::time::{SystemTime, UNIX_EPOCH};
    let dur = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default();
    let secs = dur.as_secs();
    let days = secs / 86400;
    let time = secs % 86400;
    let hours = time / 3600;
    let mins = (time % 3600) / 60;
    let sec = time % 60;

    // Approximate date from days since epoch
    let year = 1970 + (days as f64 / 365.25) as u64;
    let rem_days = days - ((year - 1970) as f64 * 365.25) as u64;
    let month = 1 + rem_days / 28;
    let day = 1 + rem_days % 28;

    format!("{:04}{:02}{:02}_{:02}{:02}{:02}", year, month.min(12), day.min(28), hours, mins, sec)
}

#[cfg(test)]
mod tests {
    use super::*;
    use rusqlite::Connection;

    // ── RED: Tests written before migration runner implementation ──

    #[test]
    fn test_migration_creates_schema_version() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        let version: u32 = conn.query_row(
            "SELECT COALESCE(MAX(version), 0) FROM schema_version",
            [],
            |row| row.get(0),
        ).unwrap();
        assert!(version >= 1, "Schema version should be >= 1 after migration");
    }

    #[test]
    fn test_migration_tables_exist() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        // Verify core tables exist
        let tables = ["samples", "plugins", "projects", "presets", "tags",
                       "sample_tags", "plugin_tags", "watched_folders",
                       "search_history", "sample_analysis"];
        for table in &tables {
            let count: i32 = conn.query_row(
                "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?1",
                params![table],
                |row| row.get(0),
            ).unwrap();
            assert_eq!(count, 1, "Table '{}' should exist", table);
        }
    }

    #[test]
    fn test_migration_fts_tables_exist() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        let fts_tables = ["samples_fts", "plugins_fts", "projects_fts", "presets_fts"];
        for table in &fts_tables {
            let count: i32 = conn.query_row(
                "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?1",
                params![table],
                |row| row.get(0),
            ).unwrap();
            assert_eq!(count, 1, "FTS table '{}' should exist", table);
        }
    }

    #[test]
    fn test_migration_triggers_exist() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        let triggers = ["samples_ai", "samples_ad", "samples_au"];
        for trigger in &triggers {
            let count: i32 = conn.query_row(
                "SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' AND name=?1",
                params![trigger],
                |row| row.get(0),
            ).unwrap();
            assert_eq!(count, 1, "Trigger '{}' should exist", trigger);
        }
    }

    #[test]
    fn test_migration_idempotent() {
        let conn = Connection::open_in_memory().unwrap();

        // Run twice — second run should be a no-op
        run_migrations(&conn).unwrap();
        run_migrations(&conn).unwrap();

        let count: i32 = conn.query_row(
            "SELECT COUNT(*) FROM schema_version",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(count, 1, "Only one schema_version entry after idempotent run");
    }

    #[test]
    fn test_migration_records_description() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        let (version, desc): (u32, String) = conn.query_row(
            "SELECT version, description FROM schema_version ORDER BY version DESC LIMIT 1",
            [],
            |row| Ok((row.get(0)?, row.get(1)?)),
        ).unwrap();

        assert_eq!(version, 1);
        assert!(desc.contains("Initial schema"), "Description should mention 'Initial schema'");
    }

    #[test]
    fn test_get_current_version() {
        let conn = Connection::open_in_memory().unwrap();

        // Before migration
        assert_eq!(get_current_version(&conn).unwrap_or(0), 0);

        // After migration
        run_migrations(&conn).unwrap();
        assert_eq!(get_current_version(&conn).unwrap(), 1);
    }

    #[test]
    fn test_sample_fts_sync_trigger_insert() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        // Insert a sample — FTS trigger should fire
        conn.execute(
            "INSERT INTO samples (file_path, file_hash, file_size, file_format, name, category)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params!["/test/kick.wav", "abc123", 4096, "wav", "Kick 1", "kick"],
        ).unwrap();

        // Verify FTS index has the row
        let count: i32 = conn.query_row(
            "SELECT COUNT(*) FROM samples_fts",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(count, 1, "FTS should have 1 entry after insert");
    }

    #[test]
    fn test_sample_fts_sync_trigger_delete() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        conn.execute(
            "INSERT INTO samples (file_path, file_hash, file_size, file_format)
             VALUES (?1, ?2, ?3, ?4)",
            params!["/test/snare.wav", "def456", 2048, "wav"],
        ).unwrap();

        conn.execute("DELETE FROM samples WHERE id = 1", []).unwrap();

        let count: i32 = conn.query_row(
            "SELECT COUNT(*) FROM samples_fts",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(count, 0, "FTS should have 0 entries after delete");
    }

    #[test]
    fn test_indexes_exist() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        let indexes = ["idx_samples_category", "idx_samples_bpm",
                        "idx_samples_musical_key", "idx_samples_favorite",
                        "idx_samples_rating", "idx_samples_file_hash",
                        "idx_plugins_category", "idx_plugins_format",
                        "idx_projects_last_opened", "idx_projects_name",
                        "idx_presets_plugin", "idx_presets_category",
                        "idx_search_history_time"];
        for idx in &indexes {
            let count: i32 = conn.query_row(
                "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name=?1",
                params![idx],
                |row| row.get(0),
            ).unwrap();
            assert_eq!(count, 1, "Index '{}' should exist", idx);
        }
    }

    #[test]
    fn test_migration_fresh_db_all_migrations_applied() {
        let conn = Connection::open_in_memory().unwrap();
        run_migrations(&conn).unwrap();

        let count: i32 = conn.query_row(
            "SELECT COUNT(*) FROM schema_version",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(count, MIGRATIONS.len() as i32,
            "All {} migrations should be recorded", MIGRATIONS.len());
    }
}
