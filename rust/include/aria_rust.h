// ARIA DAW — Rust FFI Header
// C ABI functions exported by the Rust library.
// Included by C++ code to call Rust functions.

#ifndef ARIA_RUST_H
#define ARIA_RUST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Database ─────────────────────────────────────────────────

typedef struct DbConnection DbConnection;

// ── Input structs ────────────────────────────────────────────

/// Input struct for inserting or updating a sample entry.
typedef struct {
    char file_path[1024];
    char file_hash[65];            // SHA-256 hex string
    int64_t file_size;
    char file_format[16];          // wav, aiff, flac, mp3, ogg, m4a
    int32_t sample_rate;
    int32_t bit_depth;
    int32_t channels;
    int32_t duration_ms;
    double bpm;
    double bpm_confidence;
    char musical_key[8];           // "Cm", "F#", etc.
    double key_confidence;
    double loudness_lufs;
    double peak_db;
    char category[64];
    char subcategory[64];
    char mood[64];
    char name[256];
    int32_t rating;                // 0-5
    int32_t favorite;              // 0 or 1
    char color[8];                 // Hex color "#RRGGBB"
    char notes[1024];
    char file_created[32];         // ISO 8601
    char file_modified[32];
} SampleEntry;

/// Search query for samples with optional filters.
typedef struct {
    char text[256];                // FTS5 full-text query
    char category[64];
    double bpm_min;
    double bpm_max;
    char musical_key[8];
    int32_t favorite_only;         // 0 or 1
    int32_t min_rating;
    int32_t limit;
    int32_t offset;
    int32_t sort_by;               // 0=relevance, 1=name, 2=date_added, 3=bpm
    int32_t sort_ascending;        // 0=descending, 1=ascending
} DbSearchQuery;

// ── Result structs ───────────────────────────────────────────

/// Result struct for a single sample search result.
typedef struct {
    int64_t id;
    char file_path[1024];
    char name[256];
    char category[64];
    double bpm;
    char musical_key[8];
    int32_t duration_ms;
    double loudness_lufs;
    int32_t rating;
    int32_t favorite;
    double rank_score;             // BM25 relevance score
} SampleResult;

/// Result struct for tag queries.
typedef struct {
    int64_t id;
    char name[128];
    char color[8];
} TagResult;

// ── DB Function declarations ────────────────────────────────

/// Open a database at the given path. Returns NULL on failure.
DbConnection* aria_db_open(const char* path);

/// Close a database connection.
void aria_db_close(DbConnection* conn);

/// Run all pending migrations. Returns true on success.
bool aria_db_run_migrations(DbConnection* conn);

/// Returns the current schema version, or -1 on error.
int32_t aria_db_schema_version(DbConnection* conn);

/// Insert a sample. Returns the new row ID, or -1 on error/duplicate.
int64_t aria_db_insert_sample(DbConnection* conn, const SampleEntry* entry);

/// Update a sample by ID. Returns true on success.
bool aria_db_update_sample(DbConnection* conn, int64_t id, const SampleEntry* entry);

/// Delete a sample by ID. Returns true on success.
bool aria_db_delete_sample(DbConnection* conn, int64_t id);

/// Get a single sample by ID. Returns true if found, false if not.
bool aria_db_get_sample(DbConnection* conn, int64_t id, SampleResult* out);

/// Search samples using FTS5. Returns the number of results written.
int32_t aria_db_search_samples(
    DbConnection* conn,
    const DbSearchQuery* query,
    SampleResult* results,
    int32_t max_results
);

/// Batch insert samples. Returns number successfully inserted.
int32_t aria_db_batch_insert_samples(
    DbConnection* conn,
    const SampleEntry* entries,
    int32_t count
);

/// Get total sample count. Returns -1 on error.
int32_t aria_db_sample_count(DbConnection* conn);

/// Create a tag. Returns the new tag ID, or -1 on error.
int64_t aria_db_add_tag(DbConnection* conn, const char* name, const char* color);

/// Assign a tag to a sample. Returns true on success.
bool aria_db_tag_sample(DbConnection* conn, int64_t sample_id, int64_t tag_id);

/// Remove a tag from a sample. Returns true on success.
bool aria_db_untag_sample(DbConnection* conn, int64_t sample_id, int64_t tag_id);

/// Get all tags for a sample. Returns number of tags written.
int32_t aria_db_get_sample_tags(
    DbConnection* conn,
    int64_t sample_id,
    TagResult* tags,
    int32_t max_tags
);

/// Get all tags. Returns number of tags written.
int32_t aria_db_get_all_tags(
    DbConnection* conn,
    TagResult* tags,
    int32_t max_tags
);

/// Add a watched folder path. Returns true on success.
bool aria_db_add_watched_folder(DbConnection* conn, const char* path, int32_t recursive);

/// Remove a watched folder path. Returns true on success.
bool aria_db_remove_watched_folder(DbConnection* conn, const char* path);

/// Run VACUUM. Returns true on success.
bool aria_db_vacuum(DbConnection* conn);

/// Run integrity check. Returns error messages (one per line), or empty string.
void aria_db_integrity_check(DbConnection* conn, char* out_buffer, int32_t buffer_size);

