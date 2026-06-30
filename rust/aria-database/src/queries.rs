// ARIA DAW — Database Queries
// Prepared-statement-based CRUD operations for all entity types.
// All functions take a `&Connection` for use from both Database and FFI layers.

use crate::errors::{DbError, DbResult};
use rusqlite::{params, Connection, types::Value};

// ── Data Types ──────────────────────────────────────────────

/// Input data for inserting or updating a sample.
#[derive(Debug, Clone)]
pub struct SampleEntry {
    pub file_path: String,
    pub file_hash: String,
    pub file_size: i64,
    pub file_format: String,
    pub sample_rate: Option<i32>,
    pub bit_depth: Option<i32>,
    pub channels: Option<i32>,
    pub duration_ms: Option<i32>,
    pub bpm: Option<f64>,
    pub bpm_confidence: Option<f64>,
    pub musical_key: Option<String>,
    pub key_confidence: Option<f64>,
    pub loudness_lufs: Option<f64>,
    pub peak_db: Option<f64>,
    pub category: Option<String>,
    pub subcategory: Option<String>,
    pub mood: Option<String>,
    pub name: Option<String>,
    pub rating: Option<i32>,
    pub favorite: Option<i32>,
    pub color: Option<String>,
    pub notes: Option<String>,
    pub tags: Option<String>,
    pub file_created: Option<String>,
    pub file_modified: Option<String>,
}

/// Result data for a sample (returned from queries).
#[derive(Debug, Clone)]
pub struct SampleResult {
    pub id: i64,
    pub file_path: String,
    pub file_hash: String,
    pub file_size: i64,
    pub file_format: String,
    pub sample_rate: Option<i32>,
    pub bit_depth: Option<i32>,
    pub channels: Option<i32>,
    pub duration_ms: Option<i32>,
    pub bpm: Option<f64>,
    pub musical_key: Option<String>,
    pub loudness_lufs: Option<f64>,
    pub peak_db: Option<f64>,
    pub category: Option<String>,
    pub name: Option<String>,
    pub rating: i32,
    pub favorite: i32,
    pub tags: Option<String>,
    pub first_indexed: Option<String>,
    pub last_indexed: Option<String>,
}

/// Search query parameters for FTS5 + filtered search.
#[derive(Debug, Clone)]
pub struct SearchQuery {
    pub text: String,
    pub category: Option<String>,
    pub musical_key: Option<String>,
    pub bpm_min: Option<f64>,
    pub bpm_max: Option<f64>,
    pub favorite_only: bool,
    pub min_rating: i32,
    pub limit: u32,
    pub offset: u32,
}

/// A single tag.
#[derive(Debug, Clone)]
pub struct Tag {
    pub id: i64,
    pub name: String,
    pub color: Option<String>,
}

// ── Samples CRUD ────────────────────────────────────────────

/// Insert a new sample. Returns the new row ID.
/// Returns Err(DbError::Duplicate) if file_path already exists.
pub fn insert_sample(conn: &Connection, entry: &SampleEntry) -> DbResult<i64> {
    // Check for duplicate first
    let exists: bool = conn.query_row(
        "SELECT COUNT(*) > 0 FROM samples WHERE file_path = ?1",
        params![entry.file_path],
        |row| row.get(0),
    )?;

    if exists {
        return Err(DbError::Duplicate(format!(
            "Sample with path '{}' already exists",
            entry.file_path
        )));
    }

    let mut stmt = conn.prepare_cached(
        "INSERT INTO samples (
            file_path, file_hash, file_size, file_format,
            sample_rate, bit_depth, channels, duration_ms,
            bpm, bpm_confidence, musical_key, key_confidence,
            loudness_lufs, peak_db,
            category, subcategory, mood,
            name, rating, favorite, color, notes,
            tags, file_created, file_modified
        ) VALUES (
            ?1, ?2, ?3, ?4,
            ?5, ?6, ?7, ?8,
            ?9, ?10, ?11, ?12,
            ?13, ?14,
            ?15, ?16, ?17,
            ?18, ?19, ?20, ?21, ?22,
            ?23, ?24, ?25
        )"
    )?;

    stmt.execute(params![
        entry.file_path,
        entry.file_hash,
        entry.file_size,
        entry.file_format,
        entry.sample_rate,
        entry.bit_depth,
        entry.channels,
        entry.duration_ms,
        entry.bpm,
        entry.bpm_confidence,
        entry.musical_key,
        entry.key_confidence,
        entry.loudness_lufs,
        entry.peak_db,
        entry.category,
        entry.subcategory,
        entry.mood,
        entry.name,
        entry.rating,
        entry.favorite,
        entry.color,
        entry.notes,
        entry.tags,
        entry.file_created,
        entry.file_modified,
    ])?;

    Ok(conn.last_insert_rowid())
}

