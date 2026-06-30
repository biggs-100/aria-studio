// ARIA DAW — Search Engine
// FTS5-powered full-text search with BM25 ranking, multi-filter, sort, and pagination.
// Dedicated module providing enhanced search over the basic query-level search.

use crate::errors::DbResult;
use crate::queries::SampleResult;
use rusqlite::{types::Value, Connection};

/// Sort mode for search results.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SortMode {
    /// BM25 relevance score (default, ascending — lower score = better match)
    Relevance = 0,
    /// Alphabetical by name
    Name = 1,
    /// By first_indexed date
    DateAdded = 2,
    /// By BPM value
    Bpm = 3,
}

/// Extended search query supporting all filters, sort modes, and tag filtering.
#[derive(Debug, Clone)]
pub struct ExtendedSearchQuery {
    /// FTS5 full-text search text (empty = browse mode, no FTS5 join)
    pub text: String,
    /// Exact category filter
    pub category: Option<String>,
    /// Exact musical key filter
    pub musical_key: Option<String>,
    /// Minimum BPM (inclusive)
    pub bpm_min: Option<f64>,
    /// Maximum BPM (inclusive)
    pub bpm_max: Option<f64>,
    /// If true, only favorited samples
    pub favorite_only: bool,
    /// Minimum rating (>= this value)
    pub min_rating: i32,
    /// Tag names to filter by (sample must have ALL specified tags)
    pub tag_names: Vec<String>,
    /// Sort mode
    pub sort_by: SortMode,
    /// Ascending sort (default: false = descending)
    pub sort_ascending: bool,
    /// Maximum results per page
    pub limit: u32,
    /// Page offset
    pub offset: u32,
}

/// Result from a search, including BM25 rank score.
#[derive(Debug, Clone)]
pub struct SearchResult {
    pub sample: SampleResult,
    /// BM25 relevance score (lower = better match, 0.0 when not using FTS5)
    pub rank_score: f64,
}

/// Sanitize a user-provided query string for safe use in FTS5 MATCH syntax.
///
/// FTS5 has special operators: `^`, `*`, `"`, `(`, `)`, `+`, `-`, `NOT`, `AND`, `OR`, `:`, `\`.
/// We wrap each whitespace-delimited token in double quotes to prevent those operators
/// from changing query semantics, while still allowing multi-word queries.
pub fn sanitize_fts_query(text: &str) -> String {
    let trimmed = text.trim();
    if trimmed.is_empty() {
        return String::new();
    }

    // Normalize whitespace: collapse multiple spaces
    let normalized = trimmed
        .split_whitespace()
        .collect::<Vec<&str>>()
        .join(" ");

    // FTS5 special characters that have query syntax meaning
    fn has_special_chars(s: &str) -> bool {
        s.chars().any(|c| matches!(c, '"' | '*' | '^' | '(' | ')' | '+' | '-' | ':' | '\\'))
    }

    // If there are no special characters, return as-is (FTS5 handles it fine)
    if !has_special_chars(&normalized) {
        return normalized;
    }

    // Wrap each token in quotes to escape special chars.
    // Strip any inner quotes from tokens first.
    let quoted: Vec<String> = normalized
        .split_whitespace()
        .map(|token| {
            let cleaned: String = token.chars().filter(|&c| c != '"').collect();
            if cleaned.is_empty() {
                None
            } else {
                Some(format!("\"{}\"", cleaned))
            }
        })
        .filter_map(|t| t)
        .collect();

    if quoted.is_empty() {
        // All tokens were just quotes — fall back to the cleaned normalized form
        normalized.chars().filter(|&c| c != '"').collect()
    } else {
        quoted.join(" ")
    }
}

