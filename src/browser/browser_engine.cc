#include "browser/browser_engine.h"

#include "kernel/application.h"
#include "kernel/event_bus.h"
#include "kernel/service_locator.h"

#include <chrono>
#include <cstring>
#include <thread>

namespace aria {
namespace browser {

// ═══════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════

BrowserEngine::BrowserEngine() = default;

BrowserEngine::~BrowserEngine() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════

bool BrowserEngine::init(const std::string& db_path) {
    if (initialized_.load()) return true;

    // 1. Open database via FFI
    db_ = aria_db_open(db_path.c_str());
    if (!db_) return false;

    // 2. Run migrations
    if (!aria_db_run_migrations(db_)) {
        aria_db_close(db_);
        db_ = nullptr;
        return false;
    }

    // 3. Create sub-components
    search_provider_ = std::make_unique<SearchProvider>(db_);
    preview_player_  = std::make_unique<PreviewPlayer>();

    // 4. Create waveform cache in config directory
    //    Derive cache dir from db_path parent
    auto pos = db_path.find_last_of("/\\");
    std::string cache_dir = (pos != std::string::npos)
                                ? db_path.substr(0, pos)
                                : ".";
    cache_dir += "/waveform_cache";
    waveform_cache_ = std::make_unique<WaveformCache>(cache_dir);

    // 5. Register with ServiceLocator
    auto& sl = Application::instance().services();
    sl.register_service<BrowserEngine>(std::unique_ptr<BrowserEngine>(this));

    initialized_.store(true);

    // 6. Publish init event
    publish_event(BrowserEventType::kScanCompleted, 0, "engine_initialized");

    return true;
}

void BrowserEngine::shutdown() {
    if (!initialized_.load()) return;

    // Stop watcher
    if (watcher_) {
        aria_fs_watcher_stop(watcher_);
        aria_fs_watcher_destroy(watcher_);
        watcher_ = nullptr;
    }

    // Stop scanner
    scanning_.store(false);
    if (scanner_) {
        aria_fs_scanner_destroy(scanner_);
        scanner_ = nullptr;
    }

    // Destroy sub-components (order matters — preview stops threads)
    preview_player_.reset();
    search_provider_.reset();
    waveform_cache_.reset();

    // Close database
    if (db_) {
        aria_db_close(db_);
        db_ = nullptr;
    }

    initialized_.store(false);
}

// ═══════════════════════════════════════════════════════════════
// Folder management
// ═══════════════════════════════════════════════════════════════

void BrowserEngine::add_watched_folder(const std::string& path) {
    if (!db_) return;

    // Store in database
    aria_db_add_watched_folder(db_, path.c_str(), 1);

    watched_folders_.push_back(path);

    // Create/recreate watcher with all folders
    if (watcher_) {
        aria_fs_watcher_stop(watcher_);
        aria_fs_watcher_destroy(watcher_);
    }

    // Build paths array for FFI
    std::vector<const char*> c_paths;
    for (const auto& p : watched_folders_) {
        c_paths.push_back(p.c_str());
    }

    watcher_ = aria_fs_watcher_create(c_paths.data(),
                                       static_cast<int32_t>(c_paths.size()));
    if (watcher_) {
        aria_fs_watcher_start(watcher_, watcher_event_callback, this);
    }
}

void BrowserEngine::remove_watched_folder(const std::string& path) {
    if (!db_) return;

    aria_db_remove_watched_folder(db_, path.c_str());

    auto it = std::remove(watched_folders_.begin(), watched_folders_.end(), path);
    if (it != watched_folders_.end()) {
        watched_folders_.erase(it, watched_folders_.end());
    }

    // Recreate watcher without this path
    if (watcher_) {
        aria_fs_watcher_stop(watcher_);
        aria_fs_watcher_destroy(watcher_);
        watcher_ = nullptr;
    }

    if (!watched_folders_.empty()) {
        std::vector<const char*> c_paths;
        for (const auto& p : watched_folders_) {
            c_paths.push_back(p.c_str());
        }
        watcher_ = aria_fs_watcher_create(c_paths.data(),
                                           static_cast<int32_t>(c_paths.size()));
        if (watcher_) {
            aria_fs_watcher_start(watcher_, watcher_event_callback, this);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Scanning
// ═══════════════════════════════════════════════════════════════

void BrowserEngine::start_scan() {
    if (!db_ || watched_folders_.empty() || scanning_.load()) return;

    scanning_.store(true);
    scan_progress_.store(0.0);
    scan_indexed_.store(0);
    scan_total_.store(0);

    publish_event(BrowserEventType::kScanStarted);

    // Build paths array for FFI
    std::vector<const char*> c_paths;
    for (const auto& p : watched_folders_) {
        c_paths.push_back(p.c_str());
    }

    scanner_ = aria_fs_scanner_create(c_paths.data(),
                                       static_cast<int32_t>(c_paths.size()));
    if (!scanner_) {
        scanning_.store(false);
        return;
    }

    // Run scanner in background thread
    std::thread([this]() {
        aria_fs_scanner_run(scanner_, scan_progress_callback, this);

        // Scan complete
        aria_fs_scanner_destroy(scanner_);
        scanner_ = nullptr;
        scanning_.store(false);
        scan_progress_.store(1.0);

        publish_event(BrowserEventType::kScanCompleted,
                      scan_indexed_.load());
    }).detach();
}

// ═══════════════════════════════════════════════════════════════
// FFI Callbacks
// ═══════════════════════════════════════════════════════════════

void BrowserEngine::scan_progress_callback(int64_t indexed, int64_t total,
                                            ScannedFileResult* last_file,
                                            void* userdata) {
    auto* self = static_cast<BrowserEngine*>(userdata);
    if (!self) return;

    self->scan_indexed_.store(indexed);
    self->scan_total_.store(total);
    self->scan_progress_.store(total > 0
                                   ? static_cast<double>(indexed) / total
                                   : 0.0);

    // Publish progress event with last file info
    if (last_file) {
        self->publish_event(BrowserEventType::kIndexProgress,
                            indexed,
                            last_file->path);
    }
}

void BrowserEngine::watcher_event_callback(WatchEvent* event, void* userdata) {
    auto* self = static_cast<BrowserEngine*>(userdata);
    if (!self || !event) return;

    self->handle_watch_event(*event);
}

void BrowserEngine::handle_watch_event(const WatchEvent& event) {
    BrowserEventType type;
    switch (event.event_type) {
        case WATCH_EVENT_CREATED:
            type = BrowserEventType::kFileScanned;
            break;
        case WATCH_EVENT_MODIFIED:
            type = BrowserEventType::kFileChanged;
            break;
        case WATCH_EVENT_DELETED:
            type = BrowserEventType::kFileDeleted;
            break;
        default:
            return; // Unhandled event type
    }

    publish_event(type, 0, event.path);
}

// ═══════════════════════════════════════════════════════════════
// Event publishing
// ═══════════════════════════════════════════════════════════════

void BrowserEngine::publish_event(BrowserEventType type, int64_t int_val,
                                   const char* str_val) {
    auto& bus = Application::instance().events();

    BrowserEvent ev;
    ev.type      = type;
    ev.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count());
    ev.int_value    = int_val;
    ev.double_value = 0.0;

    if (str_val) {
        std::strncpy(ev.str_data, str_val, sizeof(ev.str_data) - 1);
        ev.str_data[sizeof(ev.str_data) - 1] = '\0';
    }

    bus.publish(ev);
}

} // namespace browser
} // namespace aria
