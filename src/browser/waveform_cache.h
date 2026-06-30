#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aria_rust.h"

namespace aria {
namespace browser {

/// Two-tier LRU waveform cache.
/// Tier 1: In-memory LRU list (default 100 entries).
/// Tier 2: On-disk cache (.wvc files keyed by SHA-256 hash).
///
/// Waveform data (peak and minima arrays) is generated via FFI
/// call to aria-filesystem's symphonia-based decoder.
class WaveformCache {
public:
    explicit WaveformCache(std::string cache_dir = default_cache_dir());
    ~WaveformCache();

    // Non-copyable, non-movable
    WaveformCache(const WaveformCache&) = delete;
    WaveformCache& operator=(const WaveformCache&) = delete;

    /// Waveform data at a specific resolution (samples per pixel).
    struct WaveformData {
        std::vector<float> peaks;       // Per-pixel maximum values
        std::vector<float> minima;      // Per-pixel minimum values
        uint32_t           resolution;  // Samples per pixel
        uint32_t           sample_rate;
        uint32_t           channels;
    };

    /// Get waveform for a file at the given resolution.
    /// Checks memory cache first, then disk cache, then generates.
    std::optional<WaveformData> get_waveform(const std::string& file_path,
                                              uint32_t resolution = 1024);

    /// Pre-generate and cache a waveform.
    bool generate_and_cache(const std::string& file_path,
                            uint32_t resolution = 1024);

    // ─── Cache statistics ─────────────────────────────────────

    struct CacheStats {
        uint64_t cache_hits      = 0;
        uint64_t cache_misses    = 0;
        size_t   memory_entries  = 0;
        uint64_t disk_entries    = 0;
        uint64_t disk_size_bytes = 0;
    };
    CacheStats stats() const;

    // ─── Maintenance ──────────────────────────────────────────

    /// Clear all cached data (both memory and disk).
    void clear_cache();

    /// Remove disk cache entries whose source files no longer exist.
    void cleanup_orphaned();

    /// Set maximum memory cache entries.
    void set_max_memory_entries(size_t max) { max_memory_entries_ = max; }

private:
    /// Default cache directory path.
    static std::string default_cache_dir();

    /// Get the disk cache file path for a given hash.
    std::string cache_path(const std::string& hash) const;

    /// Load waveform from disk cache.
    std::optional<WaveformData> load_from_disk(const std::string& hash);

    /// Save waveform to disk cache.
    bool save_to_disk(const std::string& hash, const WaveformData& data);

    /// Touch an entry in the LRU list (move to front).
    void touch_lru(const std::string& key);

    /// Evict the least recently used entry from memory.
    void evict_lru();

    // ─── Memory LRU cache ─────────────────────────────────────

    struct CacheEntry {
        WaveformData                          data;
        std::chrono::steady_clock::time_point cached_at;
    };

    std::string                              cache_dir_;
    size_t                                   max_memory_entries_ = 100;

    mutable std::mutex                       mutex_;
    std::list<std::string>                   lru_list_;
    std::unordered_map<std::string, CacheEntry> cache_map_;

    // Stats
    std::atomic<uint64_t> hits_{0};
    std::atomic<uint64_t> misses_{0};
};

} // namespace browser
} // namespace aria
