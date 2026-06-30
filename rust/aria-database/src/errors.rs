// ARIA DAW — Database Error Types
// Typed error enum for all database operations.

use std::fmt;

/// Unified error type for all database-related operations.
#[derive(Debug, Clone, PartialEq)]
pub enum DbError {
    /// SQLite-level error (connection, query, constraint violations)
    Sqlite(String),
    /// Migration-specific error (ordering, SQL failure, backup)
    Migration(String),
    /// Resource not found (sample, tag, plugin by ID/path)
    NotFound(String),
    /// Duplicate resource (unique constraint on file_path, tag name, etc.)
    Duplicate(String),
}

impl fmt::Display for DbError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DbError::Sqlite(msg) => write!(f, "SQLite error: {}", msg),
            DbError::Migration(msg) => write!(f, "Migration error: {}", msg),
            DbError::NotFound(msg) => write!(f, "Not found: {}", msg),
            DbError::Duplicate(msg) => write!(f, "Duplicate: {}", msg),
        }
    }
}

impl std::error::Error for DbError {}

impl From<rusqlite::Error> for DbError {
    fn from(e: rusqlite::Error) -> Self {
        DbError::Sqlite(e.to_string())
    }
}

/// Convenience alias for Results using DbError.
pub type DbResult<T> = Result<T, DbError>;

#[cfg(test)]
mod tests {
    use super::*;

    // ── RED: Tests written first, referencing DbError type ─────

    #[test]
    fn test_sqlite_error_display() {
        let err = DbError::Sqlite("disk I/O error".into());
        let msg = err.to_string();
        assert_eq!(msg, "SQLite error: disk I/O error");
    }

    #[test]
    fn test_migration_error_display() {
        let err = DbError::Migration("version 3 failed".into());
        assert_eq!(err.to_string(), "Migration error: version 3 failed");
    }

    #[test]
    fn test_not_found_error_display() {
        let err = DbError::NotFound("sample id 42".into());
        assert_eq!(err.to_string(), "Not found: sample id 42");
    }

    #[test]
    fn test_duplicate_error_display() {
        let err = DbError::Duplicate("file_path already exists".into());
        assert_eq!(err.to_string(), "Duplicate: file_path already exists");
    }

    #[test]
    fn test_from_rusqlite_error() {
        let sqlite_err = rusqlite::Error::InvalidParameterName("foo".into());
        let db_err: DbError = sqlite_err.into();
        match db_err {
            DbError::Sqlite(_) => {} // correct variant
            other => panic!("Expected Sqlite variant, got: {}", other),
        }
    }

    #[test]
    fn test_dbresult_alias() {
        let ok: DbResult<i32> = Ok(42);
        assert_eq!(ok.unwrap(), 42);

        let err: DbResult<i32> = Err(DbError::NotFound("test".into()));
        assert!(err.is_err());
    }

    #[test]
    fn test_error_trait_impl() {
        fn assert_error<E: std::error::Error>() {}
        assert_error::<DbError>();
    }

    // ── TRIANGULATE: Edge cases ──────────────────────────────

    #[test]
    fn test_error_with_empty_message() {
        let err = DbError::Sqlite("".into());
        assert_eq!(err.to_string(), "SQLite error: ");
    }

    #[test]
    fn test_not_found_with_id() {
        let err = DbError::NotFound("tag #999".into());
        assert!(err.to_string().contains("tag #999"));
    }

    #[test]
    fn test_dbresult_can_be_used_in_function() {
        fn find_sample(id: i64) -> DbResult<String> {
            if id == 1 {
                Ok("found".into())
            } else {
                Err(DbError::NotFound(format!("sample id {}", id)))
            }
        }

        assert_eq!(find_sample(1).unwrap(), "found");
        let err = find_sample(99).unwrap_err();
        assert_eq!(err.to_string(), "Not found: sample id 99");
    }
}