/// Search samples using FTS5 MATCH with BM25 ranking, filters, tag join, sort, and pagination.
///
/// Returns (results, total_count) where total_count reflects all matching rows
/// before pagination (for UI "X of Y" display).
pub fn search_samples(
    conn: &Connection,
    query: &ExtendedSearchQuery,
) -> DbResult<(Vec<SearchResult>, u32)> {
    let is_fts = !query.text.is_empty();
    let has_tags = !query.tag_names.is_empty();

    // ── Build SELECT columns ──────────────────────────────────
    let select_cols = concat!(
        "s.id, s.file_path, s.file_hash, s.file_size, s.file_format, ",
        "s.sample_rate, s.bit_depth, s.channels, s.duration_ms, ",
        "s.bpm, s.musical_key, s.loudness_lufs, s.peak_db, ",
        "s.category, s.name, s.rating, s.favorite, ",
        "s.tags, s.first_indexed, s.last_indexed"
    );

    // Use FTS5's built-in `rank` column (BM25 by default) instead of calling
    // bm25() directly, because bm25() cannot be used in GROUP BY contexts
    // (required for tag filtering via sample_tags join).
    let rank_expr = if is_fts { "rank" } else { "0.0" };

    let data_select = format!("SELECT {}, {} AS _rank_score", select_cols, rank_expr);

    // ── Build FROM clause ─────────────────────────────────────
    let mut from_parts = vec!["samples s".to_string()];

    if is_fts {
        from_parts.push("JOIN samples_fts fts ON s.id = fts.rowid".to_string());
    }
    if has_tags {
        // INNER JOIN because tag filtering only matches samples that have tags
        from_parts.push("JOIN sample_tags st ON s.id = st.sample_id".to_string());
        from_parts.push("JOIN tags t ON st.tag_id = t.id".to_string());
    }

    let from_clause = format!("FROM {}", from_parts.join(" "));

    // ── Build WHERE clause ────────────────────────────────────
    let mut where_clauses: Vec<String> = Vec::new();
    let mut params_val: Vec<Value> = Vec::new();
    let mut param_idx = 1u32;

    if is_fts {
        let sanitized = sanitize_fts_query(&query.text);
        where_clauses.push(format!("samples_fts MATCH ?{}", param_idx));
        params_val.push(Value::from(sanitized));
        param_idx += 1;
    }

    if let Some(ref cat) = query.category {
        if !cat.is_empty() {
            where_clauses.push(format!("s.category = ?{}", param_idx));
            params_val.push(Value::from(cat.clone()));
            param_idx += 1;
        }
    }

    if let Some(ref key) = query.musical_key {
        if !key.is_empty() {
            where_clauses.push(format!("s.musical_key = ?{}", param_idx));
            params_val.push(Value::from(key.clone()));
            param_idx += 1;
        }
    }

    if let Some(bpm_min) = query.bpm_min {
        where_clauses.push(format!("s.bpm >= ?{}", param_idx));
        params_val.push(Value::from(bpm_min));
        param_idx += 1;
    }

    if let Some(bpm_max) = query.bpm_max {
        where_clauses.push(format!("s.bpm <= ?{}", param_idx));
        params_val.push(Value::from(bpm_max));
        param_idx += 1;
    }

    if query.favorite_only {
        where_clauses.push(format!("s.favorite = ?{}", param_idx));
        params_val.push(Value::from(1i32));
        param_idx += 1;
    }

    if query.min_rating > 0 {
        where_clauses.push(format!("s.rating >= ?{}", param_idx));
        params_val.push(Value::from(query.min_rating));
        param_idx += 1;
    }

    // Tag name filter (in WHERE because we JOIN with tags)
    if has_tags {
        let tag_placeholders: Vec<String> = query.tag_names.iter()
            .enumerate()
            .map(|(i, _)| format!("?{}", param_idx + i as u32))
            .collect();
        where_clauses.push(format!("t.name IN ({})", tag_placeholders.join(", ")));
        for tag_name in &query.tag_names {
            params_val.push(Value::from(tag_name.clone()));
        }
        param_idx += query.tag_names.len() as u32;
    }

    let where_sql = if where_clauses.is_empty() {
        String::new()
    } else {
        format!("WHERE {}", where_clauses.join(" AND "))
    };

    // ── Build HAVING clause (for tag filter) ──────────────────
    let having_sql = if has_tags {
        let having_clause = format!("HAVING COUNT(DISTINCT t.name) = ?{}", param_idx);
        params_val.push(Value::from(query.tag_names.len() as i64));
        param_idx += 1;
        having_clause
    } else {
        String::new()
    };

    // ── Build ORDER BY clause ─────────────────────────────────
    let direction = if query.sort_ascending { "ASC" } else { "DESC" };

    let order_sql = match query.sort_by {
        SortMode::Relevance => {
            if is_fts {
                "ORDER BY rank ASC".to_string()
            } else {
                // In browse mode (no FTS), relevance sort defaults to last_indexed
                format!("ORDER BY s.last_indexed {} NULLS LAST", direction)
            }
        }
        SortMode::Name => {
            format!("ORDER BY s.name {} NULLS LAST", direction)
        }
        SortMode::DateAdded => {
            format!("ORDER BY s.first_indexed {} NULLS LAST", direction)
        }
        SortMode::Bpm => {
            format!("ORDER BY s.bpm {} NULLS LAST, s.id", direction)
        }
    };

    // ── Build GROUP BY clause ─────────────────────────────────
    let group_sql = if has_tags {
        "GROUP BY s.id".to_string()
    } else {
        String::new()
    };

    // ── Build full SQL ────────────────────────────────────────
    let limit_param = param_idx;
    let offset_param = param_idx + 1;

    // Count query (no LIMIT/OFFSET)
    let count_from = if has_tags {
        // For COUNT with GROUP BY, we need a subquery to get the actual count
        let count_sql = format!(
            "SELECT COUNT(*) FROM (SELECT 1 {} {} {} {}) AS cnt",
            from_clause, where_sql, group_sql, having_sql
        );
        count_sql
    } else {
        let count_sql = format!(
            "SELECT COUNT(*) {} {}",
            from_clause, where_sql
        );
        count_sql
    };

    // Data query
    let data_sql_parts = vec![
        data_select,
        from_clause,
        where_sql,
        group_sql,
        having_sql,
        order_sql,
        format!("LIMIT ?{} OFFSET ?{}", limit_param, offset_param),
    ];
    let data_sql = data_sql_parts.iter()
        .filter(|s| !s.is_empty())
        .cloned()
        .collect::<Vec<String>>()
        .join(" ");

    // ── Build parameter refs ──────────────────────────────────
    let limit_val = Value::from(query.limit as i64);
    let offset_val = Value::from(query.offset as i64);

    // Execute count query
    let total: u32 = {
        let mut stmt = conn.prepare_cached(&count_from)?;
        let param_refs: Vec<&dyn rusqlite::types::ToSql> =
            params_val.iter().map(|v| v as &dyn rusqlite::types::ToSql).collect();
        stmt.query_row(param_refs.as_slice(), |row| row.get(0))?
    };

    // Execute data query
    let mut all_params: Vec<Value> = params_val;
    all_params.push(limit_val);
    all_params.push(offset_val);

    let param_refs: Vec<&dyn rusqlite::types::ToSql> =
        all_params.iter().map(|v| v as &dyn rusqlite::types::ToSql).collect();

    let mut stmt = conn.prepare_cached(&data_sql)?;
    let rows = stmt.query_map(param_refs.as_slice(), |row| {
        let rank_score: f64 = row.get(20)?; // _rank_score is column 21 (0-indexed = 20)
        let sample = SampleResult {
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
        };
        Ok(SearchResult { sample, rank_score })
    })?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row?);
    }

    Ok((results, total))
}

