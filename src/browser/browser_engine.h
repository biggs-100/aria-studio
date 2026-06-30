#pragma once

#include "kernel/event_types.h"
#include "browser/search_provider.h"
#include "browser/preview_player.h"
#include "browser/waveform_cache.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "aria_rust.h"

namespace aria {

// Forward declaration
class EventBus;
class ServiceLocator;

namespace browser {

/// BrowserEngine — top-level orchestrator for the sample browser.
///
/// Owns the DB connection (via FFI), SearchProvider, PreviewPlayer,
/// and WaveformCache. Manages scanning, file watching, and search.
/// Registers with ServiceLocator and publishes events on EventBus.
class BrowserEngine {
public:
    BrowserEngine();
    ~BrowserEngine();

    // Non-copyable, non-movable
    BrowserEngine(const BrowserEngine&) = delete;
    BrowserEngine& operator=(const BrowserEngine&) = delete;

    // ─── Lifecycle ────────────────────────────────────────────

    /// Initialize the browser engine.
    /// Opens the database, runs migrations, creates sub-components.
    /// Registers with ServiceLocator.
    /// @param db_path  Path to the SQLite database file.
    /// @return true on success.
    bool init(const std::string& db_path);

    /// Shut down the browser engine.
    /// Stops watchers, closes DB, frees resources.
    void shutdown();

    /// Returns true if the engine is initialized.
    bool is_initialized() const { return initialized_.load(); }

    // ─── Folder management ────────────────────────────────────

    /// Add a folder to watch for file changes.
    void add_watched_folder(const std::string& path);

    /// Remove a folder from the watch list.
    void remove_watched_folder(const std::string& path);

    /// Get the list of watched folders.
    const std::vector<std::string>& watched_folders() const { return watched_folders_; }

    // ─── Scanning ─────────────────────────────────────────────

    /// Start a background scan of all watched folders.
    void start_scan();

    /// Check if a scan is currently running.
    bool is_scanning() const { return scanning_.load(); }

    /// Get scan progress (0.0 to 1.0).
    double scan_progress() const { return scan_progress_.load(); }

    /// Get number of files indexed in the current/Last scan.
    int64_t scan_indexed() const { return scan_indexed_.load(); }
    int64_t scan_total()   const { return scan_total_.load(); }

    // ─── Sub-component access ─────────────────────────────────

    SearchProvider* search()   const { return search_provider_.get(); }
    PreviewPlayer*  preview()  const { return preview_player_.get(); }
    WaveformCache*  waveforms() const { return waveform_cache_.get(); }

    // ─── FFI callbacks (static, called from C) ────────────────

    /// Called by the Rust scanner for each file processed.
    static void scan_progress_callback(int64_t indexed, int64_t total,
                                       ScannedFileResult* last_file,
                                       void* userdata);

    /// Called by the Rust watcher for each file system event.
    static void watcher_event_callback(WatchEvent* event, void* userdata);

private:
    // Internal
    void handle_watch_event(const WatchEvent& event);
    void publish_event(BrowserEventType type, int64_t int_val = 0,
                       const char* str_val = nullptr);

    // FFI handles
    DbConnection* db_        = nullptr;
    void*         scanner_   = nullptr;
    void*         watcher_   = nullptr;

    // Sub-components
    std::unique_ptr<SearchProvider> search_provider_;
    std::unique_ptr<PreviewPlayer>  preview_player_;
    std::unique_ptr<WaveformCache>  waveform_cache_;

    // State
    std::atomic<bool>   initialized_{false};
    std::atomic<bool>   scanning_{false};
    std::atomic<double> scan_progress_{0.0};
    std::atomic<int64_t> scan_indexed_{0};
    std::atomic<int64_t> scan_total_{0};

    // Watched folder paths
    std::vector<std::string> watched_folders_;
};

} // namespace browser
} // namespace aria
