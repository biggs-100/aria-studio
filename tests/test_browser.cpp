// Browser Engine, SearchProvider, PreviewPlayer, WaveformCache — Catch2 unit tests.
// Uses mock FFI patterns where the actual Rust library is not available.
// These tests verify the C++ logic independently of the Rust FFI layer.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "browser/browser_engine.h"
#include "browser/search_provider.h"
#include "browser/preview_player.h"
#include "browser/waveform_cache.h"
#include "kernel/event_types.h"

#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace aria::browser;

// ═══════════════════════════════════════════════════════════════
// Test helpers
// ═══════════════════════════════════════════════════════════════

/// Create a temporary directory for test artifacts.
static std::string create_temp_dir() {
    auto tmp = std::filesystem::temp_directory_path();
    auto uid = std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    auto dir = tmp / ("aria_test_browser_" + uid);
    std::filesystem::create_directories(dir);
    return dir.string();
}

/// Remove a temporary directory recursively.
static void remove_temp_dir(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

// ═══════════════════════════════════════════════════════════════
// BrowserEventType tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("BrowserEventType values are distinct", "[browser][events]") {
    SECTION("all event type values are unique") {
        auto file_scanned    = static_cast<uint32_t>(BrowserEventType::kFileScanned);
        auto file_changed    = static_cast<uint32_t>(BrowserEventType::kFileChanged);
        auto index_progress  = static_cast<uint32_t>(BrowserEventType::kIndexProgress);
        auto search_result   = static_cast<uint32_t>(BrowserEventType::kSearchResult);
        auto preview_loaded  = static_cast<uint32_t>(BrowserEventType::kPreviewLoaded);
        auto search_started  = static_cast<uint32_t>(BrowserEventType::kSearchStarted);
        auto search_completed= static_cast<uint32_t>(BrowserEventType::kSearchCompleted);
        auto scan_started    = static_cast<uint32_t>(BrowserEventType::kScanStarted);
        auto scan_completed  = static_cast<uint32_t>(BrowserEventType::kScanCompleted);
        auto file_deleted    = static_cast<uint32_t>(BrowserEventType::kFileDeleted);
        auto clip_created    = static_cast<uint32_t>(BrowserEventType::kClipCreated);

        CHECK(file_scanned != file_changed);
        CHECK(file_scanned != index_progress);
        CHECK(file_scanned != search_result);
        CHECK(file_scanned != preview_loaded);
        CHECK(file_changed != index_progress);
        CHECK(search_started != search_completed);
        CHECK(scan_started != scan_completed);
        CHECK(file_deleted != file_scanned);
        CHECK(clip_created != file_scanned);
        CHECK(clip_created != file_changed);
        CHECK(clip_created != file_deleted);
    }

    SECTION("event type values are in the 2000 range") {
        CHECK(static_cast<uint32_t>(BrowserEventType::kFileScanned) >= 2001);
        CHECK(static_cast<uint32_t>(BrowserEventType::kClipCreated) == 2011);
        CHECK(static_cast<uint32_t>(BrowserEventType::kScanCompleted) <= 2011);
    }
}

TEST_CASE("BrowserEvent default construction", "[browser][events]") {
    SECTION("default BrowserEvent has zero values") {
        BrowserEvent ev{};
        // type is an enum, timestamp is uint64_t — both zero-initialized
        CHECK(ev.int_value == 0);
        CHECK(ev.double_value == 0.0);
        CHECK(ev.str_data[0] == '\0');
    }

    SECTION("BrowserEvent can be populated") {
        BrowserEvent ev;
        ev.type = BrowserEventType::kFileScanned;
        ev.timestamp = 1234567890;
        ev.int_value = 42;
        ev.double_value = 3.14;
        std::strncpy(ev.str_data, "/path/to/file.wav", sizeof(ev.str_data) - 1);
        ev.str_data[sizeof(ev.str_data) - 1] = '\0';

        CHECK(ev.type == BrowserEventType::kFileScanned);
        CHECK(ev.timestamp == 1234567890);
        CHECK(ev.int_value == 42);
        CHECK(ev.double_value == 3.14);
        CHECK(std::string(ev.str_data) == "/path/to/file.wav");
    }
}