/// Update an existing sample by ID. Returns Ok(()) on success,
/// Err(DbError::NotFound) if the sample doesn't exist.
pub fn update_sample(conn: &Connection, id: i64, entry: &SampleEntry) -> DbResult<()> {
    let affected = conn.execute(
        "UPDATE samples SET
            file_path = ?1, file_hash = ?2, file_size = ?3, file_format = ?4,
            sample_rate = ?5, bit_depth = ?6, channels = ?7, duration_ms = ?8,
            bpm = ?9, bpm_confidence = ?10, musical_key = ?11, key_confidence = ?12,
            loudness_lufs = ?13, peak_db = ?14,
            category = ?15, subcategory = ?16, mood = ?17,
            name = ?18, rating = ?19, favorite = ?20, color = ?21, notes = ?22,
            tags = ?23, file_created = ?24, file_modified = ?25,
            last_indexed = datetime('now')
        WHERE id = ?26",
        params![
            entry.file_path, entry.file_hash, entry.file_size, entry.file_format,
            entry.sample_rate, entry.bit_depth, entry.channels, entry.duration_ms,
            entry.bpm, entry.bpm_confidence, entry.musical_key, entry.key_confidence,
            entry.loudness_lufs, entry.peak_db,
            entry.category, entry.subcategory, entry.mood,
            entry.name, entry.rating, entry.favorite, entry.color, entry.notes,
            entry.tags, entry.file_created, entry.file_modified,
            id,
        ],
    )?;

    if affected == 0 {
        return Err(DbError::NotFound(format!("Sample with id {} not found", id)));
    }
    Ok(())
}

/// Delete a sample by ID. Cascade removes sample_tags entries.
/// Returns Err(DbError::NotFound) if the sample doesn't exist.
pub fn delete_sample(conn: &Connection, id: i64) -> DbResult<()> {
    let affected = conn.execute(
        "DELETE FROM samples WHERE id = ?1",
        params![id],
    )?;

    if affected == 0 {
        return Err(DbError::NotFound(format!("Sample with id {} not found", id)));
    }
    Ok(())
}

/// Get a single sample by ID.
pub fn get_sample(conn: &Connection, id: i64) -> DbResult<SampleResult> {
    conn.query_row(
        "SELECT id, file_path, file_hash, file_size, file_format,
                sample_rate, bit_depth, channels, duration_ms,
                bpm, musical_key, loudness_lufs, peak_db,
                category, name, rating, favorite,
                tags, first_indexed, last_indexed
         FROM samples WHERE id = ?1",
        params![id],
        |row| {
            Ok(SampleResult {
                id: row.get(0)?,
                file_path: row.get(1)?,
                file_hash: row.get(2)?,
                file_size: row.get(3)?,
                file_format: row.get(4)?,
                sample_rate: row.get(5)?,
                bit_depth: row.get(6)?,
                channels: row.get(7)?,
                duration_ms: row.get(8)?,
                bpm: row.get(9)?,
                musical_key: row.get(10)?,
                loudness_lufs: row.get(11)?,
                peak_db: row.get(12)?,
                category: row.get(13)?,
                name: row.get(14)?,
                rating: row.get(15)?,
                favorite: row.get(16)?,
                tags: row.get(17)?,
                first_indexed: row.get(18)?,
                last_indexed: row.get(19)?,
            })
        },
    ).map_err(|e| match e {
        rusqlite::Error::QueryReturnedNoRows => {
            DbError::NotFound(format!("Sample with id {} not found", id))
        }
        other => DbError::Sqlite(other.to_string()),
    })
}

