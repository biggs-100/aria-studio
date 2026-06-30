// ARIA DAW — Database Integration Tests
// Catch2 v3 test suite for the SQLite FFI bridge.
// Tests DB open/close, migration, CRUD, batch insert, tag joins, and search.
//
// These tests call the Rust FFI functions via aria_rust.h.
// They require the compiled aria_bridge static library to be linked.

#include <catch2/catch_all.hpp>
#include <aria_rust.h>

#include <cstring>
#include <string>
#include <vector>

// ── Test Helpers ────────────────────────────────────────────

/// Create a sample entry struct with given path and name.
static SampleEntry make_sample_entry(const char* path, const char* name) {
    SampleEntry entry{};
    std::strncpy(entry.file_path, path, sizeof(entry.file_path) - 1);
    std::strncpy(entry.file_hash, "test_hash_00000000000000000000000000000", sizeof(entry.file_hash) - 1);
    entry.file_size = 4096;
    std::strncpy(entry.file_format, "wav", sizeof(entry.file_format) - 1);
    entry.sample_rate = 44100;
    entry.bit_depth = 24;
    entry.channels = 2;
    entry.duration_ms = 5000;
    entry.bpm = 120.0;
    entry.bpm_confidence = 0.9;
    std::strncpy(entry.musical_key, "Cm", sizeof(entry.musical_key) - 1);
    entry.key_confidence = 0.8;
    entry.loudness_lufs = -14.5;
    entry.peak_db = -3.0;
    std::strncpy(entry.category, "kick", sizeof(entry.category) - 1);
    std::strncpy(entry.name, name, sizeof(entry.name) - 1);
    entry.rating = 4;
    entry.favorite = 1;
    return entry;
}

// ── Test Cases ──────────────────────────────────────────────

TEST_CASE("Database: open and close", "[db][core]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    aria_db_close(conn);
    // Should not crash
    SUCCEED("Database opened and closed successfully");
}

TEST_CASE("Database: null path returns null", "[db][core]") {
    DbConnection* conn = aria_db_open(nullptr);
    REQUIRE(conn == nullptr);
}

TEST_CASE("Database: migration runs", "[db][migration]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);

    bool ok = aria_db_run_migrations(conn);
    REQUIRE(ok);

    int32_t version = aria_db_schema_version(conn);
    REQUIRE(version >= 1);

    aria_db_close(conn);
}

TEST_CASE("Database: migration is idempotent", "[db][migration]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);

    REQUIRE(aria_db_run_migrations(conn));
    REQUIRE(aria_db_run_migrations(conn)); // second run should also succeed

    int32_t version = aria_db_schema_version(conn);
    REQUIRE(version >= 1);

    aria_db_close(conn);
}

TEST_CASE("Database: insert and retrieve sample", "[db][crud]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    SampleEntry entry = make_sample_entry("/test/kick_01.wav", "Kick 01");

    int64_t id = aria_db_insert_sample(conn, &entry);
    REQUIRE(id > 0);

    SampleResult result{};
    bool found = aria_db_get_sample(conn, id, &result);
    REQUIRE(found);
    CHECK(result.id == id);
    CHECK(std::string(result.name) == "Kick 01");
    CHECK(result.rating == 4);
    CHECK(result.favorite == 1);

    aria_db_close(conn);
}

TEST_CASE("Database: insert duplicate path fails", "[db][crud]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    SampleEntry entry = make_sample_entry("/test/unique.wav", "Unique");
    int64_t id1 = aria_db_insert_sample(conn, &entry);
    REQUIRE(id1 > 0);

    int64_t id2 = aria_db_insert_sample(conn, &entry);
    CHECK(id2 == -1); // Should indicate failure (duplicate)

    aria_db_close(conn);
}

TEST_CASE("Database: update sample", "[db][crud]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    SampleEntry entry = make_sample_entry("/test/update_me.wav", "Before");
    int64_t id = aria_db_insert_sample(conn, &entry);
    REQUIRE(id > 0);

    SampleEntry updated = make_sample_entry("/test/update_me.wav", "After");
    updated.bpm = 140.0;
    updated.rating = 5;
    bool ok = aria_db_update_sample(conn, id, &updated);
    REQUIRE(ok);

    SampleResult result{};
    REQUIRE(aria_db_get_sample(conn, id, &result));
    CHECK(std::string(result.name) == "After");
    CHECK(result.bpm == Approx(140.0));
    CHECK(result.rating == 5);

    aria_db_close(conn);
}

