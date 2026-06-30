#ifndef ARIA_FILE_WATCHER_H
#define ARIA_FILE_WATCHER_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace aria {

/// Platform-independent file watcher with debounce support.
///
/// Currently implements Win32 ReadDirectoryChangesW API.
/// Stubs for macOS (kqueue) and Linux (inotify) are planned.
///
/// Usage:
///   FileWatcher watcher;
///   watcher.watch("path/to/file.json", [](const std::string& path) {
///       // reload
///   });
///   // watcher is active until destroyed or stop() is called.
class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    /// Start watching a file for changes.
    /// On change (after debounce), the callback is invoked with the file path.
    void watch(const std::string& path,
               std::function<void(const std::string&)> callback);

    /// Stop watching.
    void stop();

    /// Returns true if currently watching a file.
    bool is_watching() const noexcept { return watching_; }

    /// Returns the currently watched path.
    const std::string& watched_path() const noexcept { return watched_path_; }

private:
#if defined(_WIN32)
    /// Platform-specific implementation (Win32 ReadDirectoryChangesW).
    void watch_impl_win32();
#endif

    std::string watched_path_;
    std::function<void(const std::string&)> callback_;
    bool watching_ = false;
    std::unique_ptr<std::thread> watch_thread_;

    /// Debounce interval.
    static constexpr auto kDebounceMs = std::chrono::milliseconds(200);
};

} // namespace aria

#endif // ARIA_FILE_WATCHER_H
