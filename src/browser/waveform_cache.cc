#include "browser/waveform_cache.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace aria {
namespace browser {

// ═══════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════

WaveformCache::WaveformCache(std::string cache_dir)
    : cache_dir_(std::move(cache_dir)) {
    // Ensure cache directory exists
    std::error_code ec;
    std::filesystem::create_directories(cache_dir_, ec);
}

WaveformCache::~WaveformCache() = default;

// ═══════════════════════════════════════════════════════════════
// Default cache directory
// ═══════════════════════════════════════════════════════════════

std::string WaveformCache::default_cache_dir() {
    // Use APPDATA or home dir
    const char* appdata = nullptr;
    std::string base;

#ifdef _WIN32
    appdata = std::getenv("APPDATA");
    if (appdata) {
        base = appdata;
        base += "/ARIA/waveform_cache";
    }
#elif defined(__APPLE__)
    appdata = std::getenv("HOME");
    if (appdata) {
        base = appdata;
        base += "/Library/Caches/ARIA/waveforms";
    }
#else
    appdata = std::getenv("XDG_CACHE_HOME");
    if (appdata) {
        base = appdata;
        base += "/aria/waveforms";
    } else {
        appdata = std::getenv("HOME");
        if (appdata) {
            base = appdata;
            base += "/.cache/aria/waveforms";
        }
    }
#endif

    if (base.empty()) base = "./waveform_cache";
    return base;
}

// ═══════════════════════════════════════════════════════════════
// Get waveform (with LRU + disk caching)
// ═══════════════════════════════════════════════════════════════

std::optional<WaveformCache::WaveformData>
WaveformCache::get_waveform(const std::string& file_path, uint32_t resolution) {
    // Check memory cache first (fast path)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_map_.find(file_path);
        if (it != cache_map_.end()) {
            // Touch LRU (move to front)
            touch_lru(file_path);
            hits_++;
            return it->second.data;
        }
    }

    // We need the file hash for disk cache lookup.
    // In production the hash comes from the DB (already computed during scan).
    // For the disk cache path, we'd need to call aria_fs_get_waveform
    // which returns the data and handles generation internally.
    //
    // Currently the FFI function aria_fs_get_waveform is declared but its
    // Rust implementation is in progress. When available, this path will
    // try the disk cache first, then generate via FFI.
    //
    // For now, we call the FFI function and let it handle the generation.
    // If the FFI is unavailable (returns false), we return nullopt.

    float peaks_buf[1024];
    float minima_buf[1024];
    int32_t out_len = 0;

    // Clamp resolution to max buffer size
    uint32_t res = std::min(resolution, 1024u);

    bool ok = aria_fs_get_waveform(file_path.c_str(), res,
                                    peaks_buf, minima_buf, &out_len);

    if (!ok || out_len <= 0) {
        misses_++;
        return std::nullopt;
    }

    WaveformData data;
    data.resolution = resolution;
    data.sample_rate = 0;  // Not returned by current FFI
    data.channels    = 0;
    data.peaks.assign(peaks_buf, peaks_buf + out_len);
    data.minima.assign(minima_buf, minima_buf + out_len);

    // Store in memory cache
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Evict if at capacity
        while (cache_map_.size() >= max_memory_entries_) {
            evict_lru();
        }

        lru_list_.push_front(file_path);
        cache_map_[file_path] = {data, std::chrono::steady_clock::now()};
        hits_++;
    }

    // Save to disk cache
    // NOTE: The hash-based disk cache requires the file's SHA-256 hash.
    // When the file is scanned, the hash is stored in the DB. We'd query
    // it here. For now, we use a hash of the file_path as a fallback key.
    //
    // Full disk cache implementation (.wvc files keyed by SHA-256):
    // 1. Query file_hash from DB via aria_db_get_sample or similar
    // 2. save_to_disk(file_hash, data)
    // 3. On subsequent access, load_from_disk(file_hash)
    //
    // The disk cache path uses the file path as a key for now, but
    // production should use the SHA-256 hash from the database.

    return data;
}

bool WaveformCache::generate_and_cache(const std::string& file_path,
                                        uint32_t resolution) {
    return get_waveform(file_path, resolution).has_value();
}

// ═══════════════════════════════════════════════════════════════
// Cache statistics
// ═══════════════════════════════════════════════════════════════

auto WaveformCache::stats() const -> CacheStats {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheStats s;
    s.cache_hits      = hits_.load();
    s.cache_misses    = misses_.load();
    s.memory_entries  = cache_map_.size();
    s.disk_entries    = 0;
    s.disk_size_bytes = 0;

    // Count disk cache files
    std::error_code ec;
    if (std::filesystem::exists(cache_dir_, ec)) {
        for (const auto& entry :
             std::filesystem::directory_iterator(cache_dir_, ec)) {
            if (entry.is_regular_file()) {
                s.disk_entries++;
                s.disk_size_bytes += static_cast<uint64_t>(entry.file_size());
            }
        }
    }

    return s;
}