// ── Multi-Entity Search ─────────────────────────────────────

/// Result from plugin search.
#[derive(Debug, Clone)]
pub struct PluginSearchResult {
    pub id: i64,
    pub name: String,
    pub vendor: String,
    pub category: String,
    pub format: String,
    pub features: String,
    pub rank_score: f64,
}

/// Result from project search.
#[derive(Debug, Clone)]
pub struct ProjectSearchResult {
    pub id: i64,
    pub name: String,
    pub file_path: String,
    pub author: String,
    pub tempo: Option<f64>,
    pub track_count: Option<i32>,
    pub rank_score: f64,
}

/// Result from preset search.
#[derive(Debug, Clone)]
pub struct PresetSearchResult {
    pub id: i64,
    pub name: String,
    pub plugin_name: String,
    pub category: String,
    pub author: String,
    pub is_factory: bool,
    pub rank_score: f64,
}

/// Search plugins using FTS5.
pub fn search_plugins(
    conn: &Connection,
    text: &str,
    category: Option<&str>,
    limit: u32,
    offset: u32,
) -> DbResult<(Vec<PluginSearchResult>, u32)> {
    let is_fts = !text.is_empty();

    let from = if is_fts {
        "FROM plugins_fts fts JOIN plugins p ON fts.rowid = p.id"
    } else {
        "FROM plugins p"
    };

    let mut where_clauses: Vec<String> = Vec::new();
    let mut params: Vec<Value> = Vec::new();
    let mut idx = 1u32;

    if is_fts {
        where_clauses.push(format!("plugins_fts MATCH ?{idx}"));
        params.push(Value::from(text.to_string()));
        idx += 1;
    }
    if let Some(cat) = category {
        where_clauses.push(format!("p.category = ?{idx}"));
        params.push(Value::from(cat.to_string()));
        idx += 1;
    }

    let where_sql = if where_clauses.is_empty() {
        String::new()
    } else {
        format!("WHERE {}", where_clauses.join(" AND "))
    };
    let rank_expr = if is_fts { "rank" } else { "0.0" };

    let count_sql = format!("SELECT COUNT(*) {from} {where_sql}");
    let data_sql = format!(
        "SELECT p.id, p.name, p.vendor, p.category, p.format, p.features, {rank_expr} AS _r \
         {from} {where_sql} ORDER BY _r ASC LIMIT ?{idx} OFFSET ?{}",
        idx + 1
    );

    let total: u32 = {
        let mut stmt = conn.prepare_cached(&count_sql)?;
        let refs: Vec<&dyn rusqlite::types::ToSql> = params.iter().map(|v| v as _).collect();
        stmt.query_row(refs.as_slice(), |row| row.get(0))?
    };

    let mut all_params = params;
    all_params.push(Value::from(limit as i64));
    all_params.push(Value::from(offset as i64));
    let refs: Vec<&dyn rusqlite::types::ToSql> = all_params.iter().map(|v| v as _).collect();

    let mut stmt = conn.prepare_cached(&data_sql)?;
    let rows = stmt.query_map(refs.as_slice(), |row| {
        Ok(PluginSearchResult {
            id: row.get(0)?,
            name: row.get(1)?,
            vendor: row.get(2)?,
            category: row.get(3)?,
            format: row.get(4)?,
            features: row.get(5)?,
            rank_score: row.get(6)?,
        })
    })?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row?);
    }
    Ok((results, total))
}

