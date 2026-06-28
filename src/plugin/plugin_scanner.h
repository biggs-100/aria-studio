#ifndef ARIA_PLUGIN_PLUGIN_SCANNER_H
#define ARIA_PLUGIN_PLUGIN_SCANNER_H

#include "audio_plugin_types.h"
#include "format_scanner.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria::plugin {

/// Result of a single scan operation.
struct ScanResult {
    std::vector<PluginID> found;          ///< Newly discovered plugins
    std::vector<PluginID> removed;        ///< Plugins whose files disappeared
    std::vector<PluginDescriptor> all;    ///< Complete list of descriptors
    uint32_t scan_duration_ms = 0;        ///< Total scan time
    uint32_t files_scanned = 0;           ///< Number of files actually opened
};

/// Incremental plugin scanner using file modification times.
///
/// On each scan, compares cached mtimes against current filesystem
/// state. Only files that changed (new, modified, deleted) are
/// re-scanned. Results are cached in a JSON file for fast startup.
class PluginScanner {
public:
    PluginScanner() = default;
    ~PluginScanner() = default;

    PluginScanner(const PluginScanner&) = delete;
    PluginScanner& operator=(const PluginScanner&) = delete;

    // ── Scanning ───────────────────────────────────────────────

    /// Run an incremental scan across the given paths.
    /// @param paths    Directories to scan for plugins.
    /// @param scanners Format-specific scanners to use.
    /// @return ScanResult with found/removed/all descriptors.
    ScanResult scan(const std::vector<std::string>& paths,
                    const std::vector<std::unique_ptr<FormatScanner>>& scanners);

    /// Force a full rescan — clears cache and rescans everything.
    ScanResult rescan(const std::vector<std::string>& paths,
                      const std::vector<std::unique_ptr<FormatScanner>>& scanners);

    // ── Cache ──────────────────────────────────────────────────

    /// Set the path for the JSON cache file.
    void set_cache_path(const std::string& path);

    /// Load cache from the configured path. Returns false if cache
    /// is missing or corrupt (caller should fall back to full scan).
    bool load_cache();

    /// Save current cache to the configured path (atomic write).
    bool save_cache();

    /// Schedule a cache write (called automatically after scan).
    bool schedule_save();

    // ── Control ────────────────────────────────────────────────

    /// Cancel a running scan. The scan stops after the current file.
    void cancel();

    /// Whether a scan is currently in progress.
    bool is_scanning() const;

    /// Progress of the current scan (0.0 — 1.0).
    float progress() const;

private:
    /// Rebuild mtime cache from a ScanResult's descriptors.
    void rebuild_mtime_cache(const std::vector<PluginDescriptor>& descriptors);

    /// Scan a single file with the appropriate format scanner.
    std::vector<PluginDescriptor> scan_file(
        const std::string& path,
        const std::vector<std::unique_ptr<FormatScanner>>& scanners);

    // ── Members ────────────────────────────────────────────────

    std::string cache_path_;

    // mtime cache: file path → last known modification time
    std::unordered_map<std::string, std::filesystem::file_time_type> mtime_cache_;

    // All known plugin descriptors (from the last successful scan)
    std::vector<PluginDescriptor> descriptors_;

    // Cancellation and progress
    std::atomic<bool> cancel_{false};
    std::atomic<bool> scanning_{false};
    std::atomic<float> progress_{0.0f};
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_PLUGIN_SCANNER_H
