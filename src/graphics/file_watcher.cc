#include "graphics/file_watcher.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace aria {

// ══════════════════════════════════════════════════════════════════
// FileWatcher — Platform-Independent Logic
// ══════════════════════════════════════════════════════════════════

FileWatcher::FileWatcher() {
    // Set default debounce for .lua files in constructor so it's
    // available immediately, even before watch_directory() is called.
    std::lock_guard<std::mutex> lock(debounce_mutex_);
    debounce_overrides_[".lua"] = kLuaDebounceMs;
}

FileWatcher::~FileWatcher() {
    stop();
}

bool FileWatcher::watch_directory(
    const std::string& dir_path,
    std::function<void(const std::string&)> callback) {
    stop(); // Stop any previous watch

    dir_path_ = dir_path;
    callback_ = std::move(callback);
    running_ = true;

    watch_thread_ = std::make_unique<std::thread>([this]() {
        platform_watch_loop();
    });

    return true;
}

void FileWatcher::set_debounce(std::string_view extension,
                                std::chrono::milliseconds ms) {
    std::lock_guard<std::mutex> lock(debounce_mutex_);
    debounce_overrides_[std::string(extension)] = ms;
}

std::chrono::milliseconds
FileWatcher::get_debounce(std::string_view extension) const {
    std::lock_guard<std::mutex> lock(debounce_mutex_);
    auto it = debounce_overrides_.find(std::string(extension));
    if (it != debounce_overrides_.end()) {
        return it->second;
    }
    return kDefaultDebounceMs;
}

std::chrono::milliseconds
FileWatcher::debounce_for(const std::string& filename) const {
    // Extract extension
    auto dot = filename.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = filename.substr(dot);
        return get_debounce(ext);
    }
    return kDefaultDebounceMs;
}

void FileWatcher::stop() {
    running_ = false;
    if (watch_thread_ && watch_thread_->joinable()) {
        watch_thread_->join();
        watch_thread_.reset();
    }
}

void FileWatcher::fire_with_debounce(
    const std::string& filename,
    std::chrono::steady_clock::time_point now) {
    std::lock_guard<std::mutex> lock(fire_mutex_);

    auto debounce = debounce_for(filename);
    auto it = last_fire_times_.find(filename);

    if (it != last_fire_times_.end()) {
        // Check if enough time has passed since last fire
        auto elapsed = now - it->second;
        if (elapsed < debounce) {
            return; // Still in debounce window — skip
        }
    }

    last_fire_times_[filename] = now;
    if (callback_) {
        callback_(filename);
    }
}

// ══════════════════════════════════════════════════════════════════
// Platform Watch Loops
// ══════════════════════════════════════════════════════════════════

#if defined(_WIN32)

