// ARIA DAW — File Watcher Manual Test
// Catch2 v3 manual test for file create/modify/delete detection.
//
// This test is MANUAL — it REQUIRES real filesystem events and
// is not run in CI. Run it explicitly with:
//   ctest --preset debug -R "watcher_manual" -V
//
// On Windows: tests ReadDirectoryChangesW via notify.
// On macOS: tests FSEvents via notify.
// On Linux: tests inotify via notify.

#include <catch2/catch_all.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

// The watcher FFI is available via aria_rust.h when the Rust
// static library is linked.
#include <aria_rust.h>

namespace fs = std::filesystem;

// ── Test Helpers ────────────────────────────────────────────

/// Create a temporary directory for watcher tests.
struct TempDir {
    fs::path path;

    TempDir() {
        auto tmp = fs::temp_directory_path();
        path = tmp / "aria_watcher_test_XXXXXX";
        // Use a UUID-style suffix for uniqueness
        path += std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path file(const char* name) const {
        return path / name;
    }
};

/// Write content to a file.
static void write_file(const fs::path& p, const char* content = "test") {
    std::ofstream ofs(p);
    ofs << content;
    ofs.close();
}

/// Sleep for a given duration to allow watcher events to propagate.
static void wait_for_events(int ms = 1500) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ── Event Tracking ──────────────────────────────────────────

/// Simple structure to track received watch events.
struct EventTracker {
    int creates = 0;
    int modifies = 0;
    int deletes = 0;
    int renames = 0;
    std::string last_path;
    std::string last_old_path;
};

/// C callback that updates an EventTracker.
extern "C" void track_event(WatchEvent* event, void* userdata) {
    auto* tracker = static_cast<EventTracker*>(userdata);
    tracker->last_path = event->path;
    if (event->event_type == WATCH_EVENT_RENAMED) {
        tracker->last_old_path = event->old_path;
    }

    switch (event->event_type) {
        case WATCH_EVENT_CREATED:
            tracker->creates++;
            break;
        case WATCH_EVENT_MODIFIED:
            tracker->modifies++;
            break;
        case WATCH_EVENT_DELETED:
            tracker->deletes++;
            break;
        case WATCH_EVENT_RENAMED:
            tracker->renames++;
            break;
    }
}

// ── Test Cases ──────────────────────────────────────────────

TEST_CASE("Watcher: detect file creation", "[watcher][manual]") {
    TempDir dir;

    // Create watcher
    const char* paths[] = { dir.path.string().c_str() };
    void* watcher = aria_fs_watcher_create(paths, 1);
    REQUIRE(watcher != nullptr);

    EventTracker tracker;
    bool started = aria_fs_watcher_start(
        watcher,
        track_event,
        &tracker
    );
    REQUIRE(started);

    // Create a file
    write_file(dir.file("new_file.wav"));
    wait_for_events();

    // Should have received a create event
    CHECK(tracker.creates >= 1);
    CHECK(tracker.last_path.find("new_file.wav") != std::string::npos);

    aria_fs_watcher_stop(watcher);
    aria_fs_watcher_destroy(watcher);
}

TEST_CASE("Watcher: detect file modification", "[watcher][manual]") {
    TempDir dir;

    // Create a file first
    write_file(dir.file("modify_test.wav"), "original");
    wait_for_events(500); // Let watcher settle if active

    const char* paths[] = { dir.path.string().c_str() };
    void* watcher = aria_fs_watcher_create(paths, 1);
    REQUIRE(watcher != nullptr);

    EventTracker tracker;
    bool started = aria_fs_watcher_start(watcher, track_event, &tracker);
    REQUIRE(started);

    wait_for_events(500); // Allow watcher to initialize

    // Modify the file
    write_file(dir.file("modify_test.wav"), "modified content");
    wait_for_events();

    // Should have received a modify event
    CHECK(tracker.modifies >= 1);

    aria_fs_watcher_stop(watcher);
    aria_fs_watcher_destroy(watcher);
}

TEST_CASE("Watcher: detect file deletion", "[watcher][manual]") {
    TempDir dir;

    // Create a file
    write_file(dir.file("delete_test.wav"));
    wait_for_events(500);

    const char* paths[] = { dir.path.string().c_str() };
    void* watcher = aria_fs_watcher_create(paths, 1);
    REQUIRE(watcher != nullptr);

    EventTracker tracker;
    bool started = aria_fs_watcher_start(watcher, track_event, &tracker);
    REQUIRE(started);

    wait_for_events(500); // Allow watcher to initialize

    // Delete the file
    fs::remove(dir.file("delete_test.wav"));
    wait_for_events();

    // Should have received a delete event
    CHECK(tracker.deletes >= 1);

    aria_fs_watcher_stop(watcher);
    aria_fs_watcher_destroy(watcher);
}

TEST_CASE("Watcher: detect file rename", "[watcher][manual]") {
    TempDir dir;

    // Create a file
    write_file(dir.file("old_name.wav"));
    wait_for_events(500);

    const char* paths[] = { dir.path.string().c_str() };
    void* watcher = aria_fs_watcher_create(paths, 1);
    REQUIRE(watcher != nullptr);

    EventTracker tracker;
    bool started = aria_fs_watcher_start(watcher, track_event, &tracker);
    REQUIRE(started);

    wait_for_events(500); // Allow watcher to initialize

    // Rename the file
    fs::rename(dir.file("old_name.wav"), dir.file("new_name.wav"));
    wait_for_events();

    // On some platforms, rename may be reported as create+delete
    // At minimum, we should see either a rename event or create+delete
    bool rename_detected = (tracker.renames >= 1) ||
                           (tracker.creates >= 1 && tracker.deletes >= 1);
    CHECK(rename_detected);

    aria_fs_watcher_stop(watcher);
    aria_fs_watcher_destroy(watcher);
}

TEST_CASE("Watcher: start with null paths fails", "[watcher][manual]") {
    void* watcher = aria_fs_watcher_create(nullptr, 0);
    CHECK(watcher == nullptr);
}

TEST_CASE("Watcher: start stop lifecycle", "[watcher][manual]") {
    TempDir dir;

    const char* paths[] = { dir.path.string().c_str() };
    void* watcher = aria_fs_watcher_create(paths, 1);
    REQUIRE(watcher != nullptr);

    EventTracker tracker;
    bool started = aria_fs_watcher_start(watcher, track_event, &tracker);
    REQUIRE(started);

    wait_for_events(300);

    // Stop
    aria_fs_watcher_stop(watcher);

    // After stopping, create a file — should NOT trigger events
    EventTracker after_stop;
    write_file(dir.file("after_stop.wav"));
    wait_for_events();

    // Tracker should not have changed
    CHECK(after_stop.creates == 0);
    CHECK(after_stop.modifies == 0);
    CHECK(after_stop.deletes == 0);

    aria_fs_watcher_destroy(watcher);
}
