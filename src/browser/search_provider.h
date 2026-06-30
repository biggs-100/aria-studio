#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "aria_rust.h"

namespace aria {
namespace browser {

/// Parameters for a search query.
struct SearchParams {
    std::string text;
    std::string category;
    double      bpm_min        = 0.0;
    double      bpm_max        = 999.0;
    std::string musical_key;
    bool        favorite_only  = false;
    int32_t     min_rating     = 0;
    int32_t     limit          = 100;
    int32_t     offset         = 0;
    int32_t     sort_by        = 0;   // 0=relevance, 1=name, 2=date_added, 3=bpm
    bool        sort_ascending = false;
};

/// Result of a search query.
struct SearchResult {
    std::vector<SampleResult> samples;
    int32_t                   total_count = 0;
    std::chrono::milliseconds elapsed{0};
};

/// Callback signature for search completion.
using SearchCallback = std::function<void(const SearchResult&)>;

/// Wraps Rust FFI search calls in a thread pool for non-blocking UI.
/// Maintains a simple result cache keyed by query fingerprint.
class SearchProvider {
public:
    explicit SearchProvider(DbConnection* db);
    ~SearchProvider();

    // Non-copyable, non-movable
    SearchProvider(const SearchProvider&) = delete;
    SearchProvider& operator=(const SearchProvider&) = delete;

    /// Execute a search synchronously (blocks calling thread).
    SearchResult search_sync(const SearchParams& params);

    /// Execute a search asynchronously on a background thread.
    /// Calls `callback` with results when complete.
    void search_async(const SearchParams& params, SearchCallback callback);

    /// Try to get the most recent async result (non-blocking).
    std::optional<SearchResult> try_get_result();

    /// Check if an async search is in progress.
    bool is_searching() const { return searching_.load(); }

    /// Clear the result cache.
    void clear_cache();

    /// Get cache statistics.
    struct CacheStats {
        size_t entries    = 0;
        size_t hits       = 0;
        size_t misses     = 0;
    };
    CacheStats cache_stats() const;

private:
    /// Build a key string from search params for cache lookup.
    static std::string make_cache_key(const SearchParams& params);

    /// Convert SearchParams to DbSearchQuery FFI struct.
    static DbSearchQuery to_ffi_query(const SearchParams& params);

    DbConnection* db_;

    // Async search state
    std::atomic<bool>                searching_{false};
    std::thread                      search_thread_;
    std::mutex                       result_mutex_;
    std::optional<SearchResult>      pending_result_;

    // Cache
    struct CachedResult {
        SearchResult result;
        std::chrono::steady_clock::time_point cached_at;
    };
    mutable std::mutex               cache_mutex_;
    std::unordered_map<std::string, CachedResult> cache_;
    size_t                           cache_hits_   = 0;
    size_t                           cache_misses_ = 0;

    static constexpr size_t          MAX_CACHE_ENTRIES = 50;
    static constexpr auto            CACHE_TTL = std::chrono::minutes(5);
};

} // namespace browser
} // namespace aria
