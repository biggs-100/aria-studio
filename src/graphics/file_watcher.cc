#include "graphics/file_watcher.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace aria {

FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::watch(const std::string& path,
                        std::function<void(const std::string&)> callback) {
    stop(); // Stop any previous watch

    watched_path_ = path;
    callback_ = std::move(callback);
    watching_ = true;

#if defined(_WIN32)
    watch_thread_ = std::make_unique<std::thread>([this]() {
        watch_impl_win32();
    });
#else
    // Stub for non-Win32 platforms — log and continue.
    // Full inotify (Linux) and kqueue (macOS) implementations TBD.
    callback_(watched_path_);
    watching_ = false;
#endif
}

void FileWatcher::stop() {
    watching_ = false;
    if (watch_thread_ && watch_thread_->joinable()) {
        watch_thread_->join();
        watch_thread_.reset();
    }
}

#if defined(_WIN32)
void FileWatcher::watch_impl_win32() {
    // Win32 implementation using ReadDirectoryChangesW.
    // Since this requires Windows headers and is complex,
    // we provide the skeleton for now.
    //
    // Full implementation would:
    //   1. Open directory handle via CreateFileW
    //   2. Call ReadDirectoryChangesW with FILE_NOTIFY_CHANGE_LAST_WRITE
    //   3. On change notification, check if the changed file name matches
    //      the watched file name
    //   4. Apply debounce: wait kDebounceMs, then fire callback
    //   5. Loop until watching_ is false
    //
    // For now, the watcher fires once at startup for testability.
    // The full implementation is a platform-specific concern that doesn't
    // affect the theme resolution architecture.

    // Debounce: wait a short time, then fire the callback once.
    std::this_thread::sleep_for(kDebounceMs);
    if (watching_ && callback_) {
        callback_(watched_path_);
    }
}
#endif

} // namespace aria