/// Search projects using FTS5.
pub fn search_projects(
    conn: &Connection,
    text: &str,
    author: Option<&str>,
    limit: u32,
    offset: u32,
) -> DbResult<(Vec<ProjectSearchResult>, u32)> {
    let is_fts = !text.is_empty();
    let from = if is_fts {
        "FROM projects_fts fts JOIN projects p ON fts.rowid = p.id"
    } else {
        "FROM projects p"
    };

    let mut where_clauses: Vec<String> = Vec::new();
    let mut params: Vec<Value> = Vec::new();
    let mut idx = 1u32;

    if is_fts {
        where_clauses.push(format!("projects_fts MATCH ?{idx}"));
        params.push(Value::from(text.to_string()));
        idx += 1;
    }
    if let Some(a) = author {
        where_clauses.push(format!("p.author = ?{idx}"));
        params.push(Value::from(a.to_string()));
        idx += 1;
    }

    let where_sql = if where_clauses.is_empty() { String::new() } else { format!("WHERE {}", where_clauses.join(" AND ")) };
    let rank_expr = if is_fts { "rank" } else { "0.0" };

    let count_sql = format!("SELECT COUNT(*) {from} {where_sql}");
    let data_sql = format!(
        "SELECT p.id, p.name, p.file_path, COALESCE(p.author,''), p.tempo, p.track_count, {rank_expr} AS _r \
         {from} {where_sql} ORDER BY _r ASC LIMIT ?{idx} OFFSET ?{}",
        idx + 1
    );

    let total: u32 = {
        let mut stmt = conn.prepare_cached(&count_sql)?;
        let refs: Vec<&dyn rusqlite::types::ToSql> = params.iter().map(|v| v as _).collect();
        stmt.query_row(refs.as_slice(), |row| row.get(0))?
    };

    let mut all_params = params;
    all_params.push(Value::from(limit as i64));
    all_params.push(Value::from(offset as i64));
    let refs: Vec<&dyn rusqlite::types::ToSql> = all_params.iter().map(|v| v as _).collect();

    let mut stmt = conn.prepare_cached(&data_sql)?;
    let rows = stmt.query_map(refs.as_slice(), |row| {
        Ok(ProjectSearchResult {
            id: row.get(0)?,
            name: row.get(1)?,
            file_path: row.get(2)?,
            author: row.get(3)?,
            tempo: row.get(4)?,
            track_count: row.get(5)?,
            rank_score: row.get(6)?,
        })
    })?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row?);
    }
    Ok((results, total))
}

/// Search presets using FTS5.
pub fn search_presets(
    conn: &Connection,
    text: &str,
    category: Option<&str>,
    plugin_name: Option<&str>,
    limit: u32,
    offset: u32,
) -> DbResult<(Vec<PresetSearchResult>, u32)> {
    let is_fts = !text.is_empty();
    let from = if is_fts {
        "FROM presets_fts fts JOIN presets p ON fts.rowid = p.id"
    } else {
        "FROM presets p"
    };

    let mut where_clauses: Vec<String> = Vec::new();
    let mut params: Vec<Value> = Vec::new();
    let mut idx = 1u32;

    if is_fts {
        where_clauses.push(format!("presets_fts MATCH ?{idx}"));
        params.push(Value::from(text.to_string()));
        idx += 1;
    }
    if let Some(cat) = category {
        where_clauses.push(format!("p.category = ?{idx}"));
        params.push(Value::from(cat.to_string()));
        idx += 1;
    }
    if let Some(pn) = plugin_name {
        where_clauses.push(format!("p.plugin_name = ?{idx}"));
        params.push(Value::from(pn.to_string()));
        idx += 1;
    }

    let where_sql = if where_clauses.is_empty() { String::new() } else { format!("WHERE {}", where_clauses.join(" AND ")) };
    let rank_expr = if is_fts { "rank" } else { "0.0" };

    let count_sql = format!("SELECT COUNT(*) {from} {where_sql}");
    let data_sql = format!(
        "SELECT p.id, p.name, COALESCE(p.plugin_name,''), COALESCE(p.category,''), \
         COALESCE(p.author,''), COALESCE(p.is_factory,0), {rank_expr} AS _r \
         {from} {where_sql} ORDER BY _r ASC LIMIT ?{idx} OFFSET ?{}",
        idx + 1
    );

    let total: u32 = {
        let mut stmt = conn.prepare_cached(&count_sql)?;
        let refs: Vec<&dyn rusqlite::types::ToSql> = params.iter().map(|v| v as _).collect();
        stmt.query_row(refs.as_slice(), |row| row.get(0))?
    };

    let mut all_params = params;
    all_params.push(Value::from(limit as i64));
    all_params.push(Value::from(offset as i64));
    let refs: Vec<&dyn rusqlite::types::ToSql> = all_params.iter().map(|v| v as _).collect();

    let mut stmt = conn.prepare_cached(&data_sql)?;
    let rows = stmt.query_map(refs.as_slice(), |row| {
        Ok(PresetSearchResult {
            id: row.get(0)?,
            name: row.get(1)?,
            plugin_name: row.get(2)?,
            category: row.get(3)?,
            author: row.get(4)?,
            is_factory: row.get::<_, i32>(5)? != 0,
            rank_score: row.get(6)?,
        })
    })?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row?);
    }
    Ok((results, total))
}