// ── Win32: ReadDirectoryChangesW with polling ───────────────────
//
// Uses FindFirstChangeNotification + WaitForMultipleObjects for a
// simpler and more portable approach than IOCP completion ports.
// This avoids the complexity of overlapped I/O while still providing
// directory change notifications.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void FileWatcher::platform_watch_loop() {
    // Convert UTF-8 path to wide char for Windows API
    int wlen = MultiByteToWideChar(CP_UTF8, 0, dir_path_.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;

    std::wstring wpath(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, dir_path_.c_str(), -1, &wpath[0], wlen);
    // Remove null terminator that MultiByteToWideChar adds
    wpath.resize(wlen - 1);

    HANDLE hDir = CreateFileW(
        wpath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (hDir == INVALID_HANDLE_VALUE) {
        // Fall back: poll directory periodically
        std::error_code ec;
        auto last_write = std::filesystem::last_write_time(dir_path_, ec);
        while (running_) {
            std::this_thread::sleep_for(kDefaultDebounceMs);
            if (!running_) break;
            auto now = std::filesystem::last_write_time(dir_path_, ec);
            if (!ec && now != last_write) {
                last_write = now;
                auto now_clock = std::chrono::steady_clock::now();
                fire_with_debounce(dir_path_, now_clock);
            }
        }
        return;
    }

    // Buffer for ReadDirectoryChangesW
    std::vector<char> buffer(64 * 1024); // 64 KB buffer
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    const DWORD filter = FILE_NOTIFY_CHANGE_LAST_WRITE
                       | FILE_NOTIFY_CHANGE_CREATION
                       | FILE_NOTIFY_CHANGE_FILE_NAME;

    while (running_) {
        DWORD bytes_returned = 0;
        BOOL success = ReadDirectoryChangesW(
            hDir,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            FALSE, // Not watching subdirectories
            filter,
            &bytes_returned,
            &overlapped,
            nullptr);

        if (!success) {
            std::this_thread::sleep_for(kDefaultDebounceMs);
            continue;
        }

        // Wait for the event with timeout (so we can check running_)
        DWORD wait = WaitForSingleObject(overlapped.hEvent, 100);
        if (wait == WAIT_TIMEOUT) {
            CancelIo(hDir);
            continue;
        }

        // Get the result
        if (!GetOverlappedResult(hDir, &overlapped, &bytes_returned, FALSE)) {
            ResetEvent(overlapped.hEvent);
            continue;
        }

        // Process the notification buffer
        auto* notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
        bool processed = false;

        while (notify && !processed && running_) {
            // Convert wide filename to UTF-8
            int name_len = notify->FileNameLength / sizeof(WCHAR);
            int utf8_len = WideCharToMultiByte(
                CP_UTF8, 0, notify->FileName, name_len, nullptr, 0, nullptr, nullptr);
            if (utf8_len > 0) {
                std::string filename(static_cast<size_t>(utf8_len), '\0');
                WideCharToMultiByte(
                    CP_UTF8, 0, notify->FileName, name_len,
                    &filename[0], utf8_len, nullptr, nullptr);
                auto now = std::chrono::steady_clock::now();
                fire_with_debounce(filename, now);
            }

            if (notify->NextEntryOffset == 0) {
                processed = true;
            } else {
                notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<char*>(notify) + notify->NextEntryOffset);
            }
        }

        ResetEvent(overlapped.hEvent);
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
}

#elif defined(__linux__)

// ── Linux: inotify ──────────────────────────────────────────────

#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>

void FileWatcher::platform_watch_loop() {
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        // Fallback: no notification available
        while (running_) {
            std::this_thread::sleep_for(kDefaultDebounceMs);
        }
        return;
    }

    int wd = inotify_add_watch(fd, dir_path_.c_str(),
                               IN_CLOSE_WRITE | IN_CREATE | IN_DELETE);
    if (wd < 0) {
        close(fd);
        return;
    }

    // Buffer for inotify events (must be at least sizeof(inotify_event) + NAME_MAX + 1)
    std::vector<char> buffer(4096);

    while (running_) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = {0, 100000}; // 100ms timeout for polling

        int ret = select(fd + 1, &rfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue; // timeout — loop to check running_

        ssize_t len = read(fd, buffer.data(), buffer.size());
        if (len <= 0) continue;

        size_t i = 0;
        while (i < static_cast<size_t>(len)) {
            auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
            if (event->len > 0) {
                auto now = std::chrono::steady_clock::now();
                fire_with_debounce(event->name, now);
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}

#elif defined(__APPLE__)

// ── macOS: FSEvents ─────────────────────────────────────────────

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

// FSEvents callback — called from CoreFoundation runloop
static void fsevents_callback(ConstFSEventStreamRef /*streamRef*/,
                               void* clientCallBackInfo,
                               size_t numEvents,
                               void* eventPaths,
                               const FSEventStreamEventFlags[],
                               const FSEventStreamEventId[]) {
    auto* watcher = static_cast<FileWatcher*>(clientCallBackInfo);
    if (!watcher) return;

    auto paths = static_cast<const char**>(eventPaths);
    auto now = std::chrono::steady_clock::now();

    for (size_t i = 0; i < numEvents; ++i) {
        if (paths[i]) {
            // Extract filename from the full path
            std::string full_path(paths[i]);
            auto pos = full_path.rfind('/');
            std::string filename = (pos != std::string::npos)
                                     ? full_path.substr(pos + 1)
                                     : full_path;
            watcher->fire_with_debounce(filename, now);
        }
    }
}

void FileWatcher::platform_watch_loop() {
    CFStringRef path = CFStringCreateWithCString(
        nullptr, dir_path_.c_str(), kCFStringEncodingUTF8);
    if (!path) return;

    CFArrayRef paths_to_watch = CFArrayCreate(
        nullptr, reinterpret_cast<const void**>(&path), 1, &kCFTypeArrayCallBacks);

    FSEventStreamContext context{};
    context.info = this;

    FSEventStreamRef stream = FSEventStreamCreate(
        nullptr,
        &fsevents_callback,
        &context,
        paths_to_watch,
        kFSEventStreamEventIdSinceNow,
        0.2, // 200ms latency
        kFSEventStreamCreateFlagNone);

    if (stream) {
        FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(),
                                         kCFRunLoopDefaultMode);
        FSEventStreamStart(stream);

        // Run the runloop while watching
        while (running_) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
        }

        FSEventStreamStop(stream);
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
    }

    CFRelease(paths_to_watch);
    CFRelease(path);
}

#else

// ── Unsupported platform: polling fallback ──────────────────────
// Polls the directory at the default debounce interval.

void FileWatcher::platform_watch_loop() {
    std::error_code ec;
    auto last_write = std::filesystem::last_write_time(dir_path_, ec);
    while (running_) {
        std::this_thread::sleep_for(kDefaultDebounceMs);
        if (!running_) break;
        auto now = std::filesystem::last_write_time(dir_path_, ec);
        if (!ec && now != last_write) {
            last_write = now;
            auto now_clock = std::chrono::steady_clock::now();
            fire_with_debounce(dir_path_, now_clock);
        }
    }
}

#endif

// ─── Force template instantiation for unique_ptr<thread> ─────────
// Required on some platforms where the destructor is visible but the
// type is incomplete at destruction time.

} // namespace aria