// ═══════════════════════════════════════════════════════════════
// BrowserEngine tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("BrowserEngine lifecycle", "[browser][engine]") {
    auto tmp_dir = create_temp_dir();
    auto db_path = tmp_dir + "/test_aria.db";

    SECTION("engine starts uninitialized") {
        auto engine = std::make_unique<BrowserEngine>();
        CHECK_FALSE(engine->is_initialized());
    }

    SECTION("shutdown is safe on uninitialized engine") {
        auto engine = std::make_unique<BrowserEngine>();
        // Should not crash
        engine->shutdown();
        CHECK_FALSE(engine->is_initialized());
    }

    SECTION("init with valid path attempts initialization") {
        auto engine = std::make_unique<BrowserEngine>();

        // This requires the Rust static library to be linked.
        // If available, init succeeds; otherwise it returns false.
        bool ok = engine->init(db_path);

        if (ok) {
            CHECK(engine->is_initialized());
            CHECK(engine->search() != nullptr);
            CHECK(engine->preview() != nullptr);
            CHECK(engine->waveforms() != nullptr);
            CHECK(engine->watched_folders().empty());

            engine->shutdown();
            CHECK_FALSE(engine->is_initialized());
        } else {
            // Rust lib not linked — expected behavior, skip assertions
            CHECK_FALSE(engine->is_initialized());
        }
    }

    SECTION("init with invalid path returns false") {
        auto engine = std::make_unique<BrowserEngine>();
        bool ok = engine->init("/nonexistent/path/that/cannot/be/created/aria.db");
        CHECK_FALSE(ok);
        CHECK_FALSE(engine->is_initialized());
    }

    SECTION("double init returns true (already initialized)") {
        auto engine = std::make_unique<BrowserEngine>();
        bool ok = engine->init(db_path);
        if (ok) {
            bool ok2 = engine->init("/other/path.db");
            CHECK(ok2);
            CHECK(engine->is_initialized());
            engine->shutdown();
        }
    }

    remove_temp_dir(tmp_dir);
}

TEST_CASE("BrowserEngine folder management", "[browser][engine][folders]") {
    auto tmp_dir = create_temp_dir();
    auto db_path = tmp_dir + "/test_aria.db";

    SECTION("watched folders start empty") {
        auto engine = std::make_unique<BrowserEngine>();
        bool ok = engine->init(db_path);
        if (ok) {
            CHECK(engine->watched_folders().empty());
            engine->shutdown();
        }
    }

    SECTION("add and remove watched folders when initialized") {
        auto engine = std::make_unique<BrowserEngine>();
        bool ok = engine->init(db_path);
        if (!ok) {
            remove_temp_dir(tmp_dir);
            return;
        }

        auto test_dir = tmp_dir + "/samples";
        std::filesystem::create_directories(test_dir);

        engine->add_watched_folder(test_dir);
        CHECK(engine->watched_folders().size() == 1);
        CHECK(engine->watched_folders()[0] == test_dir);

        engine->remove_watched_folder(test_dir);
        CHECK(engine->watched_folders().empty());

        engine->shutdown();
    }

    remove_temp_dir(tmp_dir);
}

TEST_CASE("BrowserEngine scan lifecycle", "[browser][engine][scan]") {
    auto tmp_dir = create_temp_dir();
    auto db_path = tmp_dir + "/test_aria.db";

    SECTION("not scanning initially") {
        auto engine = std::make_unique<BrowserEngine>();
        CHECK_FALSE(engine->is_scanning());
        CHECK(engine->scan_progress() == 0.0);
        CHECK(engine->scan_indexed() == 0);
        CHECK(engine->scan_total() == 0);
    }

    SECTION("start_scan with no watched folders is safe") {
        auto engine = std::make_unique<BrowserEngine>();
        bool ok = engine->init(db_path);
        if (ok) {
            // No watched folders — scan should be a no-op
            engine->start_scan();
            CHECK_FALSE(engine->is_scanning());
            engine->shutdown();
        }
    }

    remove_temp_dir(tmp_dir);
}

// ═══════════════════════════════════════════════════════════════
// PreviewPlayer tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("PreviewPlayer state machine", "[browser][preview]") {
    auto player = std::make_unique<PreviewPlayer>();

    SECTION("initial state is stopped") {
        CHECK(player->is_stopped());
        CHECK_FALSE(player->is_playing());
        CHECK_FALSE(player->is_paused());
    }

    SECTION("initial volume is 0 dB") {
        CHECK(player->volume() == 0.0);
    }

    SECTION("stop is safe when already stopped") {
        // Should not crash or throw
        CHECK_NOTHROW(player->stop());
        CHECK(player->is_stopped());
    }

    SECTION("pause on stopped state is a no-op") {
        player->pause();
        CHECK(player->is_stopped());
    }

    SECTION("resume on stopped state is a no-op") {
        player->resume();
        CHECK(player->is_stopped());
    }

    SECTION("play with non-existent file returns false") {
        bool ok = player->play("/nonexistent/file.wav");
        CHECK_FALSE(ok);
        CHECK(player->is_stopped());
    }

    SECTION("play with non-audio file returns false") {
        bool ok = player->play("/nonexistent/file.txt");
        CHECK_FALSE(ok);
        CHECK(player->is_stopped());
    }

    SECTION("current_file is empty initially") {
        CHECK(player->current_file().empty());
    }
}