/// Run ANALYZE. Returns true on success.
bool aria_db_analyze(DbConnection* conn);

// ── File System ──────────────────────────────────────────────

/// File information returned by the scanner.
typedef struct {
    char path[1024];
    int64_t file_size;
    int64_t modified_at;
    bool is_directory;
    char extension[16];
} FileInfo;

/// Result of a scanned file, returned via ScanProgressCallback.
typedef struct {
    char path[1024];
    int64_t file_size;
    char file_hash[65];            // SHA-256 hex
    int32_t sample_rate;
    int32_t bit_depth;
    int32_t channels;
    int32_t duration_ms;
    double loudness_lufs;
    double peak_db;
    char status[16];               // "indexed", "skipped", "failed"
    char error[256];               // empty if successful
} ScannedFileResult;

/// Progress callback: indexed count, total discovered, user data.
typedef void (*ScanProgressCallback)(int64_t indexed, int64_t total, ScannedFileResult* last_file, void* userdata);

/// File watcher event types.
typedef enum {
    WATCH_EVENT_CREATED,
    WATCH_EVENT_MODIFIED,
    WATCH_EVENT_DELETED,
    WATCH_EVENT_RENAMED,
} WatchEventType;

/// A single file watch event.
typedef struct {
    WatchEventType event_type;
    char path[1024];
    char old_path[1024];           // Only for renamed events
} WatchEvent;

/// Watch event callback.
typedef void (*WatchEventCallback)(WatchEvent* event, void* userdata);

bool aria_fs_is_audio(const char* path);
bool aria_fs_is_plugin(const char* path);

// ── Scanner FFI ──────────────────────────────────────────────

/// Create a file scanner. `paths` is an array of C strings, `count` is the array length.
/// Returns an opaque pointer, or NULL on failure.
void* aria_fs_scanner_create(const char* const* paths, int32_t count);

/// Run the scanner. Calls `progress_cb` for each file with current progress.
void aria_fs_scanner_run(void* scanner, ScanProgressCallback progress_cb, void* userdata);

/// Destroy a scanner and free its resources.
void aria_fs_scanner_destroy(void* scanner);

// ── Watcher FFI ──────────────────────────────────────────────

/// Create a file watcher for the given paths.
/// Returns an opaque pointer, or NULL on failure.
void* aria_fs_watcher_create(const char* const* paths, int32_t count);

/// Start watching. Returns true if the watcher started successfully.
bool aria_fs_watcher_start(void* watcher, WatchEventCallback event_cb, void* userdata);

/// Stop the watcher.
void aria_fs_watcher_stop(void* watcher);

/// Destroy a watcher and free its resources.
void aria_fs_watcher_destroy(void* watcher);

// ── Waveform FFI ────────────────────────────────────────────

/// Generate waveform peaks and minima for an audio file.
/// @param path       Path to the audio file.
/// @param resolution Samples per pixel (max 1024).
/// @param out_peaks  Output buffer for peak values (must hold at least `resolution` floats).
/// @param out_minima Output buffer for minima values (must hold at least `resolution` floats).
/// @param out_len    Set to the number of values written (<= resolution).
/// @return true on success, false on error (file not found, not audio, etc.).
bool aria_fs_get_waveform(const char* path, uint32_t resolution,
                          float* out_peaks, float* out_minima, int32_t* out_len);

// ── Multi-Entity Search Results ─────────────────────────────

typedef struct {
    int64_t id;
    char name[256];
    char vendor[128];
    char category[64];
    char format[16];
    char features[512];
    double rank_score;
} FfiPluginResult;

typedef struct {
    int64_t id;
    char name[256];
    char file_path[1024];
    char author[128];
    double tempo;
    int32_t track_count;
    double rank_score;
} FfiProjectResult;

typedef struct {
    int64_t id;
    char name[256];
    char plugin_name[128];
    char category[64];
    char author[128];
    int32_t is_factory;
    double rank_score;
} FfiPresetResult;

/// Search plugins using FTS5.
FfiPluginResult* aria_db_search_plugins(DbConnection* conn, const char* text, const char* category,
                                        uint32_t limit, uint32_t offset, uint32_t* out_count);
/// Search projects using FTS5.
FfiProjectResult* aria_db_search_projects(DbConnection* conn, const char* text, const char* author,
                                          uint32_t limit, uint32_t offset, uint32_t* out_count);
/// Search presets using FTS5.
FfiPresetResult* aria_db_search_presets(DbConnection* conn, const char* text, const char* category,
                                        const char* plugin_name, uint32_t limit, uint32_t offset,
                                        uint32_t* out_count);
/// Free results from multi-entity search.
void aria_db_free_plugin_results(FfiPluginResult* results);
void aria_db_free_project_results(FfiProjectResult* results);
void aria_db_free_preset_results(FfiPresetResult* results);

// ── Networking ───────────────────────────────────────────────

typedef struct NetworkingContext NetworkingContext;

NetworkingContext* aria_net_create(bool enabled);
void aria_net_destroy(NetworkingContext* ctx);
bool aria_net_is_enabled(const NetworkingContext* ctx);

// ── Utilities ────────────────────────────────────────────────

const char* aria_version_string(void);
void aria_free_string(char* s);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUST_H
