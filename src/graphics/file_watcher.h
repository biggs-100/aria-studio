#ifndef ARIA_FILE_WATCHER_H
#define ARIA_FILE_WATCHER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace aria {

/// Platform-independent file watcher with per-extension debounce.
///
/// Watches a single directory for file change events. Callbacks are
/// invoked on a background thread after the debounce interval for the
/// file's extension has elapsed.
///
/// Debounce defaults:
///   .lua  → 200ms   (responsive hot-reload)
///   other → 2000ms  (media/project files)
class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    /// Start watching directory for changes.
    /// callback receives the relative filename (not full path).
    /// Returns true if watching started successfully.
    bool watch_directory(const std::string& dir_path,
                         std::function<void(const std::string&)> callback);

    /// Override the debounce interval for a specific extension.
    /// extension includes the dot: ".lua", ".wav", etc.
    void set_debounce(std::string_view extension,
                      std::chrono::milliseconds ms);

    /// Get the debounce interval for an extension.
    std::chrono::milliseconds get_debounce(std::string_view extension) const;

    /// Stop watching. Blocks until the watch thread finishes.
    void stop();

    /// Returns true if currently watching a directory.
    bool is_watching() const noexcept { return running_; }

    /// Get the currently watched directory path.
    const std::string& watched_directory() const noexcept {
        return dir_path_;
    }

private:
    std::string dir_path_;
    std::function<void(const std::string&)> callback_;
    std::unordered_map<std::string, std::chrono::milliseconds> debounce_overrides_;
    std::unique_ptr<std::thread> watch_thread_;
    std::atomic<bool> running_{false};
    mutable std::mutex debounce_mutex_;

    /// Default debounce values.
    static constexpr auto kDefaultDebounceMs = std::chrono::milliseconds(2000);
    static constexpr auto kLuaDebounceMs = std::chrono::milliseconds(200);

    /// Resolve debounce for a given filename.
    std::chrono::milliseconds debounce_for(const std::string& filename) const;

    /// Platform-specific watch loop.
    void platform_watch_loop();

    /// Fire callback after debounce.
    void fire_with_debounce(const std::string& filename,
                            std::chrono::steady_clock::time_point now);

    /// Track last fire time per filename.
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_fire_times_;
    std::mutex fire_mutex_;
};

} // namespace aria

#endif // ARIA_FILE_WATCHER_H