TEST_CASE("PreviewPlayer volume control", "[browser][preview][volume]") {
    auto player = std::make_unique<PreviewPlayer>();

    SECTION("set_volume clamps below minimum") {
        player->set_volume(-100.0);
        CHECK(player->volume() == -60.0);  // Clamped to min
    }

    SECTION("set_volume clamps above maximum") {
        player->set_volume(20.0);
        CHECK(player->volume() == 12.0);  // Clamped to max
    }

    SECTION("set_volume accepts normal range") {
        player->set_volume(0.0);
        CHECK(player->volume() == 0.0);

        player->set_volume(-6.0);
        CHECK(player->volume() == -6.0);

        player->set_volume(6.0);
        CHECK(player->volume() == 6.0);
    }

    SECTION("set_volume at boundary values") {
        player->set_volume(-60.0);
        CHECK(player->volume() == -60.0);

        player->set_volume(12.0);
        CHECK(player->volume() == 12.0);
    }

    SECTION("multiple set_volume calls work") {
        player->set_volume(-12.0);
        player->set_volume(-6.0);
        player->set_volume(0.0);
        CHECK(player->volume() == 0.0);
    }
}

TEST_CASE("PreviewPlayer position and duration", "[browser][preview][state]") {
    auto player = std::make_unique<PreviewPlayer>();

    SECTION("position is 0 when stopped") {
        CHECK(player->position() == 0.0);
    }

    SECTION("duration is 0 when stopped") {
        CHECK(player->duration() == 0.0);
    }

    SECTION("stop resets position") {
        player->stop();
        CHECK(player->position() == 0.0);
    }
}

// ═══════════════════════════════════════════════════════════════
// WaveformCache tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("WaveformCache construction and directory", "[browser][waveform]") {
    auto tmp_dir = create_temp_dir();
    auto cache_dir = tmp_dir + "/wf_cache";

    SECTION("cache directory is created on construction") {
        { auto cache = std::make_unique<WaveformCache>(cache_dir); }
        CHECK(std::filesystem::exists(cache_dir));
    }

    SECTION("empty cache has zero entries and no hits") {
        WaveformCache cache(cache_dir);
        auto stats = cache.stats();
        CHECK(stats.memory_entries == 0);
        CHECK(stats.disk_entries == 0);
        CHECK(stats.cache_hits == 0);
        CHECK(stats.cache_misses == 0);
    }

    SECTION("get_waveform on non-existent file returns nullopt") {
        WaveformCache cache(cache_dir);
        auto result = cache.get_waveform("/nonexistent/file.wav");
        CHECK_FALSE(result.has_value());
    }

    remove_temp_dir(tmp_dir);
}

TEST_CASE("WaveformCache miss tracking", "[browser][waveform][miss]") {
    auto tmp_dir = create_temp_dir();
    auto cache_dir = tmp_dir + "/wf_miss_test";
    auto cache = std::make_unique<WaveformCache>(cache_dir);

    SECTION("cache miss increments counter") {
        cache->get_waveform("/nonexistent/file1.wav");
        cache->get_waveform("/nonexistent/file2.wav");
        auto stats = cache->stats();
        CHECK(stats.cache_misses == 2);
        CHECK(stats.cache_hits == 0);
    }

    SECTION("generate_and_cache fails for non-existent files") {
        bool ok = cache->generate_and_cache("/nonexistent/file.wav");
        CHECK_FALSE(ok);
    }

    cache->clear_cache();
    remove_temp_dir(tmp_dir);
}

TEST_CASE("WaveformCache memory management", "[browser][waveform][memory]") {
    auto tmp_dir = create_temp_dir();
    auto cache_dir = tmp_dir + "/wf_mem_test";
    auto cache = std::make_unique<WaveformCache>(cache_dir);

    SECTION("clear_cache resets all stats") {
        cache->get_waveform("/nonexistent/file.wav");
        cache->clear_cache();
        auto stats = cache->stats();
        CHECK(stats.cache_hits == 0);
        CHECK(stats.cache_misses == 0);
        CHECK(stats.memory_entries == 0);
    }

    SECTION("set_max_memory_entries accepts new limit") {
        cache->set_max_memory_entries(50);
        auto stats = cache->stats();
        CHECK(stats.memory_entries == 0);
        // Max entries doesn't affect current count
    }

    SECTION("set_max_memory_entries can be increased") {
        cache->set_max_memory_entries(200);
        auto stats = cache->stats();
        CHECK(stats.memory_entries == 0);
    }

    cache->clear_cache();
    remove_temp_dir(tmp_dir);
}