TEST_CASE("Database: delete sample", "[db][crud]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    SampleEntry entry = make_sample_entry("/test/delete_me.wav", "Delete Me");
    int64_t id = aria_db_insert_sample(conn, &entry);
    REQUIRE(id > 0);

    REQUIRE(aria_db_delete_sample(conn, id));

    SampleResult result{};
    CHECK_FALSE(aria_db_get_sample(conn, id, &result)); // Should not be found

    aria_db_close(conn);
}

TEST_CASE("Database: batch insert", "[db][crud]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    std::vector<SampleEntry> entries;
    for (int i = 0; i < 10; i++) {
        char path[64];
        std::snprintf(path, sizeof(path), "/test/batch_%02d.wav", i);
        char name[64];
        std::snprintf(name, sizeof(name), "Batch %02d", i);
        entries.push_back(make_sample_entry(path, name));
    }

    int32_t count = aria_db_batch_insert_samples(conn, entries.data(), 10);
    REQUIRE(count == 10);

    int32_t total = aria_db_sample_count(conn);
    CHECK(total == 10);

    aria_db_close(conn);
}

TEST_CASE("Database: tag operations", "[db][tags]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    // Insert a sample
    SampleEntry entry = make_sample_entry("/test/tag_test.wav", "Tagged Sample");
    int64_t sample_id = aria_db_insert_sample(conn, &entry);
    REQUIRE(sample_id > 0);

    // Add tags
    int64_t dark_id = aria_db_add_tag(conn, "dark", "#000000");
    REQUIRE(dark_id > 0);
    int64_t bass_id = aria_db_add_tag(conn, "bass", nullptr);
    REQUIRE(bass_id > 0);

    // Assign tags
    REQUIRE(aria_db_tag_sample(conn, sample_id, dark_id));
    REQUIRE(aria_db_tag_sample(conn, sample_id, bass_id));

    // Retrieve tags
    TagResult tags[10]{};
    int32_t tag_count = aria_db_get_sample_tags(conn, sample_id, tags, 10);
    CHECK(tag_count == 2);

    // Untag
    REQUIRE(aria_db_untag_sample(conn, sample_id, dark_id));
    tag_count = aria_db_get_sample_tags(conn, sample_id, tags, 10);
    CHECK(tag_count == 1);
    CHECK(std::string(tags[0].name) == "bass");

    aria_db_close(conn);
}

TEST_CASE("Database: search samples with FTS5 — browse mode", "[db][search]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    // Insert test samples
    SampleEntry kick = make_sample_entry("/test/deep_kick.wav", "Deep Kick");
    kick.bpm = 120.0;
    int64_t k_id = aria_db_insert_sample(conn, &kick);
    REQUIRE(k_id > 0);

    SampleEntry snare = make_sample_entry("/test/crisp_snare.wav", "Crisp Snare");
    snare.bpm = 140.0;
    std::strncpy(snare.category, "snare", sizeof(snare.category) - 1);
    int64_t s_id = aria_db_insert_sample(conn, &snare);
    REQUIRE(s_id > 0);

    // Browse mode — empty query returns all
    DbSearchQuery search{};
    search.limit = 10;
    search.sort_by = 0;
    search.sort_ascending = 0;

    SampleResult results[10]{};
    int32_t count = aria_db_search_samples(conn, &search, results, 10);
    CHECK(count == 2);

    aria_db_close(conn);
}

TEST_CASE("Database: FTS5 text search by name", "[db][search][fts5]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    // Insert samples with distinctive names
    SampleEntry kick = make_sample_entry("/test/dark_kick.wav", "Dark Kick");
    kick.bpm = 120.0;
    REQUIRE(aria_db_insert_sample(conn, &kick) > 0);

    SampleEntry snare = make_sample_entry("/test/crisp_snare.wav", "Crisp Snare");
    snare.bpm = 140.0;
    std::strncpy(snare.category, "snare", sizeof(snare.category) - 1);
    REQUIRE(aria_db_insert_sample(conn, &snare) > 0);

    SampleEntry hat = make_sample_entry("/test/closed_hat.wav", "Closed Hat");
    hat.bpm = 130.0;
    std::strncpy(hat.category, "hat", sizeof(hat.category) - 1);
    REQUIRE(aria_db_insert_sample(conn, &hat) > 0);

    // Search for "snare" — should find only the snare sample
    DbSearchQuery search{};
    std::strncpy(search.text, "snare", sizeof(search.text) - 1);
    search.limit = 10;
    search.sort_by = 0;
    search.sort_ascending = 0;

    SampleResult results[10]{};
    int32_t count = aria_db_search_samples(conn, &search, results, 10);
    CHECK(count == 1);
    CHECK(std::string(results[0].name) == "Crisp Snare");

    // Verify BM25 rank score is populated
    CHECK(results[0].rank_score != 0.0);

    aria_db_close(conn);
}