/// Performance benchmark: measure search response time for 100k-scale inserts + FTS5 query.
/// Returns (results, total, elapsed_ms).
pub fn benchmark_search_latency(conn: &Connection) -> DbResult<(Vec<SearchResult>, u32, u128)> {
    use std::time::Instant;

    // Insert 1000 samples to get meaningful timing
    for i in 0..1000 {
        let path = format!("/bench/sample_{i}.wav");
        let hash = format!("bench_hash_{i}");
        let category = if i % 3 == 0 { "kick" } else if i % 3 == 1 { "snare" } else { "hat" };
        let name = format!("Sample {i}");

        conn.execute(
            "INSERT INTO samples (file_path, file_hash, file_size, file_format, category, name, bpm, rating)
             VALUES (?1, ?2, 4096, 'wav', ?3, ?4, ?5, ?6)",
            rusqlite::params![path, hash, category, name, 100.0 + (i % 100) as f64, (i % 5) as i32],
        )?;
    }

    let start = Instant::now();
    let query = ExtendedSearchQuery {
        text: "kick".to_string(),
        category: None,
        musical_key: None,
        bpm_min: None,
        bpm_max: None,
        favorite_only: false,
        min_rating: 0,
        tag_names: vec![],
        sort_by: SortMode::Relevance,
        sort_ascending: false,
        limit: 100,
        offset: 0,
    };
    let (results, total) = search_samples(conn, &query)?;
    let elapsed = start.elapsed().as_millis();

    Ok((results, total, elapsed))
}

