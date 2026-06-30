// ARIA DAW — Database FFI Bridge
// C ABI exports for database operations.
// All functions are `#[no_mangle] extern "C"` with C-compatible types.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use aria_database::queries;
use aria_database::search;
use aria_database::Database;

// ── Opaque handle ───────────────────────────────────────────

/// Opaque handle to a database connection, returned to C/C++ callers.
/// Internally wraps a Box<Database>.
pub struct DbConnection {
    db: Database,
}

// ── FFI Structs (repr(C) to match aria_rust.h) ──────────────

#[derive(Copy, Clone)]
#[repr(C)]
pub struct FfiSampleEntry {
    pub file_path: [c_char; 1024],
    pub file_hash: [c_char; 65],
    pub file_size: i64,
    pub file_format: [c_char; 16],
    pub sample_rate: i32,
    pub bit_depth: i32,
    pub channels: i32,
    pub duration_ms: i32,
    pub bpm: f64,
    pub bpm_confidence: f64,
    pub musical_key: [c_char; 8],
    pub key_confidence: f64,
    pub loudness_lufs: f64,
    pub peak_db: f64,
    pub category: [c_char; 64],
    pub subcategory: [c_char; 64],
    pub mood: [c_char; 64],
    pub name: [c_char; 256],
    pub rating: i32,
    pub favorite: i32,
    pub color: [c_char; 8],
    pub notes: [c_char; 1024],
    pub file_created: [c_char; 32],
    pub file_modified: [c_char; 32],
}

#[derive(Copy, Clone)]
#[repr(C)]
pub struct FfiSearchQuery {
    pub text: [c_char; 256],
    pub category: [c_char; 64],
    pub bpm_min: f64,
    pub bpm_max: f64,
    pub musical_key: [c_char; 8],
    pub favorite_only: i32,
    pub min_rating: i32,
    pub limit: i32,
    pub offset: i32,
    // Fields added in PR3 — sort support
    pub sort_by: i32,         // 0=relevance, 1=name, 2=date_added, 3=bpm
    pub sort_ascending: i32,  // 0=descending, 1=ascending
}

#[derive(Copy, Clone)]
#[repr(C)]
pub struct FfiSampleResult {
    pub id: i64,
    pub file_path: [c_char; 1024],
    pub name: [c_char; 256],
    pub category: [c_char; 64],
    pub bpm: f64,
    pub musical_key: [c_char; 8],
    pub duration_ms: i32,
    pub loudness_lufs: f64,
    pub rating: i32,
    pub favorite: i32,
    pub rank_score: f64,
}

#[derive(Copy, Clone)]
#[repr(C)]
pub struct FfiTagResult {
    pub id: i64,
    pub name: [c_char; 128],
    pub color: [c_char; 8],
}

// ── Conversion helpers ──────────────────────────────────────

fn cstr_to_str(buf: &[c_char]) -> &str {
    let ptr = buf.as_ptr() as *const i8;
    let cstr = unsafe { CStr::from_ptr(ptr) };
    cstr.to_str().unwrap_or("")
}

fn str_to_carray<const N: usize>(s: &str, arr: &mut [c_char; N]) {
    let bytes = s.as_bytes();
    let len = bytes.len().min(N - 1);
    for (i, &b) in bytes[..len].iter().enumerate() {
        arr[i] = b as c_char;
    }
    arr[len] = 0; // null-terminate
}

fn entry_from_ffi(ffi: &FfiSampleEntry) -> queries::SampleEntry {
    queries::SampleEntry {
        file_path: cstr_to_str(&ffi.file_path).to_string(),
        file_hash: cstr_to_str(&ffi.file_hash).to_string(),
        file_size: ffi.file_size,
        file_format: cstr_to_str(&ffi.file_format).to_string(),
        sample_rate: if ffi.sample_rate > 0 { Some(ffi.sample_rate) } else { None },
        bit_depth: if ffi.bit_depth > 0 { Some(ffi.bit_depth) } else { None },
        channels: if ffi.channels > 0 { Some(ffi.channels) } else { None },
        duration_ms: if ffi.duration_ms > 0 { Some(ffi.duration_ms) } else { None },
        bpm: if ffi.bpm > 0.0 { Some(ffi.bpm) } else { None },
        bpm_confidence: if ffi.bpm_confidence > 0.0 { Some(ffi.bpm_confidence) } else { None },
        musical_key: str_nonempty(ffi.musical_key),
        key_confidence: if ffi.key_confidence > 0.0 { Some(ffi.key_confidence) } else { None },
        loudness_lufs: if ffi.loudness_lufs != 0.0 { Some(ffi.loudness_lufs) } else { None },
        peak_db: if ffi.peak_db != 0.0 { Some(ffi.peak_db) } else { None },
        category: str_nonempty(ffi.category),
        subcategory: str_nonempty(ffi.subcategory),
        mood: str_nonempty(ffi.mood),
        name: str_nonempty(ffi.name),
        rating: Some(ffi.rating),
        favorite: Some(ffi.favorite),
        color: str_nonempty(ffi.color),
        notes: str_nonempty(ffi.notes),
        tags: None, // Will be set via tag API
        file_created: str_nonempty(ffi.file_created),
        file_modified: str_nonempty(ffi.file_modified),
    }
}