/// Search samples using FTS5 full-text search with optional filters.
/// Returns (results, total_count) for pagination.
pub fn search_samples(conn: &Connection, query: &SearchQuery) -> DbResult<(Vec<SampleResult>, u32)> {
    // Build dynamic query with optional filters using Value params
    let mut where_clauses: Vec<String> = Vec::new();
    let mut params_val: Vec<Value> = Vec::new();
    let mut param_idx = 1;

    // Always join with FTS if text is present
    let is_fts = !query.text.is_empty();

    if is_fts {
        where_clauses.push(format!("samples_fts MATCH ?{idx}", idx = param_idx));
        params_val.push(Value::from(query.text.clone()));
        param_idx += 1;
    }

    if let Some(ref cat) = query.category {
        where_clauses.push(format!("s.category = ?{idx}", idx = param_idx));
        params_val.push(Value::from(cat.clone()));
        param_idx += 1;
    }

    if let Some(ref key) = query.musical_key {
        where_clauses.push(format!("s.musical_key = ?{idx}", idx = param_idx));
        params_val.push(Value::from(key.clone()));
        param_idx += 1;
    }

    if let Some(bpm_min) = query.bpm_min {
        where_clauses.push(format!("s.bpm >= ?{idx}", idx = param_idx));
        params_val.push(Value::from(bpm_min));
        param_idx += 1;
    }

    if let Some(bpm_max) = query.bpm_max {
        where_clauses.push(format!("s.bpm <= ?{idx}", idx = param_idx));
        params_val.push(Value::from(bpm_max));
        param_idx += 1;
    }

    if query.favorite_only {
        where_clauses.push(format!("s.favorite = ?{idx}", idx = param_idx));
        params_val.push(Value::from(1i32));
        param_idx += 1;
    }

    if query.min_rating > 0 {
        where_clauses.push(format!("s.rating >= ?{idx}", idx = param_idx));
        params_val.push(Value::from(query.min_rating));
        param_idx += 1;
    }

    let where_sql = if where_clauses.is_empty() {
        String::new()
    } else {
        format!("WHERE {}", where_clauses.join(" AND "))
    };

    let from_clause = if is_fts {
        "FROM samples s JOIN samples_fts fts ON s.id = fts.rowid".to_string()
    } else {
        "FROM samples s".to_string()
    };

    // Build parameter slices for both count and data queries
    let limit_val = Value::from(query.limit as i64);
    let offset_val = Value::from(query.offset as i64);

    // Count query
    let count_sql = format!("SELECT COUNT(*) {} {}", from_clause, where_sql);
    let total: u32 = {
        let mut stmt = conn.prepare_cached(&count_sql)?;
        let param_refs: Vec<&dyn rusqlite::types::ToSql> =
            params_val.iter().map(|v| v as &dyn rusqlite::types::ToSql).collect();
        stmt.query_row(param_refs.as_slice(), |row| row.get(0))?
    };

    // Data query with LIMIT/OFFSET
    let order = if is_fts {
        "ORDER BY rank".to_string()
    } else {
        "ORDER BY s.last_indexed DESC".to_string()
    };

    let data_sql = format!(
        "SELECT s.id, s.file_path, s.file_hash, s.file_size, s.file_format,
                s.sample_rate, s.bit_depth, s.channels, s.duration_ms,
                s.bpm, s.musical_key, s.loudness_lufs, s.peak_db,
                s.category, s.name, s.rating, s.favorite,
                s.tags, s.first_indexed, s.last_indexed
         {} {}
         {} LIMIT ?{limit_idx} OFFSET ?{offset_idx}",
        from_clause, where_sql, order,
        limit_idx = param_idx,
        offset_idx = param_idx + 1,
    );

    // Combine all params: filters + limit + offset
    let mut all_params: Vec<Value> = params_val;
    all_params.push(limit_val);
    all_params.push(offset_val);

    let param_refs: Vec<&dyn rusqlite::types::ToSql> =
        all_params.iter().map(|v| v as &dyn rusqlite::types::ToSql).collect();

    let mut stmt = conn.prepare_cached(&data_sql)?;
    let rows = stmt.query_map(param_refs.as_slice(), |row| {
        Ok(SampleResult {
            id: row.get(0)?,
            file_path: row.get(1)?,
            file_hash: row.get(2)?,
            file_size: row.get(3)?,
            file_format: row.get(4)?,
            sample_rate: row.get(5)?,
            bit_depth: row.get(6)?,
            channels: row.get(7)?,
            duration_ms: row.get(8)?,
            bpm: row.get(9)?,
            musical_key: row.get(10)?,
            loudness_lufs: row.get(11)?,
            peak_db: row.get(12)?,
            category: row.get(13)?,
            name: row.get(14)?,
            rating: row.get(15)?,
            favorite: row.get(16)?,
            tags: row.get(17)?,
            first_indexed: row.get(18)?,
            last_indexed: row.get(19)?,
        })
    })?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row?);
    }

    Ok((results, total))
}

/// Batch insert samples in a single transaction for indexing speed.
/// Returns the number of successfully inserted samples.
/// On constraint violations, the entire batch is rolled back (all-or-nothing).
pub fn batch_insert_samples(conn: &Connection, entries: &[SampleEntry]) -> DbResult<u32> {
    conn.execute("BEGIN", [])?;

    let mut count = 0u32;
    let result = (|| -> DbResult<()> {
        let mut stmt = conn.prepare_cached(
            "INSERT INTO samples (
                file_path, file_hash, file_size, file_format,
                sample_rate, bit_depth, channels, duration_ms,
                bpm, bpm_confidence, musical_key, key_confidence,
                loudness_lufs, peak_db,
                category, subcategory, mood,
                name, rating, favorite, color, notes,
                tags, file_created, file_modified
            ) VALUES (
                ?1, ?2, ?3, ?4,
                ?5, ?6, ?7, ?8,
                ?9, ?10, ?11, ?12,
                ?13, ?14,
                ?15, ?16, ?17,
                ?18, ?19, ?20, ?21, ?22,
                ?23, ?24, ?25
            )"
        )?;

        for entry in entries {
            stmt.execute(params![
                entry.file_path,
                entry.file_hash,
                entry.file_size,
                entry.file_format,
                entry.sample_rate,
                entry.bit_depth,
                entry.channels,
                entry.duration_ms,
                entry.bpm,
                entry.bpm_confidence,
                entry.musical_key,
                entry.key_confidence,
                entry.loudness_lufs,
                entry.peak_db,
                entry.category,
                entry.subcategory,
                entry.mood,
                entry.name,
                entry.rating,
                entry.favorite,
                entry.color,
                entry.notes,
                entry.tags,
                entry.file_created,
                entry.file_modified,
            ])?;
            count += 1;
        }
        Ok(())
    })();

    match result {
        Ok(()) => {
            conn.execute("COMMIT", [])?;
            Ok(count)
        }
        Err(e) => {
            conn.execute("ROLLBACK", [])?;
            Err(e)
        }
    }
}