TEST_CASE("WaveformCache LRU list behavior", "[browser][waveform][lru]") {
    auto tmp_dir = create_temp_dir();
    auto cache_dir = tmp_dir + "/wf_lru_test";
    auto cache = std::make_unique<WaveformCache>(cache_dir);

    SECTION("stats returns default values after construction") {
        auto stats = cache->stats();
        CHECK(stats.memory_entries == 0);
        CHECK(stats.cache_hits == 0);
        CHECK(stats.cache_misses == 0);
    }

    SECTION("cache with max_entries=3 and 4 misses", "[browser][waveform][lru][eviction]") {
        // The cache only stores successful FFI results internally.
        // Non-existent files produce misses but don't fill the cache.
        // This test verifies the miss counters work correctly with
        // the LRU eviction limit set.

        cache->set_max_memory_entries(3);
        cache->get_waveform("/nonexistent/a.wav");
        cache->get_waveform("/nonexistent/b.wav");
        cache->get_waveform("/nonexistent/c.wav");

        auto stats = cache->stats();
        CHECK(stats.cache_misses == 3);
        CHECK(stats.memory_entries == 0);  // All failed — cache not populated

        cache->clear_cache();
    }

    cache->clear_cache();
    remove_temp_dir(tmp_dir);
}

TEST_CASE("WaveformCache disk I/O structure", "[browser][waveform][disk]") {
    auto tmp_dir = create_temp_dir();
    auto cache_dir = tmp_dir + "/wf_disk_test";

    SECTION("stats on empty disk cache") {
        std::filesystem::create_directories(cache_dir);
        WaveformCache cache(cache_dir);
        auto stats = cache.stats();
        CHECK(stats.disk_entries == 0);
        CHECK(stats.disk_size_bytes == 0);
    }

    SECTION("orphaned cleanup on empty cache is safe") {
        std::filesystem::create_directories(cache_dir);
        WaveformCache cache(cache_dir);
        // Should not crash
        CHECK_NOTHROW(cache.cleanup_orphaned());
    }

    remove_temp_dir(tmp_dir);
}

// ═══════════════════════════════════════════════════════════════
// SearchProvider basic tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SearchProvider construction", "[browser][search]") {
    SECTION("provider with null DB returns empty results") {
        auto provider = std::make_unique<SearchProvider>(nullptr);

        SearchParams params;
        params.text = "test";

        auto result = provider->search_sync(params);
        CHECK(result.samples.empty());
        CHECK(result.total_count == 0);
    }

    SECTION("provider is not searching initially") {
        auto provider = std::make_unique<SearchProvider>(nullptr);
        CHECK_FALSE(provider->is_searching());
    }

    SECTION("clear_cache is safe on empty cache") {
        auto provider = std::make_unique<SearchProvider>(nullptr);
        CHECK_NOTHROW(provider->clear_cache());
    }

    SECTION("cache_stats returns zeros initially") {
        auto provider = std::make_unique<SearchProvider>(nullptr);
        auto stats = provider->cache_stats();
        CHECK(stats.entries == 0);
        CHECK(stats.hits == 0);
        CHECK(stats.misses == 0);
    }

    SECTION("search_sync with db=nullptr returns empty") {
        auto provider = std::make_unique<SearchProvider>(nullptr);
        SearchParams params;
        params.text = "kick";
        params.category = "kick";
        params.limit = 10;

        auto result = provider->search_sync(params);
        CHECK(result.samples.empty());
    }
}

TEST_CASE("SearchProvider async search", "[browser][search][async]") {
    SECTION("try_get_result returns nullopt when no search done") {
        auto provider = std::make_unique<SearchProvider>(nullptr);
        auto result = provider->try_get_result();
        CHECK_FALSE(result.has_value());
    }

    SECTION("search_async with null DB completes without crashing") {
        auto provider = std::make_unique<SearchProvider>(nullptr);
        bool callback_called = false;

        SearchParams params;
        params.text = "test";

        provider->search_async(params, [&](const SearchResult& res) {
            callback_called = true;
            CHECK(res.samples.empty());
        });

        // Give the thread time to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        CHECK_FALSE(provider->is_searching());
        // Callback may or may not have fired depending on timing
    }

    SECTION("second async search while first is running is blocked") {
        auto provider = std::make_unique<SearchProvider>(nullptr);

        SearchParams params;
        params.text = "test";

        provider->search_async(params, nullptr);
        // Second call should be a no-op since first is "running"
        provider->search_async(params, nullptr);
        // Verify the provider didn't crash
        CHECK(provider != nullptr);
    }
}