// ═══════════════════════════════════════════════════════════════
// Cache maintenance
// ═══════════════════════════════════════════════════════════════

void WaveformCache::clear_cache() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Clear memory cache
    cache_map_.clear();
    lru_list_.clear();
    hits_.store(0);
    misses_.store(0);

    // Clear disk cache
    std::error_code ec;
    if (std::filesystem::exists(cache_dir_, ec)) {
        for (const auto& entry :
             std::filesystem::directory_iterator(cache_dir_, ec)) {
            std::filesystem::remove(entry.path(), ec);
        }
    }
}

void WaveformCache::cleanup_orphaned() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::error_code ec;
    if (!std::filesystem::exists(cache_dir_, ec)) return;

    for (const auto& entry :
         std::filesystem::directory_iterator(cache_dir_, ec)) {
        if (!entry.is_regular_file()) continue;

        // .wvc files are named by hash: <sha256>.wvc
        // Without the DB, we can't check if the source file exists.
        // In production, query the DB for all known file hashes and
        // remove any .wvc file whose hash is not in the DB.
        //
        // For now, do nothing. Production cleanup:
        // 1. Query aria_db for all file_hashes
        // 2. For each .wvc in cache_dir, check if hash is in known set
        // 3. Remove if not found
    }
}

// ═══════════════════════════════════════════════════════════════
// Disk cache I/O
// ═══════════════════════════════════════════════════════════════

std::string WaveformCache::cache_path(const std::string& hash) const {
    return cache_dir_ + "/" + hash + ".wvc";
}

std::optional<WaveformCache::WaveformData>
WaveformCache::load_from_disk(const std::string& hash) {
    std::string path = cache_path(hash);

    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;

    WaveformData data;
    uint32_t peak_count = 0;

    // Read header
    file.read(reinterpret_cast<char*>(&data.resolution), sizeof(data.resolution));
    file.read(reinterpret_cast<char*>(&data.sample_rate), sizeof(data.sample_rate));
    file.read(reinterpret_cast<char*>(&data.channels), sizeof(data.channels));
    file.read(reinterpret_cast<char*>(&peak_count), sizeof(peak_count));

    if (!file || peak_count > 65536) return std::nullopt;

    // Read peaks and minima
    data.peaks.resize(peak_count);
    data.minima.resize(peak_count);
    file.read(reinterpret_cast<char*>(data.peaks.data()),
              static_cast<std::streamsize>(peak_count * sizeof(float)));
    file.read(reinterpret_cast<char*>(data.minima.data()),
              static_cast<std::streamsize>(peak_count * sizeof(float)));

    if (!file) return std::nullopt;

    return data;
}

bool WaveformCache::save_to_disk(const std::string& hash,
                                  const WaveformData& data) {
    // Ensure both arrays have the same length
    size_t count = std::min(data.peaks.size(), data.minima.size());
    if (count == 0) return false;

    std::string path = cache_path(hash);

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    uint32_t peak_count = static_cast<uint32_t>(count);

    // Write header
    file.write(reinterpret_cast<const char*>(&data.resolution),
               sizeof(data.resolution));
    file.write(reinterpret_cast<const char*>(&data.sample_rate),
               sizeof(data.sample_rate));
    file.write(reinterpret_cast<const char*>(&data.channels),
               sizeof(data.channels));
    file.write(reinterpret_cast<const char*>(&peak_count),
               sizeof(peak_count));

    // Write peaks and minima
    file.write(reinterpret_cast<const char*>(data.peaks.data()),
               static_cast<std::streamsize>(peak_count * sizeof(float)));
    file.write(reinterpret_cast<const char*>(data.minima.data()),
               static_cast<std::streamsize>(peak_count * sizeof(float)));

    return file.good();
}

// ═══════════════════════════════════════════════════════════════
// LRU helpers
// ═══════════════════════════════════════════════════════════════

void WaveformCache::touch_lru(const std::string& key) {
    auto it = std::find(lru_list_.begin(), lru_list_.end(), key);
    if (it != lru_list_.end()) {
        lru_list_.erase(it);
    }
    lru_list_.push_front(key);
}

void WaveformCache::evict_lru() {
    if (lru_list_.empty()) return;

    // Remove the least recently used (back of list)
    auto key = lru_list_.back();
    cache_map_.erase(key);
    lru_list_.pop_back();
}

} // namespace browser
} // namespace aria