/// Get the total number of samples in the database.
pub fn sample_count(conn: &Connection) -> DbResult<u32> {
    let count: u32 = conn.query_row(
        "SELECT COUNT(*) FROM samples",
        [],
        |row| row.get(0),
    )?;
    Ok(count)
}

// ── Tags CRUD ───────────────────────────────────────────────

/// Create a new tag. Returns the new tag ID.
/// Returns Err(DbError::Duplicate) if the tag name already exists.
pub fn add_tag(conn: &Connection, name: &str, color: Option<&str>) -> DbResult<i64> {
    let exists: bool = conn.query_row(
        "SELECT COUNT(*) > 0 FROM tags WHERE name = ?1",
        params![name],
        |row| row.get(0),
    )?;

    if exists {
        return Err(DbError::Duplicate(format!("Tag '{}' already exists", name)));
    }

    conn.execute(
        "INSERT INTO tags (name, color) VALUES (?1, ?2)",
        params![name, color],
    )?;

    Ok(conn.last_insert_rowid())
}

/// Assign a tag to a sample. Idempotent — duplicate assignments are ignored.
pub fn tag_sample(conn: &Connection, sample_id: i64, tag_id: i64) -> DbResult<()> {
    // Verify sample and tag exist
    verify_sample_exists(conn, sample_id)?;
    verify_tag_exists(conn, tag_id)?;

    conn.execute(
        "INSERT OR IGNORE INTO sample_tags (sample_id, tag_id) VALUES (?1, ?2)",
        params![sample_id, tag_id],
    )?;

    // Update denormalized tags field for FTS5
    update_sample_tags_field(conn, sample_id)?;

    Ok(())
}

/// Remove a tag from a sample.
pub fn untag_sample(conn: &Connection, sample_id: i64, tag_id: i64) -> DbResult<()> {
    conn.execute(
        "DELETE FROM sample_tags WHERE sample_id = ?1 AND tag_id = ?2",
        params![sample_id, tag_id],
    )?;

    // Update denormalized tags field for FTS5
    update_sample_tags_field(conn, sample_id)?;

    Ok(())
}

/// Get all tags assigned to a sample.
pub fn get_sample_tags(conn: &Connection, sample_id: i64) -> DbResult<Vec<Tag>> {
    let mut stmt = conn.prepare_cached(
        "SELECT t.id, t.name, t.color
         FROM tags t
         JOIN sample_tags st ON t.id = st.tag_id
         WHERE st.sample_id = ?1
         ORDER BY t.name"
    )?;

    let rows = stmt.query_map(params![sample_id], |row| {
        Ok(Tag {
            id: row.get(0)?,
            name: row.get(1)?,
            color: row.get(2)?,
        })
    })?;

    let mut tags = Vec::new();
    for row in rows {
        tags.push(row?);
    }
    Ok(tags)
}

/// Get all tags in the database.
pub fn get_all_tags(conn: &Connection) -> DbResult<Vec<Tag>> {
    let mut stmt = conn.prepare_cached(
        "SELECT id, name, color FROM tags ORDER BY name"
    )?;

    let rows = stmt.query_map([], |row| {
        Ok(Tag {
            id: row.get(0)?,
            name: row.get(1)?,
            color: row.get(2)?,
        })
    })?;

    let mut tags = Vec::new();
    for row in rows {
        tags.push(row?);
    }
    Ok(tags)
}

