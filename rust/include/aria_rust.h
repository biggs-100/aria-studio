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

DbConnection* aria_db_open(const char* path);
void aria_db_close(DbConnection* conn);
bool aria_db_run_migrations(DbConnection* conn);

typedef struct {
    int64_t id;
    char file_path[1024];
    char name[256];
    char category[64];
    double bpm;
    int32_t rating;
} SampleResult;

int32_t aria_db_search_samples(
    DbConnection* conn,
    const char* query,
    SampleResult* results,
    int32_t max_results
);

// ── File System ──────────────────────────────────────────────

typedef struct FileInfo {
    char path[1024];
    int64_t file_size;
    int64_t modified_at;
    bool is_directory;
    char extension[16];
} FileInfo;

bool aria_fs_is_audio(const char* path);
bool aria_fs_is_plugin(const char* path);

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