fn str_nonempty<const N: usize>(arr: [c_char; N]) -> Option<String> {
    let s = cstr_to_str(&arr);
    if s.is_empty() { None } else { Some(s.to_string()) }
}

fn result_to_ffi(result: &queries::SampleResult, ffi: &mut FfiSampleResult, rank_score: f64) {
    ffi.id = result.id;
    str_to_carray(&result.file_path, &mut ffi.file_path);
    str_to_carray(&result.name.as_deref().unwrap_or(""), &mut ffi.name);
    str_to_carray(&result.category.as_deref().unwrap_or(""), &mut ffi.category);
    ffi.bpm = result.bpm.unwrap_or(0.0);
    str_to_carray(&result.musical_key.as_deref().unwrap_or(""), &mut ffi.musical_key);
    ffi.duration_ms = result.duration_ms.unwrap_or(0) as i32;
    ffi.loudness_lufs = result.loudness_lufs.unwrap_or(0.0);
    ffi.rating = result.rating;
    ffi.favorite = result.favorite;
    ffi.rank_score = rank_score;
}

fn tag_to_ffi(tag: &queries::Tag, ffi: &mut FfiTagResult) {
    ffi.id = tag.id;
    str_to_carray(&tag.name, &mut ffi.name);
    str_to_carray(&tag.color.as_deref().unwrap_or(""), &mut ffi.color);
}

// ── FFI Exports ─────────────────────────────────────────────