/// Search samples by tag name(s). Returns samples that have ALL specified tags.
pub fn search_by_tags(conn: &Connection, tag_names: &[String], limit: u32, offset: u32) -> DbResult<(Vec<SampleResult>, u32)> {
    if tag_names.is_empty() {
        return Ok((Vec::new(), 0));
    }

    let placeholders: Vec<String> = tag_names.iter().enumerate()
        .map(|(i, _)| format!("?{}", i + 1))
        .collect();
    let placeholders_str = placeholders.join(", ");

    let having_param = tag_names.len() + 1;
    let limit_param = tag_names.len() + 2;
    let offset_param = tag_names.len() + 3;

    // Build params as Vec<Value>
    let mut params_val: Vec<Value> = tag_names.iter()
        .map(|n| Value::from(n.clone()))
        .collect();
    params_val.push(Value::from(tag_names.len() as i64));
    params_val.push(Value::from(limit as i64));
    params_val.push(Value::from(offset as i64));

    // Count query (no LIMIT/OFFSET)
    let count_sql = format!(
        "SELECT COUNT(*) FROM samples s
         JOIN sample_tags st ON s.id = st.sample_id
         JOIN tags t ON st.tag_id = t.id
         WHERE t.name IN ({})
         GROUP BY s.id
         HAVING COUNT(DISTINCT t.name) = ?{}",
        placeholders_str,
        having_param,
    );

    // For count query, we only need up to having_param
    let count_vals: Vec<Value> = params_val[..=tag_names.len()].to_vec();
    let count_refs: Vec<&dyn rusqlite::types::ToSql> =
        count_vals.iter().map(|v| v as &dyn rusqlite::types::ToSql).collect();

    let total: u32 = {
        let mut stmt = conn.prepare_cached(&count_sql)?;
        let rows = stmt.query_map(count_refs.as_slice(), |row| {
            row.get::<_, u32>(0)
        })?;
        let mut total_count = 0u32;
        for _ in rows {
            total_count += 1;
        }
        total_count
    };

    // Data query
    let data_sql = format!(
        "SELECT s.id, s.file_path, s.file_hash, s.file_size, s.file_format,
                s.sample_rate, s.bit_depth, s.channels, s.duration_ms,
                s.bpm, s.musical_key, s.loudness_lufs, s.peak_db,
                s.category, s.name, s.rating, s.favorite,
                s.tags, s.first_indexed, s.last_indexed
         FROM samples s
         JOIN sample_tags st ON s.id = st.sample_id
         JOIN tags t ON st.tag_id = t.id
         WHERE t.name IN ({})
         GROUP BY s.id
         HAVING COUNT(DISTINCT t.name) = ?{}
         ORDER BY s.rating DESC
         LIMIT ?{} OFFSET ?{}",
        placeholders_str,
        having_param,
        limit_param,
        offset_param,
    );

    let data_refs: Vec<&dyn rusqlite::types::ToSql> =
        params_val.iter().map(|v| v as &dyn rusqlite::types::ToSql).collect();

    let mut stmt = conn.prepare_cached(&data_sql)?;
    let rows = stmt.query_map(data_refs.as_slice(), |row| {
        Ok(SampleResult {
            id: row.get(0)?,
            file_path: row.get(1)?,
            file_hash: row.get(2)?,
            file_size: row.get(3)?,
            file_format: row.get(4)?,
            sample_rate: row.get(5)?,
            bit_depth: row.get(6)?,
            channels: row.get(7)?,
            duration_ms: row.get(8)?,
            bpm: row.get(9)?,
            musical_key: row.get(10)?,
            loudness_lufs: row.get(11)?,
            peak_db: row.get(12)?,
            category: row.get(13)?,
            name: row.get(14)?,
            rating: row.get(15)?,
            favorite: row.get(16)?,
            tags: row.get(17)?,
            first_indexed: row.get(18)?,
            last_indexed: row.get(19)?,
        })
    })?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row?);
    }

    Ok((results, total))
}

// ── Watched Folders ─────────────────────────────────────────

/// Add a watched folder path.
pub fn add_watched_folder(conn: &Connection, path: &str, recursive: bool) -> DbResult<()> {
    conn.execute(
        "INSERT OR IGNORE INTO watched_folders (path, recursive) VALUES (?1, ?2)",
        params![path, if recursive { 1 } else { 0 }],
    )?;
    Ok(())
}

/// Remove a watched folder by path.
pub fn remove_watched_folder(conn: &Connection, path: &str) -> DbResult<()> {
    conn.execute(
        "DELETE FROM watched_folders WHERE path = ?1",
        params![path],
    )?;
    Ok(())
}

/// Get all watched folder paths.
pub fn get_watched_folders(conn: &Connection) -> DbResult<Vec<(String, bool)>> {
    let mut stmt = conn.prepare_cached(
        "SELECT path, recursive FROM watched_folders ORDER BY path"
    )?;

    let rows = stmt.query_map([], |row| {
        Ok((
            row.get::<_, String>(0)?,
            row.get::<_, i32>(1)? != 0,
        ))
    })?;

    let mut folders = Vec::new();
    for row in rows {
        folders.push(row?);
    }
    Ok(folders)
}

// ── Maintenance ─────────────────────────────────────────────

/// Run VACUUM to reclaim space.
pub fn vacuum(conn: &Connection) -> DbResult<()> {
    conn.execute_batch("VACUUM")
        .map_err(|e| DbError::Sqlite(format!("VACUUM failed: {}", e)))
}

/// Run integrity check. Returns list of error messages (empty if OK).
pub fn integrity_check(conn: &Connection) -> DbResult<Vec<String>> {
    let mut stmt = conn.prepare_cached("PRAGMA integrity_check")?;
    let rows = stmt.query_map([], |row| {
        row.get::<_, String>(0)
    })?;

    let mut errors = Vec::new();
    for row in rows {
        let msg = row?;
        if msg != "ok" {
            errors.push(msg);
        }
    }
    Ok(errors)
}

/// Run ANALYZE to update query planner statistics.
pub fn analyze(conn: &Connection) -> DbResult<()> {
    conn.execute_batch("ANALYZE")
        .map_err(|e| DbError::Sqlite(format!("ANALYZE failed: {}", e)))
}

/// Remove sample entries whose files no longer exist on disk.
/// Returns the number of removed samples.
pub fn remove_missing_files(_conn: &Connection) -> DbResult<u32> {
    // Temporary: until we have filesystem integration with file existence list,
    // this is a no-op. Real implementation will diff DB paths against FS scan results.
    Ok(0)
}

/// Create a backup of the database to the specified path.
pub fn backup_database_to(conn: &Connection, dest_path: &str) -> DbResult<()> {
    conn.backup(rusqlite::DatabaseName::Main, dest_path, None)
        .map_err(|e| DbError::Sqlite(format!("Backup failed: {}", e)))
}

/// Get current schema version from schema_version table.
pub fn get_schema_version(conn: &Connection) -> DbResult<u32> {
    conn.query_row(
        "SELECT COALESCE(MAX(version), 0) FROM schema_version",
        [],
        |row| row.get(0),
    ).map_err(|e| DbError::Migration(format!("Failed to get schema version: {}", e)))
}

