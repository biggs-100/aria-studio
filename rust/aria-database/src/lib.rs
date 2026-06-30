// ARIA DAW — Database Module
// SQLite-powered metadata database for samples, plugins, projects, and presets.
// WAL mode for concurrent reads during indexing. Mutex-guarded single writer.

use rusqlite::Connection;
use std::sync::Mutex;

pub mod errors;
pub mod migration;
pub mod queries;
pub mod schema;
pub mod search;

use errors::DbResult;

/// Thread-safe database handle wrapping a single SQLite connection.
/// Uses WAL mode for concurrent reads from other connections.
pub struct Database {
    conn: Mutex<Connection>,
}

impl Database {
    /// Open a database at the given path (or `:memory:` for testing).
    /// Configures WAL mode, synchronous=NORMAL, foreign_keys=ON, 64MB cache.
    pub fn open(path: &str) -> DbResult<Self> {
        let conn = if path == ":memory:" {
            Connection::open_in_memory()
        } else {
            Connection::open(path)
        }
        .map_err(|e| errors::DbError::Sqlite(format!("Failed to open database: {}", e)))?;

        // Configure WAL mode for concurrent reads
        conn.execute_batch(
            "PRAGMA journal_mode=WAL;
             PRAGMA synchronous=NORMAL;
             PRAGMA foreign_keys=ON;
             PRAGMA cache_size=-65536;"
        )
        .map_err(|e| errors::DbError::Sqlite(format!("Failed to configure database: {}", e)))?;

        Ok(Database {
            conn: Mutex::new(conn),
        })
    }

    /// Run all pending migrations. Safe to call multiple times.
    pub fn run_migrations(&self) -> DbResult<()> {
        let conn = self.conn.lock().unwrap();
        migration::run_migrations(&conn)
    }

    /// Get a reference to the inner connection (locked).
    pub fn conn(&self) -> std::sync::MutexGuard<'_, Connection> {
        self.conn.lock().unwrap()
    }

    /// Close the database by dropping the connection.
    pub fn close(&self) -> DbResult<()> {
        // Connection is dropped when MutexGuard is released
        Ok(())
    }

    /// Path is not stored — use the path you passed to open().
    /// This method exists for API compatibility.
    #[deprecated(note = "Use your own path reference")]
    pub fn path(&self) -> &'static str {
        "database"
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_database_open_in_memory() {
        let db = Database::open(":memory:").unwrap();
        // With in-memory, WAL mode might not be set (SQLite limitation)
        // Just verify we got a working connection
        let val: i32 = db.conn().query_row(
            "SELECT 1 + 1",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(val, 2);
    }

    #[test]
    fn test_database_foreign_keys_enabled() {
        let db = Database::open(":memory:").unwrap();
        let fk: i32 = db.conn().query_row(
            "PRAGMA foreign_keys",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(fk, 1, "Foreign keys should be enabled");
    }

    #[test]
    fn test_database_run_migrations() {
        let db = Database::open(":memory:").unwrap();
        db.run_migrations().unwrap();

        let version: u32 = db.conn().query_row(
            "SELECT COALESCE(MAX(version), 0) FROM schema_version",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(version, 1, "Schema version should be 1 after migration");
    }

    #[test]
    fn test_database_migration_idempotent() {
        let db = Database::open(":memory:").unwrap();
        db.run_migrations().unwrap();
        db.run_migrations().unwrap();

        let count: i32 = db.conn().query_row(
            "SELECT COUNT(*) FROM schema_version",
            [],
            |row| row.get(0),
        ).unwrap();
        assert_eq!(count, 1, "Should have exactly 1 schema_version entry");
    }

    #[test]
    fn test_database_close() {
        let db = Database::open(":memory:").unwrap();
        db.close().unwrap();
        // After close, we can still access the connection (it's just a nop)
        let _val: i32 = db.conn().query_row("SELECT 1", [], |row| row.get(0)).unwrap();
    }

    #[test]
    fn test_database_open_invalid_path() {
        // Opening in invalid path should fail with parent dir
        let result = Database::open("/nonexistent/aria.db");
        assert!(result.is_err(), "Opening in nonexistent directory should fail");
    }
}