#[no_mangle]
pub extern "C" fn aria_db_open(path: *const c_char) -> *mut DbConnection {
    if path.is_null() {
        return std::ptr::null_mut();
    }
    let path_str = cstr_to_str(unsafe { std::slice::from_raw_parts(path as *const c_char, 1024) });
    match Database::open(path_str) {
        Ok(db) => Box::into_raw(Box::new(DbConnection { db })),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn aria_db_close(conn: *mut DbConnection) {
    if !conn.is_null() {
        unsafe { drop(Box::from_raw(conn)); }
    }
}

#[no_mangle]
pub extern "C" fn aria_db_run_migrations(conn: *mut DbConnection) -> bool {
    if conn.is_null() { return false; }
    let conn = unsafe { &*conn };
    conn.db.run_migrations().is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_schema_version(conn: *mut DbConnection) -> i32 {
    if conn.is_null() { return -1; }
    let conn = unsafe { &*conn };
    match queries::get_schema_version(&conn.db.conn()) {
        Ok(v) => v as i32,
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_insert_sample(conn: *mut DbConnection, entry: *const FfiSampleEntry) -> i64 {
    if conn.is_null() || entry.is_null() { return -1; }
    let conn = unsafe { &*conn };
    let ffi_entry = unsafe { &*entry };
    let sample_entry = entry_from_ffi(ffi_entry);
    match queries::insert_sample(&conn.db.conn(), &sample_entry) {
        Ok(id) => id,
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_update_sample(conn: *mut DbConnection, id: i64, entry: *const FfiSampleEntry) -> bool {
    if conn.is_null() || entry.is_null() { return false; }
    let conn = unsafe { &*conn };
    let ffi_entry = unsafe { &*entry };
    let sample_entry = entry_from_ffi(ffi_entry);
    queries::update_sample(&conn.db.conn(), id, &sample_entry).is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_delete_sample(conn: *mut DbConnection, id: i64) -> bool {
    if conn.is_null() { return false; }
    let conn = unsafe { &*conn };
    queries::delete_sample(&conn.db.conn(), id).is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_get_sample(conn: *mut DbConnection, id: i64, out: *mut FfiSampleResult) -> bool {
    if conn.is_null() || out.is_null() { return false; }
    let conn = unsafe { &*conn };
    match queries::get_sample(&conn.db.conn(), id) {
        Ok(result) => {
            let ffi_out = unsafe { &mut *out };
            result_to_ffi(&result, ffi_out, 0.0);
            true
        }
        Err(_) => false,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_search_samples(
    conn: *mut DbConnection,
    query: *const FfiSearchQuery,
    results: *mut FfiSampleResult,
    max_results: i32,
) -> i32 {
    if conn.is_null() || query.is_null() || results.is_null() || max_results <= 0 {
        return 0;
    }
    let conn = unsafe { &*conn };
    let ffi_query = unsafe { &*query };

    let sort_mode = match ffi_query.sort_by {
        1 => search::SortMode::Name,
        2 => search::SortMode::DateAdded,
        3 => search::SortMode::Bpm,
        _ => search::SortMode::Relevance,
    };

    let search_query = search::ExtendedSearchQuery {
        text: cstr_to_str(&ffi_query.text).to_string(),
        category: str_nonempty(ffi_query.category),
        musical_key: str_nonempty(ffi_query.musical_key),
        bpm_min: if ffi_query.bpm_min > 0.0 { Some(ffi_query.bpm_min) } else { None },
        bpm_max: if ffi_query.bpm_max > 0.0 { Some(ffi_query.bpm_max) } else { None },
        favorite_only: ffi_query.favorite_only != 0,
        min_rating: ffi_query.min_rating,
        tag_names: vec![],
        sort_by: sort_mode,
        sort_ascending: ffi_query.sort_ascending != 0,
        limit: if ffi_query.limit > 0 { ffi_query.limit as u32 } else { 50 },
        offset: if ffi_query.offset >= 0 { ffi_query.offset as u32 } else { 0 },
    };

    match search::search_samples(&conn.db.conn(), &search_query) {
        Ok((search_results, _total)) => {
            let count = search_results.len().min(max_results as usize);
            let results_slice = unsafe { std::slice::from_raw_parts_mut(results, count) };
            for (i, r) in search_results.iter().take(count).enumerate() {
                result_to_ffi(&r.sample, &mut results_slice[i], r.rank_score);
            }
            count as i32
        }
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_batch_insert_samples(
    conn: *mut DbConnection,
    entries: *const FfiSampleEntry,
    count: i32,
) -> i32 {
    if conn.is_null() || entries.is_null() || count <= 0 { return 0; }
    let conn = unsafe { &*conn };
    let entries_slice = unsafe { std::slice::from_raw_parts(entries, count as usize) };

    let sample_entries: Vec<queries::SampleEntry> = entries_slice.iter()
        .map(entry_from_ffi)
        .collect();

    match queries::batch_insert_samples(&conn.db.conn(), &sample_entries) {
        Ok(n) => n as i32,
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_sample_count(conn: *mut DbConnection) -> i32 {
    if conn.is_null() { return -1; }
    let conn = unsafe { &*conn };
    match queries::sample_count(&conn.db.conn()) {
        Ok(c) => c as i32,
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_add_tag(conn: *mut DbConnection, name: *const c_char, color: *const c_char) -> i64 {
    if conn.is_null() || name.is_null() { return -1; }
    let conn = unsafe { &*conn };
    let name_str = unsafe { CStr::from_ptr(name) }.to_str().unwrap_or("");
    let color_str = if color.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(color) }.to_str().unwrap_or("").to_string())
    };

    match queries::add_tag(&conn.db.conn(), name_str, color_str.as_deref()) {
        Ok(id) => id,
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_tag_sample(conn: *mut DbConnection, sample_id: i64, tag_id: i64) -> bool {
    if conn.is_null() { return false; }
    let conn = unsafe { &*conn };
    queries::tag_sample(&conn.db.conn(), sample_id, tag_id).is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_untag_sample(conn: *mut DbConnection, sample_id: i64, tag_id: i64) -> bool {
    if conn.is_null() { return false; }
    let conn = unsafe { &*conn };
    queries::untag_sample(&conn.db.conn(), sample_id, tag_id).is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_get_sample_tags(
    conn: *mut DbConnection,
    sample_id: i64,
    tags: *mut FfiTagResult,
    max_tags: i32,
) -> i32 {
    if conn.is_null() || tags.is_null() || max_tags <= 0 { return 0; }
    let conn = unsafe { &*conn };
    match queries::get_sample_tags(&conn.db.conn(), sample_id) {
        Ok(tag_list) => {
            let count = tag_list.len().min(max_tags as usize);
            let tags_slice = unsafe { std::slice::from_raw_parts_mut(tags, count) };
            for (i, t) in tag_list.iter().take(count).enumerate() {
                tag_to_ffi(t, &mut tags_slice[i]);
            }
            count as i32
        }
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_get_all_tags(
    conn: *mut DbConnection,
    tags: *mut FfiTagResult,
    max_tags: i32,
) -> i32 {
    if conn.is_null() || tags.is_null() || max_tags <= 0 { return 0; }
    let conn = unsafe { &*conn };
    match queries::get_all_tags(&conn.db.conn()) {
        Ok(tag_list) => {
            let count = tag_list.len().min(max_tags as usize);
            let tags_slice = unsafe { std::slice::from_raw_parts_mut(tags, count) };
            for (i, t) in tag_list.iter().take(count).enumerate() {
                tag_to_ffi(t, &mut tags_slice[i]);
            }
            count as i32
        }
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "C" fn aria_db_add_watched_folder(conn: *mut DbConnection, path: *const c_char, recursive: i32) -> bool {
    if conn.is_null() || path.is_null() { return false; }
    let conn = unsafe { &*conn };
    let path_str = unsafe { CStr::from_ptr(path) }.to_str().unwrap_or("");
    queries::add_watched_folder(&conn.db.conn(), path_str, recursive != 0).is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_remove_watched_folder(conn: *mut DbConnection, path: *const c_char) -> bool {
    if conn.is_null() || path.is_null() { return false; }
    let conn = unsafe { &*conn };
    let path_str = unsafe { CStr::from_ptr(path) }.to_str().unwrap_or("");
    queries::remove_watched_folder(&conn.db.conn(), path_str).is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_vacuum(conn: *mut DbConnection) -> bool {
    if conn.is_null() { return false; }
    let conn = unsafe { &*conn };
    queries::vacuum(&conn.db.conn()).is_ok()
}

#[no_mangle]
pub extern "C" fn aria_db_integrity_check(conn: *mut DbConnection, out_buffer: *mut c_char, buffer_size: i32) {
    if conn.is_null() || out_buffer.is_null() || buffer_size <= 0 { return; }
    let conn = unsafe { &*conn };
    let result = match queries::integrity_check(&conn.db.conn()) {
        Ok(errors) => errors.join("\n"),
        Err(e) => format!("Error: {}", e),
    };
    let c_result = CString::new(result).unwrap_or_default();
    let bytes = c_result.as_bytes_with_nul();
    let dest = unsafe { std::slice::from_raw_parts_mut(out_buffer as *mut u8, buffer_size as usize) };
    let copy_len = bytes.len().min(dest.len());
    dest[..copy_len].copy_from_slice(&bytes[..copy_len]);
    if copy_len < dest.len() {
        dest[copy_len] = 0; // ensure null termination
    }
}

#[no_mangle]
pub extern "C" fn aria_db_analyze(conn: *mut DbConnection) -> bool {
    if conn.is_null() { return false; }
    let conn = unsafe { &*conn };
    queries::analyze(&conn.db.conn()).is_ok()
}

// ── Multi-Entity Search FFI ──────────────────────────────────

#[repr(C)]
pub struct FfiPluginResult {
    pub id: i64,
    pub name: [c_char; 256],
    pub vendor: [c_char; 128],
    pub category: [c_char; 64],
    pub format: [c_char; 16],
    pub features: [c_char; 512],
    pub rank_score: f64,
}

#[repr(C)]
pub struct FfiProjectResult {
    pub id: i64,
    pub name: [c_char; 256],
    pub file_path: [c_char; 1024],
    pub author: [c_char; 128],
    pub tempo: f64,
    pub track_count: i32,
    pub rank_score: f64,
}

#[repr(C)]
pub struct FfiPresetResult {
    pub id: i64,
    pub name: [c_char; 256],
    pub plugin_name: [c_char; 128],
    pub category: [c_char; 64],
    pub author: [c_char; 128],
    pub is_factory: i32,
    pub rank_score: f64,
}

fn str_to_fixed<const N: usize>(s: &str) -> [c_char; N] {
    let mut buf = [0i8; N];
    let bytes = s.as_bytes();
    let len = bytes.len().min(N - 1);
    for (i, &b) in bytes[..len].iter().enumerate() {
        buf[i] = b as i8;
    }
    buf[len] = 0;
    buf
}

#[no_mangle]
pub extern "C" fn aria_db_search_plugins(
    conn: *mut DbConnection,
    text: *const c_char,
    category: *const c_char,
    limit: u32,
    offset: u32,
    out_count: *mut u32,
) -> *mut FfiPluginResult {
    if conn.is_null() { return std::ptr::null_mut(); }
    let conn = unsafe { &*conn };
    let text = if !text.is_null() { unsafe { CStr::from_ptr(text).to_str().unwrap_or("") } } else { "" };
    let category = if !category.is_null() { Some(unsafe { CStr::from_ptr(category).to_str().unwrap_or("") }) } else { None };
    let category_str = category.filter(|s| !s.is_empty());

    match search::search_plugins(&conn.db.conn(), text, category_str, limit, offset) {
        Ok((results, total)) => {
            if !out_count.is_null() { unsafe { *out_count = total; } }
            let mut vec = results.into_iter().map(|r| FfiPluginResult {
                id: r.id,
                name: str_to_fixed(&r.name),
                vendor: str_to_fixed(&r.vendor),
                category: str_to_fixed(&r.category),
                format: str_to_fixed(&r.format),
                features: str_to_fixed(&r.features),
                rank_score: r.rank_score,
            }).collect::<Vec<_>>();
            vec.shrink_to_fit();
            let ptr = vec.as_mut_ptr();
            std::mem::forget(vec);
            ptr
        }
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn aria_db_search_projects(
    conn: *mut DbConnection,
    text: *const c_char,
    author: *const c_char,
    limit: u32,
    offset: u32,
    out_count: *mut u32,
) -> *mut FfiProjectResult {
    if conn.is_null() { return std::ptr::null_mut(); }
    let conn = unsafe { &*conn };
    let text = if !text.is_null() { unsafe { CStr::from_ptr(text).to_str().unwrap_or("") } } else { "" };
    let author = if !author.is_null() { Some(unsafe { CStr::from_ptr(author).to_str().unwrap_or("") }) } else { None };
    let author_str = author.filter(|s| !s.is_empty());

    match search::search_projects(&conn.db.conn(), text, author_str, limit, offset) {
        Ok((results, total)) => {
            if !out_count.is_null() { unsafe { *out_count = total; } }
            let mut vec = results.into_iter().map(|r| FfiProjectResult {
                id: r.id,
                name: str_to_fixed(&r.name),
                file_path: str_to_fixed(&r.file_path),
                author: str_to_fixed(&r.author),
                tempo: r.tempo.unwrap_or(0.0),
                track_count: r.track_count.unwrap_or(0),
                rank_score: r.rank_score,
            }).collect::<Vec<_>>();
            vec.shrink_to_fit();
            let ptr = vec.as_mut_ptr();
            std::mem::forget(vec);
            ptr
        }
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn aria_db_search_presets(
    conn: *mut DbConnection,
    text: *const c_char,
    category: *const c_char,
    plugin_name: *const c_char,
    limit: u32,
    offset: u32,
    out_count: *mut u32,
) -> *mut FfiPresetResult {
    if conn.is_null() { return std::ptr::null_mut(); }
    let conn = unsafe { &*conn };
    let text = if !text.is_null() { unsafe { CStr::from_ptr(text).to_str().unwrap_or("") } } else { "" };
    let category = if !category.is_null() { Some(unsafe { CStr::from_ptr(category).to_str().unwrap_or("") }) } else { None };
    let pn = if !plugin_name.is_null() { Some(unsafe { CStr::from_ptr(plugin_name).to_str().unwrap_or("") }) } else { None };
    let category_str = category.filter(|s| !s.is_empty());
    let plugin_str = pn.filter(|s| !s.is_empty());

    match search::search_presets(&conn.db.conn(), text, category_str, plugin_str, limit, offset) {
        Ok((results, total)) => {
            if !out_count.is_null() { unsafe { *out_count = total; } }
            let mut vec = results.into_iter().map(|r| FfiPresetResult {
                id: r.id,
                name: str_to_fixed(&r.name),
                plugin_name: str_to_fixed(&r.plugin_name),
                category: str_to_fixed(&r.category),
                author: str_to_fixed(&r.author),
                is_factory: if r.is_factory { 1 } else { 0 },
                rank_score: r.rank_score,
            }).collect::<Vec<_>>();
            vec.shrink_to_fit();
            let ptr = vec.as_mut_ptr();
            std::mem::forget(vec);
            ptr
        }
        Err(_) => std::ptr::null_mut(),
    }
}

/// Free results from multi-entity search.
#[no_mangle]
pub unsafe extern "C" fn aria_db_free_plugin_results(results: *mut FfiPluginResult) {
    if !results.is_null() { drop(Box::from_raw(results)); }
}

#[no_mangle]
pub unsafe extern "C" fn aria_db_free_project_results(results: *mut FfiProjectResult) {
    if !results.is_null() { drop(Box::from_raw(results)); }
}

#[no_mangle]
pub unsafe extern "C" fn aria_db_free_preset_results(results: *mut FfiPresetResult) {
    if !results.is_null() { drop(Box::from_raw(results)); }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    /// Helper: open an in-memory DB via FFI, run migrations, return conn ptr.
    fn setup_ffi_db() -> *mut DbConnection {
        let path = CString::new(":memory:").unwrap();
        let conn = aria_db_open(path.as_ptr());
        assert!(!conn.is_null());
        let ok = aria_db_run_migrations(conn);
        assert!(ok);
        conn
    }

    #[test]
    fn test_ffi_open_close() {
        let path = CString::new(":memory:").unwrap();
        let conn = aria_db_open(path.as_ptr());
        assert!(!conn.is_null());
        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_open_null_returns_null() {
        let conn = aria_db_open(std::ptr::null());
        assert!(conn.is_null());
    }

    #[test]
    fn test_ffi_run_migrations() {
        let conn = setup_ffi_db();
        let version = aria_db_schema_version(conn);
        assert!(version >= 1);
        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_insert_and_get_sample() {
        let conn = setup_ffi_db();

        let mut entry = FfiSampleEntry {
            file_path: [0; 1024],
            file_hash: [0; 65],
            file_size: 4096,
            file_format: [0; 16],
            sample_rate: 44100,
            bit_depth: 24,
            channels: 2,
            duration_ms: 5000,
            bpm: 120.0,
            bpm_confidence: 0.9,
            musical_key: [0; 8],
            key_confidence: 0.0,
            loudness_lufs: -14.5,
            peak_db: 0.0,
            category: [0; 64],
            subcategory: [0; 64],
            mood: [0; 64],
            name: [0; 256],
            rating: 4,
            favorite: 1,
            color: [0; 8],
            notes: [0; 1024],
            file_created: [0; 32],
            file_modified: [0; 32],
        };

        str_to_carray("/test/ffi_test.wav", &mut entry.file_path);
        str_to_carray("abc123def456", &mut entry.file_hash);
        str_to_carray("wav", &mut entry.file_format);
        str_to_carray("Test FFI", &mut entry.name);
        str_to_carray("kick", &mut entry.category);

        let id = aria_db_insert_sample(conn, &entry);
        assert!(id > 0, "Insert should return positive ID, got {}", id);

        let mut result = FfiSampleResult {
            id: 0,
            file_path: [0; 1024],
            name: [0; 256],
            category: [0; 64],
            bpm: 0.0,
            musical_key: [0; 8],
            duration_ms: 0,
            loudness_lufs: 0.0,
            rating: 0,
            favorite: 0,
            rank_score: 0.0,
        };

        let found = aria_db_get_sample(conn, id, &mut result as *mut FfiSampleResult);
        assert!(found, "Should find sample by ID");
        assert_eq!(result.id, id);
        assert_eq!(cstr_to_str(&result.name), "Test FFI");
        assert_eq!(result.rating, 4);

        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_delete_sample() {
        let conn = setup_ffi_db();

        let mut entry = FfiSampleEntry {
            file_path: [0; 1024],
            file_hash: [0; 65],
            file_size: 2048,
            file_format: [0; 16],
            sample_rate: 0,
            bit_depth: 0,
            channels: 0,
            duration_ms: 0,
            bpm: 0.0,
            bpm_confidence: 0.0,
            musical_key: [0; 8],
            key_confidence: 0.0,
            loudness_lufs: 0.0,
            peak_db: 0.0,
            category: [0; 64],
            subcategory: [0; 64],
            mood: [0; 64],
            name: [0; 256],
            rating: 0,
            favorite: 0,
            color: [0; 8],
            notes: [0; 1024],
            file_created: [0; 32],
            file_modified: [0; 32],
        };

        str_to_carray("/test/ffi_del.wav", &mut entry.file_path);
        str_to_carray("delhash", &mut entry.file_hash);
        str_to_carray("wav", &mut entry.file_format);

        let id = aria_db_insert_sample(conn, &entry);
        assert!(id > 0);

        let deleted = aria_db_delete_sample(conn, id);
        assert!(deleted);

        let mut result = FfiSampleResult {
            id: 0, file_path: [0; 1024], name: [0; 256], category: [0; 64],
            bpm: 0.0, musical_key: [0; 8], duration_ms: 0, loudness_lufs: 0.0,
            rating: 0, favorite: 0, rank_score: 0.0,
        };
        let found = aria_db_get_sample(conn, id, &mut result as *mut FfiSampleResult);
        assert!(!found, "Deleted sample should not be found");

        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_tag_operations() {
        let conn = setup_ffi_db();

        // Add tags
        let tag_id = aria_db_add_tag(conn, CString::new("dark").unwrap().as_ptr(),
                                       CString::new("#000000").unwrap().as_ptr());
        assert!(tag_id > 0);

        let tag_id2 = aria_db_add_tag(conn, CString::new("bass").unwrap().as_ptr(),
                                        std::ptr::null());
        assert!(tag_id2 > 0);

        // Duplicate tag should return error (-1)
        let dup = aria_db_add_tag(conn, CString::new("dark").unwrap().as_ptr(),
                                    std::ptr::null());
        assert_eq!(dup, -1, "Duplicate tag should return -1");

        // Insert sample and tag it
        let mut entry = FfiSampleEntry {
            file_path: [0; 1024], file_hash: [0; 65], file_size: 100,
            file_format: [0; 16], sample_rate: 0, bit_depth: 0, channels: 0,
            duration_ms: 0, bpm: 0.0, bpm_confidence: 0.0, musical_key: [0; 8],
            key_confidence: 0.0, loudness_lufs: 0.0, peak_db: 0.0,
            category: [0; 64], subcategory: [0; 64], mood: [0; 64],
            name: [0; 256], rating: 0, favorite: 0, color: [0; 8],
            notes: [0; 1024], file_created: [0; 32], file_modified: [0; 32],
        };
        str_to_carray("/test/tag_ffi.wav", &mut entry.file_path);
        str_to_carray("taghash", &mut entry.file_hash);
        str_to_carray("wav", &mut entry.file_format);

        let sample_id = aria_db_insert_sample(conn, &entry);
        assert!(sample_id > 0);

        let tagged = aria_db_tag_sample(conn, sample_id, tag_id);
        assert!(tagged);

        // Get tags
        let mut tags = [FfiTagResult {
            id: 0, name: [0; 128], color: [0; 8],
        }; 10];
        let count = aria_db_get_sample_tags(conn, sample_id, tags.as_mut_ptr(), 10);
        assert_eq!(count, 1);
        assert_eq!(cstr_to_str(&tags[0].name), "dark");

        // Untag
        let untagged = aria_db_untag_sample(conn, sample_id, tag_id);
        assert!(untagged);

        let count = aria_db_get_sample_tags(conn, sample_id, tags.as_mut_ptr(), 10);
        assert_eq!(count, 0);

        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_search_samples() {
        let conn = setup_ffi_db();

        // Insert a few samples
        for i in 0..3 {
            let mut entry = FfiSampleEntry {
                file_path: [0; 1024], file_hash: [0; 65], file_size: 100 + i,
                file_format: [0; 16], sample_rate: 0, bit_depth: 0, channels: 0,
                duration_ms: 0, bpm: 0.0, bpm_confidence: 0.0, musical_key: [0; 8],
                key_confidence: 0.0, loudness_lufs: 0.0, peak_db: 0.0,
                category: [0; 64], subcategory: [0; 64], mood: [0; 64],
                name: [0; 256], rating: 0, favorite: 0, color: [0; 8],
                notes: [0; 1024], file_created: [0; 32], file_modified: [0; 32],
            };
            str_to_carray(&format!("/test/ffi_search_{}.wav", i), &mut entry.file_path);
            str_to_carray(&format!("hash_{}", i), &mut entry.file_hash);
            str_to_carray("wav", &mut entry.file_format);
            str_to_carray(&format!("Sample {}", i), &mut entry.name);
            aria_db_insert_sample(conn, &entry);
        }

        // Search with empty query — should return all
        let query = FfiSearchQuery {
            text: [0; 256], category: [0; 64], bpm_min: 0.0, bpm_max: 0.0,
            musical_key: [0; 8], favorite_only: 0, min_rating: 0,
            limit: 10, offset: 0,
            sort_by: 0, sort_ascending: 0,
        };

        let mut results = [FfiSampleResult {
            id: 0, file_path: [0; 1024], name: [0; 256], category: [0; 64],
            bpm: 0.0, musical_key: [0; 8], duration_ms: 0, loudness_lufs: 0.0,
            rating: 0, favorite: 0, rank_score: 0.0,
        }; 10];

        let count = aria_db_search_samples(conn, &query, results.as_mut_ptr(), 10);
        assert_eq!(count, 3, "Should find all 3 samples with empty search");

        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_batch_insert() {
        let conn = setup_ffi_db();

        let mut entries = Vec::new();
        for i in 0..5 {
            let mut entry = FfiSampleEntry {
                file_path: [0; 1024], file_hash: [0; 65], file_size: 100 + i,
                file_format: [0; 16], sample_rate: 0, bit_depth: 0, channels: 0,
                duration_ms: 0, bpm: 0.0, bpm_confidence: 0.0, musical_key: [0; 8],
                key_confidence: 0.0, loudness_lufs: 0.0, peak_db: 0.0,
                category: [0; 64], subcategory: [0; 64], mood: [0; 64],
                name: [0; 256], rating: 0, favorite: 0, color: [0; 8],
                notes: [0; 1024], file_created: [0; 32], file_modified: [0; 32],
            };
            str_to_carray(&format!("/test/batch_ffi_{}.wav", i), &mut entry.file_path);
            str_to_carray(&format!("bh_{}", i), &mut entry.file_hash);
            str_to_carray("wav", &mut entry.file_format);
            entries.push(entry);
        }

        let count = aria_db_batch_insert_samples(conn, entries.as_ptr(), 5);
        assert_eq!(count, 5);

        assert_eq!(aria_db_sample_count(conn), 5);

        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_watched_folders() {
        let conn = setup_ffi_db();

        let path1 = CString::new("/home/user/samples").unwrap();
        let path2 = CString::new("/home/user/loops").unwrap();

        let ok = aria_db_add_watched_folder(conn, path1.as_ptr(), 1);
        assert!(ok);

        let ok = aria_db_add_watched_folder(conn, path2.as_ptr(), 0);
        assert!(ok);

        let ok = aria_db_remove_watched_folder(conn, path1.as_ptr());
        assert!(ok);

        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_maintenance() {
        let conn = setup_ffi_db();

        let ok = aria_db_vacuum(conn);
        assert!(ok);

        let mut buf = [0i8; 4096];
        aria_db_integrity_check(conn, buf.as_mut_ptr(), 4096);
        let result = cstr_to_str(unsafe { std::slice::from_raw_parts(buf.as_ptr() as *const c_char, 4096) });
        assert!(result.is_empty() || result == "ok", "Integrity check should pass, got: {}", result);

        let ok = aria_db_analyze(conn);
        assert!(ok);

        aria_db_close(conn);
    }

    #[test]
    fn test_ffi_null_handling() {
        // All functions should handle null gracefully
        assert!(aria_db_run_migrations(std::ptr::null_mut()) == false);
        assert_eq!(aria_db_schema_version(std::ptr::null_mut()), -1);
        assert_eq!(aria_db_insert_sample(std::ptr::null_mut(), std::ptr::null()), -1);
        assert_eq!(aria_db_sample_count(std::ptr::null_mut()), -1);
        assert!(aria_db_delete_sample(std::ptr::null_mut(), 1) == false);
        assert!(aria_db_tag_sample(std::ptr::null_mut(), 1, 1) == false);
        assert!(aria_db_vacuum(std::ptr::null_mut()) == false);
    }
}