// ── Internal helpers ────────────────────────────────────────

fn verify_sample_exists(conn: &Connection, sample_id: i64) -> DbResult<()> {
    conn.query_row(
        "SELECT 1 FROM samples WHERE id = ?1",
        params![sample_id],
        |_| Ok(()),
    ).map_err(|_| DbError::NotFound(format!("Sample with id {} not found", sample_id)))
}

fn verify_tag_exists(conn: &Connection, tag_id: i64) -> DbResult<()> {
    conn.query_row(
        "SELECT 1 FROM tags WHERE id = ?1",
        params![tag_id],
        |_| Ok(()),
    ).map_err(|_| DbError::NotFound(format!("Tag with id {} not found", tag_id)))
}

/// Update the denormalized `tags` field on a sample by joining sample_tags.
fn update_sample_tags_field(conn: &Connection, sample_id: i64) -> DbResult<()> {
    // Collect tag names for this sample as a space-separated string
    let mut stmt = conn.prepare_cached(
        "SELECT group_concat(t.name, ' ')
         FROM tags t
         JOIN sample_tags st ON t.id = st.tag_id
         WHERE st.sample_id = ?1"
    )?;

    let tags_str: Option<String> = stmt.query_row(params![sample_id], |row| {
        row.get(0)
    }).ok();

    let tags_value = tags_str.unwrap_or_default();

    conn.execute(
        "UPDATE samples SET tags = ?1 WHERE id = ?2",
        params![if tags_value.is_empty() { None::<String> } else { Some(tags_value) }, sample_id],
    )?;

    Ok(())
}

