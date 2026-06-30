// ARIA DAW — File Watcher Tests (Tasks 5.6, 5.7, 5.8, 5.9)
//
// TDD: RED — written before FileWatcher implementation.
// Tests use real filesystem operations with temp directories.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "graphics/file_watcher.h"

using namespace aria;
namespace fs = std::filesystem;

// ─── Helpers ────────────────────────────────────────────────────

/// Create a unique temp directory for each test.
struct TempDir {
    fs::path path;

    TempDir() {
        auto tmp = fs::temp_directory_path();
        path = tmp / ("aria_fw_test_" + std::to_string(rand()));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    std::string str() const { return path.string(); }

    void write_file(const std::string& name, const std::string& content) {
        std::ofstream ofs(path / name);
        ofs << content;
        ofs.close();
    }

    void append_file(const std::string& name, const std::string& content) {
        std::ofstream ofs(path / name, std::ios::app);
        ofs << content;
        ofs.close();
    }

    void delete_file(const std::string& name) {
        std::error_code ec;
        fs::remove(path / name, ec);
    }

    bool file_exists(const std::string& name) const {
        return fs::exists(path / name);
    }
};

/// Wait for a condition with timeout.
template <typename F>
bool wait_for(F&& predicate, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

// ─── Task 5.6: .lua save → callback fires within 500ms ─────────

TEST_CASE("FileWatcher fires callback on .lua file modification",
           "[scripting][file_watcher][hot_reload]") {
    TempDir tmp;

    std::atomic<int> callback_count{0};
    std::string last_file;

    FileWatcher watcher;
    watcher.set_debounce(".lua", std::chrono::milliseconds(50)); // Speed up test
    REQUIRE(watcher.watch_directory(tmp.str(), [&](const std::string& fname) {
        callback_count++;
        last_file = fname;
    }));

    // Give the watcher thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Write a .lua file
    tmp.write_file("test_script.lua", "-- initial content\n");

    // Wait for the callback
    bool fired = wait_for([&]() { return callback_count > 0; },
                          std::chrono::milliseconds(1500));

    CHECK(fired);
    if (fired) {
        CHECK(last_file == "test_script.lua");
    }

    watcher.stop();
}

// ─── Task 5.7: Rapid 5 saves → single reload after debounce ────

TEST_CASE("FileWatcher debounces rapid .lua saves",
           "[scripting][file_watcher][debounce]") {
    TempDir tmp;

    std::atomic<int> callback_count{0};

    FileWatcher watcher;
    watcher.set_debounce(".lua", std::chrono::milliseconds(200));
    REQUIRE(watcher.watch_directory(tmp.str(), [&](const std::string&) {
        callback_count++;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Write 5 times in rapid succession (total < 1 second)
    for (int i = 0; i < 5; ++i) {
        tmp.write_file("rapid.lua", "-- content " + std::to_string(i) + "\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Wait a bit for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // The debounce should have coalesced the events.
    // Due to real filesystem behavior, we may get 1-3 callbacks
    // depending on timing. The key is that we get FEWER than 5.
    CHECK(callback_count < 5);
    CHECK(callback_count > 0);

    watcher.stop();
}

// ─── Task 5.8: New .lua file → event dispatched, not auto-loaded ─

TEST_CASE("FileWatcher dispatches event for new .lua file",
           "[scripting][file_watcher][creation]") {
    TempDir tmp;

    std::atomic<int> callback_count{0};
    std::string last_file;

    FileWatcher watcher;
    watcher.set_debounce(".lua", std::chrono::milliseconds(50));
    REQUIRE(watcher.watch_directory(tmp.str(), [&](const std::string& fname) {
        callback_count++;
        last_file = fname;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Create a new .lua file
    tmp.write_file("new_script.lua", "-- new script\n");

    // Wait for event
    bool fired = wait_for([&]() { return callback_count > 0; },
                          std::chrono::milliseconds(1500));

    CHECK(fired);
    if (fired) {
        // The watcher dispatches the event — the consumer decides
        // whether to auto-load. The watcher itself just notifies.
        CHECK(last_file == "new_script.lua");
    }

    watcher.stop();
}

// ─── Task 5.9: .lua file deleted → warning expected ────────────

TEST_CASE("FileWatcher dispatches event on .lua file deletion",
           "[scripting][file_watcher][deletion]") {
    TempDir tmp;

    std::atomic<int> callback_count{0};
    std::string last_file;

    // Create a file first
    tmp.write_file("running_script.lua", "-- running script\n");

    FileWatcher watcher;
    watcher.set_debounce(".lua", std::chrono::milliseconds(50));
    REQUIRE(watcher.watch_directory(tmp.str(), [&](const std::string& fname) {
        callback_count++;
        last_file = fname;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Delete the file
    tmp.delete_file("running_script.lua");

    // Wait for event
    bool fired = wait_for([&]() { return callback_count > 0; },
                          std::chrono::milliseconds(1500));

    CHECK(fired);
    if (fired) {
        CHECK(last_file == "running_script.lua");
    }

    watcher.stop();
}

// ─── Task 5.4: Debounce per file type ───────────────────────────

TEST_CASE("FileWatcher debounce differs per file extension",
           "[scripting][file_watcher][config]") {
    FileWatcher watcher;

    SECTION("default debounce for .lua is 200ms") {
        CHECK(watcher.get_debounce(".lua") == std::chrono::milliseconds(200));
    }

    SECTION("default debounce for .wav is 2000ms") {
        CHECK(watcher.get_debounce(".wav") == std::chrono::milliseconds(2000));
    }

    SECTION("default debounce for unknown ext is 2000ms") {
        CHECK(watcher.get_debounce(".xyz") == std::chrono::milliseconds(2000));
    }

    SECTION("can override debounce per extension") {
        watcher.set_debounce(".wav", std::chrono::milliseconds(500));
        CHECK(watcher.get_debounce(".wav") == std::chrono::milliseconds(500));
    }

    SECTION("override doesn't affect other extensions") {
        watcher.set_debounce(".wav", std::chrono::milliseconds(500));
        CHECK(watcher.get_debounce(".lua") == std::chrono::milliseconds(200));
    }
}

// ─── File watcher lifecycle ─────────────────────────────────────

TEST_CASE("FileWatcher lifecycle", "[scripting][file_watcher][lifecycle]") {
    TempDir tmp;

    SECTION("not watching initially") {
        FileWatcher watcher;
        CHECK_FALSE(watcher.is_watching());
    }

    SECTION("watching after start") {
        FileWatcher watcher;
        CHECK(watcher.watch_directory(tmp.str(), [](const std::string&) {}));
        // Give it a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK(watcher.is_watching());
        watcher.stop();
    }

    SECTION("stop actually stops") {
        FileWatcher watcher;
        watcher.watch_directory(tmp.str(), [](const std::string&) {});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        watcher.stop();
        CHECK_FALSE(watcher.is_watching());
    }

    SECTION("can watch directory with non-existent path (will poll)") {
        FileWatcher watcher;
        // Non-existent path — should still return true (we try)
        CHECK(watcher.watch_directory("C:\\nonexistent_dir_xyz\\",
                                       [](const std::string&) {}));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        watcher.stop();
    }

    SECTION("directory path is stored") {
        FileWatcher watcher;
        watcher.watch_directory(tmp.str(), [](const std::string&) {});
        CHECK(watcher.watched_directory() == tmp.str());
        watcher.stop();
    }
}