TEST_CASE("Database: FTS5 search — no matches returns empty", "[db][search][fts5]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    SampleEntry kick = make_sample_entry("/test/kick.wav", "A Kick");
    REQUIRE(aria_db_insert_sample(conn, &kick) > 0);

    // Search for a term that doesn't exist
    DbSearchQuery search{};
    std::strncpy(search.text, "xyzzy", sizeof(search.text) - 1);
    search.limit = 10;
    search.sort_by = 0;
    search.sort_ascending = 0;

    SampleResult results[10]{};
    int32_t count = aria_db_search_samples(conn, &search, results, 10);
    CHECK(count == 0);

    aria_db_close(conn);
}

TEST_CASE("Database: search with multi-filter (category + BPM + favorite)", "[db][search][filter]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    // Kick at 120 BPM, favorited
    SampleEntry kick1 = make_sample_entry("/test/kick_120.wav", "Kick 120");
    kick1.bpm = 120.0;
    std::strncpy(kick1.category, "kick", sizeof(kick1.category) - 1);
    kick1.favorite = 1;
    REQUIRE(aria_db_insert_sample(conn, &kick1) > 0);

    // Kick at 140 BPM, not favorited
    SampleEntry kick2 = make_sample_entry("/test/kick_140.wav", "Kick 140");
    kick2.bpm = 140.0;
    std::strncpy(kick2.category, "kick", sizeof(kick2.category) - 1);
    kick2.favorite = 0;
    REQUIRE(aria_db_insert_sample(conn, &kick2) > 0);

    // Snare at 120 BPM, favorited (wrong category)
    SampleEntry snare = make_sample_entry("/test/snare_120.wav", "Snare 120");
    snare.bpm = 120.0;
    std::strncpy(snare.category, "snare", sizeof(snare.category) - 1);
    snare.favorite = 1;
    REQUIRE(aria_db_insert_sample(conn, &snare) > 0);

    // Filter: category=kick, bpm 110-130, favorite=true
    DbSearchQuery search{};
    search.limit = 10;
    std::strncpy(search.category, "kick", sizeof(search.category) - 1);
    search.bpm_min = 110.0;
    search.bpm_max = 130.0;
    search.favorite_only = 1;
    search.sort_by = 0;
    search.sort_ascending = 0;

    SampleResult results[10]{};
    int32_t count = aria_db_search_samples(conn, &search, results, 10);
    CHECK(count == 1);
    CHECK(std::string(results[0].name) == "Kick 120");

    aria_db_close(conn);
}

TEST_CASE("Database: search sort by BPM ascending", "[db][search][sort]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    SampleEntry fast = make_sample_entry("/test/fast.wav", "Fast");
    fast.bpm = 160.0;
    fast.file_hash[0] = 'f';
    REQUIRE(aria_db_insert_sample(conn, &fast) > 0);

    SampleEntry medium = make_sample_entry("/test/medium.wav", "Medium");
    medium.bpm = 120.0;
    medium.file_hash[0] = 'm';
    REQUIRE(aria_db_insert_sample(conn, &medium) > 0);

    SampleEntry slow = make_sample_entry("/test/slow.wav", "Slow");
    slow.bpm = 80.0;
    slow.file_hash[0] = 's';
    REQUIRE(aria_db_insert_sample(conn, &slow) > 0);

    // Sort by BPM ascending
    DbSearchQuery search{};
    search.limit = 10;
    search.sort_by = 3;  // BPM
    search.sort_ascending = 1;

    SampleResult results[10]{};
    int32_t count = aria_db_search_samples(conn, &search, results, 10);
    CHECK(count == 3);
    CHECK(results[0].bpm == Approx(80.0));
    CHECK(results[1].bpm == Approx(120.0));
    CHECK(results[2].bpm == Approx(160.0));

    aria_db_close(conn);
}