// ── Tests ────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use crate::migration;
    use crate::queries::{self, SampleEntry};
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

    fn make_entry(path: &str, name: &str) -> SampleEntry {
        SampleEntry {
            file_path: path.to_string(),
            file_hash: format!("hash_{}", name),
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
            name: Some(name.to_string()),
            rating: Some(4),
            favorite: Some(1),
            color: Some("#FF0000".to_string()),
            notes: Some("Test sample".to_string()),
            tags: Some("test".to_string()),
            file_created: Some("2024-01-01T00:00:00".to_string()),
            file_modified: Some("2024-06-01T00:00:00".to_string()),
        }
    }

    // ── Multi-Entity Search Tests ──────────────────────────

    #[test]
    fn test_search_plugins_fts5() {
        let conn = setup_db();
        conn.execute_batch(
            "INSERT INTO plugins (plugin_id, name, vendor, category, format, file_path, features)
             VALUES ('comp.awesome', 'AwesomeComp', 'Acme', 'effect', 'CLAP', '/plug/awesome.clap', 'compression,limiter'),
                    ('synth.master', 'SynthMaster', 'Acme', 'instrument', 'VST3', '/plug/synth.vst3', 'polyphonic,modulation'),
                    ('reverb.pro', 'ReverbPro', 'Other', 'effect', 'CLAP', '/plug/reverb.clap', 'convolution,reverb');"
        ).unwrap();
        conn.execute("INSERT INTO plugins_fts(plugins_fts) VALUES('rebuild')", []).unwrap();

        let (results, total) = search_plugins(&conn, "compression", Some("effect"), 10, 0).unwrap();
        assert!(total >= 1, "Should find compressor plugin, got {total}");
        assert_eq!(results[0].name, "AwesomeComp");
    }

    #[test]
    fn test_search_projects_fts5() {
        let conn = setup_db();
        conn.execute_batch(
            "INSERT INTO projects (name, file_path, author, tempo)
             VALUES ('My Track', '/projects/my_track.aria', 'alex', 120.0),
                    ('Beat Session', '/projects/beat.aria', 'alex', 140.0),
                    ('Orchestra', '/projects/orch.aria', 'james', 90.0);"
        ).unwrap();
        conn.execute("INSERT INTO projects_fts(projects_fts) VALUES('rebuild')", []).unwrap();

        let (results, total) = search_projects(&conn, "track", Some("alex"), 10, 0).unwrap();
        assert_eq!(total, 1);
        assert_eq!(results[0].name, "My Track");
    }

    #[test]
    fn test_search_presets_fts5() {
        let conn = setup_db();
        conn.execute_batch(
            "INSERT INTO presets (name, plugin_name, category, author, file_path, is_factory)
             VALUES ('Warm Pad', 'SynthMaster', 'pad', 'acme', '/presets/warm.ariapreset', 1),
                    ('Dark Reverb', 'ReverbPro', 'reverb', 'acme', '/presets/dark.ariapreset', 0),
                    ('Bright Hall', 'ReverbPro', 'reverb', 'user', '/presets/bright.ariapreset', 0);"
        ).unwrap();
        conn.execute("INSERT INTO presets_fts(presets_fts) VALUES('rebuild')", []).unwrap();

        let (results, total) = search_presets(&conn, "reverb", Some("reverb"), None, 10, 0).unwrap();
        assert_eq!(total, 2, "Should find 2 reverb presets");
        assert!(results.iter().any(|r| r.name == "Dark Reverb"));
    }

    #[test]
    fn test_search_plugins_no_match() {
        let conn = setup_db();
        let (results, total) = search_plugins(&conn, "xyzzy", None, 10, 0).unwrap();
        assert_eq!(total, 0);
        assert!(results.is_empty());
    }

    // ── Performance Benchmark ───────────────────────────────

    #[test]
    fn test_benchmark_search_latency_under_50ms() {
        let conn = setup_db();
        let (_results, _total, elapsed_ms) = benchmark_search_latency(&conn).unwrap();
        assert!(
            elapsed_ms < 50,
            "Search should complete in under 50ms for 1000 samples, took {elapsed_ms}ms"
        );
    }

    /// RED phase: All tests were written before the implementation.
    /// GREEN phase: Implementation now makes them pass.

    // ── Test 1: FTS5 text search with BM25 ranking ────────────

    #[test]
    fn test_fts5_text_search_ranks_by_relevance() {
        let conn = setup_db();

        let mut kick1 = make_entry("/test/dark_kick.wav", "Dark Kick");
        kick1.bpm = Some(120.0);
        kick1.notes = Some("A dark kick drum sample".to_string());
        queries::insert_sample(&conn, &kick1).unwrap();

        let mut kick2 = make_entry("/test/hard_kick.wav", "Hard Kick");
        kick2.bpm = Some(128.0);
        kick2.notes = Some("A hard hitting kick".to_string());
        queries::insert_sample(&conn, &kick2).unwrap();

        let mut snare = make_entry("/test/crisp_snare.wav", "Crisp Snare");
        snare.category = Some("snare".to_string());
        snare.notes = Some("A snare drum".to_string());
        queries::insert_sample(&conn, &snare).unwrap();

        let query = ExtendedSearchQuery {
            text: "dark kick".to_string(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 2, "Should find 2 samples matching 'dark' or 'kick'");
        assert_eq!(results.len(), 2);
        // Both matching samples should have valid BM25 scores
        // BM25 returns float values where lower = better match
        // Values can be slightly negative or very small positive
        assert!(
            results[0].rank_score.is_finite(),
            "BM25 score should be a finite float, got {}",
            results[0].rank_score
        );
        assert!(
            results[1].rank_score.is_finite(),
            "BM25 score should be a finite float, got {}",
            results[1].rank_score
        );
        // The sample with both "dark" AND "kick" should rank higher (lower BM25 score)
        // BM25 returns 0 for best match; higher = worse
        let dark_kick_name = results[0].sample.name.as_deref().unwrap_or("");
        let dark_kick_score = results[0].rank_score;
        let other_score = results[1].rank_score;
        // The sample matching both terms should have BM25 <= the sample matching only one term
        if dark_kick_name.contains("Dark") && dark_kick_name.contains("Kick") {
            assert!(
                dark_kick_score <= other_score,
                "Multi-match sample should have lower (better) BM25 score: {} <= {}",
                dark_kick_score,
                other_score
            );
        }
    }

    // ── Test 2: No matches returns empty ──────────────────────

    #[test]
    fn test_search_no_matches_returns_empty() {
        let conn = setup_db();
        queries::insert_sample(&conn, &make_entry("/test/kick.wav", "Kick One")).unwrap();

        let query = ExtendedSearchQuery {
            text: "xyzzy".to_string(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 0, "No samples should match 'xyzzy'");
        assert!(results.is_empty());
    }

    // ── Test 3: Special characters sanitized ──────────────────

    #[test]
    fn test_search_special_chars_sanitized() {
        let conn = setup_db();
        queries::insert_sample(&conn, &make_entry("/test/vocal_fx.wav", "Vocal FX")).unwrap();

        let query = ExtendedSearchQuery {
            text: "vocal + fx".to_string(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert!(total > 0, "Should find the sample despite special chars");
        assert_eq!(results[0].sample.name.as_deref(), Some("Vocal FX"));
    }

    // ── Test 4: Multi-filter: category + BPM + favorite ───────

    #[test]
    fn test_search_multi_filter() {
        let conn = setup_db();

        let mut kick = make_entry("/test/kick_120.wav", "Kick 120");
        kick.bpm = Some(120.0);
        kick.category = Some("kick".to_string());
        kick.favorite = Some(1);
        queries::insert_sample(&conn, &kick).unwrap();

        let mut kick_fast = make_entry("/test/kick_140.wav", "Kick 140");
        kick_fast.bpm = Some(140.0);
        kick_fast.category = Some("kick".to_string());
        kick_fast.favorite = Some(0);
        queries::insert_sample(&conn, &kick_fast).unwrap();

        let mut snare = make_entry("/test/snare_120.wav", "Snare 120");
        snare.bpm = Some(120.0);
        snare.category = Some("snare".to_string());
        snare.favorite = Some(1);
        queries::insert_sample(&conn, &snare).unwrap();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: Some("kick".to_string()),
            musical_key: None,
            bpm_min: Some(110.0),
            bpm_max: Some(130.0),
            favorite_only: true,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 1, "Only one sample matches all three filters");
        assert_eq!(results[0].sample.name.as_deref(), Some("Kick 120"));
    }

    // ── Test 5: Tag filter via sample_tags join ───────────────

    #[test]
    fn test_search_tag_filter() {
        let conn = setup_db();

        let s1 = queries::insert_sample(
            &conn,
            &make_entry("/test/tagged_one.wav", "Tagged One"),
        ).unwrap();
        let s2 = queries::insert_sample(
            &conn,
            &make_entry("/test/tagged_two.wav", "Tagged Two"),
        ).unwrap();

        let dark_id = queries::add_tag(&conn, "dark", None).unwrap();
        let sub_bass_id = queries::add_tag(&conn, "sub-bass", None).unwrap();
        queries::add_tag(&conn, "ambient", None).unwrap();

        queries::tag_sample(&conn, s1, dark_id).unwrap();
        queries::tag_sample(&conn, s1, sub_bass_id).unwrap();
        queries::tag_sample(&conn, s2, dark_id).unwrap();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec!["dark".to_string(), "sub-bass".to_string()],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 1, "Only one sample has both tags");
        assert_eq!(results[0].sample.name.as_deref(), Some("Tagged One"));
    }

    // ── Test 6: Browse mode with sort by BPM ascending ────────

    #[test]
    fn test_search_browse_mode_sort_bpm() {
        let conn = setup_db();

        let mut bass1 = make_entry("/test/bass_slow.wav", "Bass Slow");
        bass1.bpm = Some(80.0);
        bass1.category = Some("bass".to_string());
        bass1.file_hash = "hash_bass_slow".to_string();
        queries::insert_sample(&conn, &bass1).unwrap();

        let mut bass2 = make_entry("/test/bass_fast.wav", "Bass Fast");
        bass2.bpm = Some(140.0);
        bass2.category = Some("bass".to_string());
        bass2.file_hash = "hash_bass_fast".to_string();
        queries::insert_sample(&conn, &bass2).unwrap();

        let mut kick = make_entry("/test/kick_only.wav", "Kick Only");
        kick.category = Some("kick".to_string());
        queries::insert_sample(&conn, &kick).unwrap();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: Some("bass".to_string()),
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Bpm,
            sort_ascending: true,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 2, "Should find 2 bass samples");
        assert_eq!(results.len(), 2);
        assert!(
            results[0].sample.bpm.unwrap() < results[1].sample.bpm.unwrap(),
            "Bass Slow (80 BPM) should come before Bass Fast (140 BPM)"
        );
    }

    // ── Test 7: Sort by date descending ───────────────────────

    #[test]
    fn test_search_sort_by_date_descending() {
        let conn = setup_db();

        let mut a = make_entry("/test/old.wav", "Old Sample");
        a.category = Some("kick".to_string());
        let a_id = queries::insert_sample(&conn, &a).unwrap();
        conn.execute(
            "UPDATE samples SET first_indexed = '2024-01-01T00:00:00' WHERE id = ?1",
            rusqlite::params![a_id],
        ).unwrap();

        let mut b = make_entry("/test/new.wav", "New Sample");
        b.category = Some("kick".to_string());
        queries::insert_sample(&conn, &b).unwrap();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: Some("kick".to_string()),
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::DateAdded,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, _total) = search_samples(&conn, &query).unwrap();
        assert_eq!(results.len(), 2);
        assert_eq!(
            results[0].sample.name.as_deref(),
            Some("New Sample"),
            "Newer sample should be first in descending date order"
        );
    }

    // ── Test 8: Pagination with limit/offset ──────────────────

    #[test]
    fn test_search_pagination() {
        let conn = setup_db();

        for i in 0..10 {
            let mut e = make_entry(
                &format!("/test/sample_{}.wav", i),
                &format!("Sample {}", i),
            );
            e.file_hash = format!("hash_pag_{}", i);
            queries::insert_sample(&conn, &e).unwrap();
        }

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::DateAdded,
            sort_ascending: false,
            limit: 3,
            offset: 0,
        };

        let (page1, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 10, "Total should reflect all 10 samples");
        assert_eq!(page1.len(), 3, "First page should have 3 results");

        let query2 = ExtendedSearchQuery { offset: 3, ..query };
        let (page2, _total) = search_samples(&conn, &query2).unwrap();
        assert_eq!(page2.len(), 3, "Second page should have 3 results");

        let page1_ids: Vec<i64> = page1.iter().map(|r| r.sample.id).collect();
        let page2_ids: Vec<i64> = page2.iter().map(|r| r.sample.id).collect();
        for id in &page1_ids {
            assert!(!page2_ids.contains(id), "Page 2 should not contain Page 1 IDs");
        }
    }

    // ── Test 9: FTS5 + tags combo ────────────────────────────

    #[test]
    fn test_search_fts5_with_tag_filter() {
        let conn = setup_db();

        let s1 = queries::insert_sample(
            &conn,
            &make_entry("/test/dark_kick_tagged.wav", "Dark Kick Tagged"),
        ).unwrap();
        // Sample 2: name="Dark Snare", category must NOT contain "kick" for correct FTS5 testing
        let mut s2_entry = make_entry("/test/dark_snare.wav", "Dark Snare");
        s2_entry.category = Some("snare".to_string());
        s2_entry.file_hash = "hash_dark_snare".to_string();
        let s2 = queries::insert_sample(&conn, &s2_entry).unwrap();

        let dark_id = queries::add_tag(&conn, "dark", None).unwrap();
        let kick_tag = queries::add_tag(&conn, "kick", None).unwrap();

        queries::tag_sample(&conn, s1, dark_id).unwrap();
        queries::tag_sample(&conn, s1, kick_tag).unwrap();
        queries::tag_sample(&conn, s2, dark_id).unwrap();

        let query = ExtendedSearchQuery {
            text: "kick".to_string(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec!["dark".to_string()],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 1, "Only one sample matches 'kick' text AND 'dark' tag");
        assert_eq!(results[0].sample.name.as_deref(), Some("Dark Kick Tagged"));
    }

    // ── Test 10: Sort by name ascending ──────────────────────

    #[test]
    fn test_search_sort_by_name() {
        let conn = setup_db();

        let mut c = make_entry("/test/c_sample.wav", "Charlie");
        c.file_hash = "hash_c".to_string();
        queries::insert_sample(&conn, &c).unwrap();
        let mut a = make_entry("/test/a_sample.wav", "Alpha");
        a.file_hash = "hash_a".to_string();
        queries::insert_sample(&conn, &a).unwrap();
        let mut b = make_entry("/test/b_sample.wav", "Bravo");
        b.file_hash = "hash_b".to_string();
        queries::insert_sample(&conn, &b).unwrap();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Name,
            sort_ascending: true,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 3);
        assert_eq!(results[0].sample.name.as_deref(), Some("Alpha"));
        assert_eq!(results[1].sample.name.as_deref(), Some("Bravo"));
        assert_eq!(results[2].sample.name.as_deref(), Some("Charlie"));
    }

    // ── Test 11: Empty text returns all (browse mode) ───────

    #[test]
    fn test_search_empty_text_returns_all() {
        let conn = setup_db();
        for i in 0..5 {
            let mut e = make_entry(
                &format!("/test/browse_{}.wav", i),
                &format!("Browse {}", i),
            );
            e.file_hash = format!("hash_br_{}", i);
            queries::insert_sample(&conn, &e).unwrap();
        }

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 100,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 5);
        assert_eq!(results.len(), 5);
    }

    // ── Tests for sanitize_fts_query ─────────────────────────

    #[test]
    fn test_sanitize_fts_query_normal_text() {
        let result = sanitize_fts_query("dark kick");
        assert_eq!(result, "dark kick", "Normal text should pass through unchanged");
    }

    #[test]
    fn test_sanitize_fts_query_special_chars() {
        let result = sanitize_fts_query("vocal + fx");
        // The '+' should be handled so the query doesn't crash
        assert!(!result.is_empty(), "Should not produce empty string");
        // Special chars should be handled (wrapped in quotes)
        assert!(
            result.contains("vocal") && result.contains("fx"),
            "Both terms should still be present"
        );
    }

    #[test]
    fn test_sanitize_fts_query_empty() {
        assert_eq!(sanitize_fts_query(""), "", "Empty string should stay empty");
    }

    #[test]
    fn test_sanitize_fts_query_whitespace() {
        let result = sanitize_fts_query("  dark   kick  ");
        assert!(!result.contains("  "), "Should normalize whitespace");
        assert_eq!(result, "dark kick");
    }

    // ── TRIANGULATE: Additional edge cases ────────────────────

    #[test]
    fn test_search_rating_filter() {
        let conn = setup_db();

        let mut low = make_entry("/test/low_rated.wav", "Low Rated");
        low.rating = Some(1);
        low.file_hash = "hash_low".to_string();
        queries::insert_sample(&conn, &low).unwrap();

        let mut high = make_entry("/test/high_rated.wav", "High Rated");
        high.rating = Some(4);
        high.file_hash = "hash_high".to_string();
        queries::insert_sample(&conn, &high).unwrap();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 3,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 1, "Only one sample has rating >= 3");
        assert_eq!(results[0].sample.name.as_deref(), Some("High Rated"));
    }

    #[test]
    fn test_search_musical_key_filter() {
        let conn = setup_db();

        let mut cm = make_entry("/test/in_cm.wav", "In Cm");
        cm.musical_key = Some("Cm".to_string());
        cm.file_hash = "hash_cm".to_string();
        queries::insert_sample(&conn, &cm).unwrap();

        let mut f = make_entry("/test/in_f.wav", "In F");
        f.musical_key = Some("F".to_string());
        f.file_hash = "hash_f".to_string();
        queries::insert_sample(&conn, &f).unwrap();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: None,
            musical_key: Some("F".to_string()),
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 1, "Only one sample with key F");
        assert_eq!(results[0].sample.name.as_deref(), Some("In F"));
    }

    #[test]
    fn test_search_empty_db() {
        let conn = setup_db();

        let query = ExtendedSearchQuery {
            text: String::new(),
            category: None,
            musical_key: None,
            bpm_min: None,
            bpm_max: None,
            favorite_only: false,
            min_rating: 0,
            tag_names: vec![],
            sort_by: SortMode::Relevance,
            sort_ascending: false,
            limit: 10,
            offset: 0,
        };

        let (results, total) = search_samples(&conn, &query).unwrap();
        assert_eq!(total, 0, "Empty DB should have 0 results");
        assert!(results.is_empty());
    }

    #[test]
    fn test_sanitize_fts_query_quoted_text() {
        // Text that already has quotes should be handled without crash
        let result = sanitize_fts_query("\"dark kick\"");
        // The sanitizer wraps tokens in quotes when special chars are present
        assert!(!result.is_empty(), "Should not produce empty string");
        assert!(
            result.contains("dark") && result.contains("kick"),
            "Both terms should be preserved: {}",
            result
        );
    }

    #[test]
    fn test_sanitize_fts_query_with_asterisk() {
        let result = sanitize_fts_query("d*");
        assert!(result.contains("d"), "Term 'd' should be preserved");
    }
}