// ── Tests ───────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use crate::migration;
    use rusqlite::Connection;

    fn setup_db() -> Connection {
        let conn = Connection::open_in_memory().unwrap();
        conn.execute_batch(
            "PRAGMA journal_mode=WAL;
             PRAGMA synchronous=NORMAL;
             PRAGMA foreign_keys=ON;"
        ).unwrap();
        migration::run_migrations(&conn).unwrap();
        conn
    }

    fn make_sample(path: &str) -> SampleEntry {
        SampleEntry {
            file_path: path.to_string(),
            file_hash: "abc123def456".to_string(),
            file_size: 4096,
            file_format: "wav".to_string(),
            sample_rate: Some(44100),
            bit_depth: Some(24),
            channels: Some(2),
            duration_ms: Some(5000),
            bpm: Some(120.0),
            bpm_confidence: Some(0.9),
            musical_key: Some("Cm".to_string()),
            key_confidence: Some(0.8),
            loudness_lufs: Some(-14.5),
            peak_db: Some(-3.0),
            category: Some("kick".to_string()),
            subcategory: Some("acoustic".to_string()),
            mood: Some("dark".to_string()),
            name: Some("Test Kick".to_string()),
            rating: Some(4),
            favorite: Some(1),
            color: Some("#FF0000".to_string()),
            notes: Some("Great kick sample".to_string()),
            tags: Some("kick acoustic".to_string()),
            file_created: Some("2024-01-01T00:00:00".to_string()),
            file_modified: Some("2024-06-01T00:00:00".to_string()),
        }
    }

    // ── RED: Tests written before queries implementation ───────

    #[test]
    fn test_insert_sample_basic() {
        let conn = setup_db();
        let entry = make_sample("/test/basic.wav");
        let id = insert_sample(&conn, &entry).unwrap();
        assert!(id > 0, "Insert should return a positive ID");
    }

    #[test]
    fn test_insert_sample_duplicate() {
        let conn = setup_db();
        let entry = make_sample("/test/dup.wav");
        insert_sample(&conn, &entry).unwrap();

        let result = insert_sample(&conn, &entry);
        assert!(result.is_err(), "Duplicate insert should fail");
        match result {
            Err(DbError::Duplicate(_)) => {} // Expected
            _ => panic!("Should be Duplicate error"),
        }
    }

    #[test]
    fn test_get_sample() {
        let conn = setup_db();
        let entry = make_sample("/test/get.wav");
        let id = insert_sample(&conn, &entry).unwrap();

        let fetched = get_sample(&conn, id).unwrap();
        assert_eq!(fetched.file_path, "/test/get.wav");
        assert_eq!(fetched.name.unwrap(), "Test Kick");
        assert_eq!(fetched.bpm.unwrap(), 120.0);
        assert_eq!(fetched.rating, 4);
        assert_eq!(fetched.favorite, 1);
    }

    #[test]
    fn test_get_sample_not_found() {
        let conn = setup_db();
        let result = get_sample(&conn, 999);
        assert!(result.is_err());
        match result {
            Err(DbError::NotFound(_)) => {} // Expected
            _ => panic!("Should be NotFound error"),
        }
    }

    #[test]
    fn test_update_sample() {
        let conn = setup_db();
        let entry = make_sample("/test/update.wav");
        let id = insert_sample(&conn, &entry).unwrap();

        let mut updated = entry;
        updated.name = Some("Updated Name".to_string());
        updated.bpm = Some(140.0);
        update_sample(&conn, id, &updated).unwrap();

        let fetched = get_sample(&conn, id).unwrap();
        assert_eq!(fetched.name.unwrap(), "Updated Name");
        assert_eq!(fetched.bpm.unwrap(), 140.0);
    }

    #[test]
    fn test_update_sample_not_found() {
        let conn = setup_db();
        let entry = make_sample("/test/nonexistent.wav");
        let result = update_sample(&conn, 999, &entry);
        assert!(result.is_err());
        match result {
            Err(DbError::NotFound(_)) => {} // Expected
            _ => panic!("Should be NotFound error"),
        }
    }

    #[test]
    fn test_delete_sample() {
        let conn = setup_db();
        let entry = make_sample("/test/delete.wav");
        let id = insert_sample(&conn, &entry).unwrap();

        delete_sample(&conn, id).unwrap();

        let result = get_sample(&conn, id);
        assert!(result.is_err(), "Deleted sample should not be found");
    }

    #[test]
    fn test_delete_sample_not_found() {
        let conn = setup_db();
        let result = delete_sample(&conn, 999);
        assert!(result.is_err());
    }

    #[test]
    fn test_batch_insert() {
        let conn = setup_db();
        let mut entries = Vec::new();
        for i in 0..10 {
            let mut e = make_sample(&format!("/test/batch_{}.wav", i));
            e.file_hash = format!("hash_{}", i);
            entries.push(e);
        }

        let count = batch_insert_samples(&conn, &entries).unwrap();
        assert_eq!(count, 10, "All 10 samples should be inserted");

        let total = sample_count(&conn).unwrap();
        assert_eq!(total, 10);
    }

    #[test]
    fn test_batch_insert_rollback_on_error() {
        let conn = setup_db();
        let mut entries = Vec::new();

        // First entry is valid
        entries.push(make_sample("/test/batch_valid.wav"));

        // Second entry has a duplicate path (will be inserted later)
        let dup = make_sample("/test/batch_valid.wav");
        entries.push(dup);

        let result = batch_insert_samples(&conn, &entries);
        assert!(result.is_err(), "Batch with duplicate should fail");

        // Transaction should have rolled back
        let total = sample_count(&conn).unwrap();
        assert_eq!(total, 0, "All inserts should be rolled back on error");
    }

    #[test]
    fn test_sample_count() {
        let conn = setup_db();
        assert_eq!(sample_count(&conn).unwrap(), 0);

        for i in 0..5 {
            let e = make_sample(&format!("/test/count_{}.wav", i));
            insert_sample(&conn, &e).unwrap();
        }

        assert_eq!(sample_count(&conn).unwrap(), 5);
    }

    #[test]
    fn test_search_samples_basic() {
        let conn = setup_db();

        let e1 = make_sample("/test/kick1.wav");
        insert_sample(&conn, &e1).unwrap();

        let mut e2 = make_sample("/test/snare1.wav");
        e2.category = Some("snare".to_string());
        e2.name = Some("Snare Hit".to_string());
        e2.tags = Some("snare acoustic".to_string());
        insert_sample(&conn, &e2).unwrap();

        // Search by name
        let query = SearchQuery {
            text: "Snare".to_string(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            limit: 10,
            offset: 0,
        };
        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 1, "Should find 1 snare sample");
        assert_eq!(results[0].name.clone().unwrap(), "Snare Hit");
    }

    #[test]
    fn test_search_samples_empty_text() {
        let conn = setup_db();
        for i in 0..3 {
            let e = make_sample(&format!("/test/all_{}.wav", i));
            insert_sample(&conn, &e).unwrap();
        }

        let query = SearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            limit: 10,
            offset: 0,
        };
        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 3, "Empty search should return all samples");
        assert_eq!(results.len(), 3);
    }

    #[test]
    fn test_search_samples_with_filters() {
        let conn = setup_db();

        let mut e1 = make_sample("/test/filter_a.wav");
        e1.bpm = Some(120.0);
        e1.musical_key = Some("Cm".to_string());
        e1.rating = Some(5);
        e1.favorite = Some(1);
        insert_sample(&conn, &e1).unwrap();

        let mut e2 = make_sample("/test/filter_b.wav");
        e2.bpm = Some(140.0);
        e2.musical_key = Some("F".to_string());
        e2.rating = Some(2);
        e2.favorite = Some(0);
        insert_sample(&conn, &e2).unwrap();

        // Filter by BPM range
        let query = SearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: Some(130.0),
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            limit: 10,
            offset: 0,
        };
        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 1, "Only one sample has BPM >= 130");
        assert_eq!(results[0].file_path, "/test/filter_b.wav");

        // Filter by favorite
        let (_fav_results, fav_total) = search_samples(&conn, &SearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: true,
            min_rating: 0,
            limit: 10,
            offset: 0,
        }).unwrap();
        assert_eq!(fav_total, 1, "Only one sample is favorited");
    }

    // ── Tags ─────────────────────────────────────────────────

    #[test]
    fn test_add_tag() {
        let conn = setup_db();
        let id = add_tag(&conn, "dark", Some("#000000")).unwrap();
        assert!(id > 0);
    }

    #[test]
    fn test_add_tag_duplicate() {
        let conn = setup_db();
        add_tag(&conn, "dark", None).unwrap();
        let result = add_tag(&conn, "dark", None);
        assert!(result.is_err());
        match result {
            Err(DbError::Duplicate(_)) => {}
            _ => panic!("Expected Duplicate error"),
        }
    }

    #[test]
    fn test_tag_sample_roundtrip() {
        let conn = setup_db();

        let entry = make_sample("/test/tagged.wav");
        let sample_id = insert_sample(&conn, &entry).unwrap();
        let tag_id = add_tag(&conn, "dark", None).unwrap();

        tag_sample(&conn, sample_id, tag_id).unwrap();

        let tags = get_sample_tags(&conn, sample_id).unwrap();
        assert_eq!(tags.len(), 1);
        assert_eq!(tags[0].name, "dark");
    }

    #[test]
    fn test_untag_sample() {
        let conn = setup_db();

        let sample_id = insert_sample(&conn, &make_sample("/test/untag.wav")).unwrap();
        let tag_id = add_tag(&conn, "bright", Some("#FFFFFF")).unwrap();

        tag_sample(&conn, sample_id, tag_id).unwrap();
        assert_eq!(get_sample_tags(&conn, sample_id).unwrap().len(), 1);

        untag_sample(&conn, sample_id, tag_id).unwrap();
        assert_eq!(get_sample_tags(&conn, sample_id).unwrap().len(), 0);
    }

    #[test]
    fn test_get_all_tags() {
        let conn = setup_db();
        add_tag(&conn, "dark", None).unwrap();
        add_tag(&conn, "bright", None).unwrap();
        add_tag(&conn, "ambient", None).unwrap();

        let tags = get_all_tags(&conn).unwrap();
        assert_eq!(tags.len(), 3);
    }

    #[test]
    fn test_search_by_tags() {
        let conn = setup_db();

        let s1 = insert_sample(&conn, &make_sample("/test/tag_a.wav")).unwrap();
        let s2 = insert_sample(&conn, &{
            let mut e = make_sample("/test/tag_b.wav");
            e.file_hash = "hash_tag_b".to_string();
            e
        }).unwrap();

        let tag_dark = add_tag(&conn, "dark", None).unwrap();
        let tag_bass = add_tag(&conn, "bass", None).unwrap();

        tag_sample(&conn, s1, tag_dark).unwrap();
        tag_sample(&conn, s1, tag_bass).unwrap();
        tag_sample(&conn, s2, tag_dark).unwrap();

        // Find all dark samples
        let (_results, total) = search_by_tags(&conn, &["dark".to_string()], 10, 0).unwrap();
        assert_eq!(total, 2, "Both samples have 'dark' tag");

        // Find samples with both dark AND bass
        let (results, _) = search_by_tags(&conn, &["dark".to_string(), "bass".to_string()], 10, 0).unwrap();
        assert_eq!(results.len(), 1, "Only sample 1 has both tags");
    }

    // ── Watched Folders ──────────────────────────────────────

    #[test]
    fn test_watched_folders() {
        let conn = setup_db();
        add_watched_folder(&conn, "/home/user/samples", true).unwrap();
        add_watched_folder(&conn, "/home/user/loops", false).unwrap();

        let folders = get_watched_folders(&conn).unwrap();
        assert_eq!(folders.len(), 2);

        remove_watched_folder(&conn, "/home/user/loops").unwrap();
        let folders = get_watched_folders(&conn).unwrap();
        assert_eq!(folders.len(), 1);
    }

    // ── Maintenance ──────────────────────────────────────────

    #[test]
    fn test_integrity_check_clean() {
        let conn = setup_db();
        let errors = integrity_check(&conn).unwrap();
        assert!(errors.is_empty(), "Fresh DB should pass integrity check");
    }

    #[test]
    fn test_schema_version() {
        let conn = setup_db();
        let version = get_schema_version(&conn).unwrap();
        assert_eq!(version, 1);
    }

    #[test]
    fn test_vacuum() {
        let conn = setup_db();
        vacuum(&conn).unwrap();
        // Vacuum on in-memory DB should succeed
    }

    #[test]
    fn test_analyze() {
        let conn = setup_db();
        analyze(&conn).unwrap();
    }

    // ── Delete cascade with tags ─────────────────────────────

    #[test]
    fn test_delete_sample_cascades_tags() {
        let conn = setup_db();

        let sample_id = insert_sample(&conn, &make_sample("/test/cascade.wav")).unwrap();
        let tag_id = add_tag(&conn, "test-tag", None).unwrap();
        tag_sample(&conn, sample_id, tag_id).unwrap();

        // Verify tag exists
        assert_eq!(get_sample_tags(&conn, sample_id).unwrap().len(), 1);

        // Delete sample
        delete_sample(&conn, sample_id).unwrap();

        // Tag assignment should be cascade-deleted
        assert_eq!(get_sample_tags(&conn, sample_id).unwrap().len(), 0);

        // Tag itself should still exist
        let all_tags = get_all_tags(&conn).unwrap();
        assert_eq!(all_tags.len(), 1);
    }
}