TEST_CASE("Database: search sort by name ascending", "[db][search][sort]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    // Insert in reverse alphabetical order
    SampleEntry charlie = make_sample_entry("/test/c_sample.wav", "Charlie");
    charlie.file_hash[0] = 'c';
    REQUIRE(aria_db_insert_sample(conn, &charlie) > 0);

    SampleEntry bravo = make_sample_entry("/test/b_sample.wav", "Bravo");
    bravo.file_hash[0] = 'b';
    REQUIRE(aria_db_insert_sample(conn, &bravo) > 0);

    SampleEntry alpha = make_sample_entry("/test/a_sample.wav", "Alpha");
    alpha.file_hash[0] = 'a';
    REQUIRE(aria_db_insert_sample(conn, &alpha) > 0);

    // Sort by name ascending
    DbSearchQuery search{};
    search.limit = 10;
    search.sort_by = 1;  // Name
    search.sort_ascending = 1;

    SampleResult results[10]{};
    int32_t count = aria_db_search_samples(conn, &search, results, 10);
    CHECK(count == 3);
    CHECK(std::string(results[0].name) == "Alpha");
    CHECK(std::string(results[1].name) == "Bravo");
    CHECK(std::string(results[2].name) == "Charlie");

    aria_db_close(conn);
}

TEST_CASE("Database: search special characters sanitized", "[db][search][fts5]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    SampleEntry entry = make_sample_entry("/test/vocal_fx.wav", "Vocal FX");
    REQUIRE(aria_db_insert_sample(conn, &entry) > 0);

    // Search with special characters — should not crash
    DbSearchQuery search{};
    std::strncpy(search.text, "vocal + fx", sizeof(search.text) - 1);
    search.limit = 10;
    search.sort_by = 0;
    search.sort_ascending = 0;

    SampleResult results[10]{};
    int32_t count = aria_db_search_samples(conn, &search, results, 10);
    CHECK(count > 0);
    CHECK(std::string(results[0].name) == "Vocal FX");

    aria_db_close(conn);
}

TEST_CASE("Database: search pagination with limit/offset", "[db][search]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    // Insert 10 samples
    for (int i = 0; i < 10; i++) {
        char path[64];
        std::snprintf(path, sizeof(path), "/test/page_%02d.wav", i);
        char name[64];
        std::snprintf(name, sizeof(name), "Page %02d", i);
        SampleEntry entry = make_sample_entry(path, name);
        entry.file_hash[0] = static_cast<char>('a' + i);
        REQUIRE(aria_db_insert_sample(conn, &entry) > 0);
    }

    // First page: limit 3, offset 0
    DbSearchQuery search{};
    search.limit = 3;
    search.offset = 0;
    search.sort_by = 1;  // Name ascending — deterministic order
    search.sort_ascending = 1;

    SampleResult page1[3]{};
    int32_t page1_count = aria_db_search_samples(conn, &search, page1, 3);
    CHECK(page1_count == 3);

    // Second page: limit 3, offset 3
    search.offset = 3;
    SampleResult page2[3]{};
    int32_t page2_count = aria_db_search_samples(conn, &search, page2, 3);
    CHECK(page2_count == 3);

    // Verify pages have different results
    CHECK(std::string(page1[0].name) != std::string(page2[0].name));

    aria_db_close(conn);
}

TEST_CASE("Database: maintenance operations", "[db][maintenance]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    // VACUUM (no-op on in-memory but should not crash)
    CHECK(aria_db_vacuum(conn));

    // Integrity check
    char integrity_buf[4096]{};
    aria_db_integrity_check(conn, integrity_buf, sizeof(integrity_buf));
    CHECK(std::string(integrity_buf).empty());

    // ANALYZE
    CHECK(aria_db_analyze(conn));

    aria_db_close(conn);
}

TEST_CASE("Database: watched folders", "[db][folders]") {
    DbConnection* conn = aria_db_open(":memory:");
    REQUIRE(conn != nullptr);
    REQUIRE(aria_db_run_migrations(conn));

    REQUIRE(aria_db_add_watched_folder(conn, "/music/samples", 1));
    REQUIRE(aria_db_add_watched_folder(conn, "/music/loops", 0));
    REQUIRE(aria_db_remove_watched_folder(conn, "/music/samples"));

    aria_db_close(conn);
}
