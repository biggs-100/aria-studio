#include "browser/search_provider.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace aria {
namespace browser {

// ═══════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════

SearchProvider::SearchProvider(DbConnection* db)
    : db_(db) {
}

SearchProvider::~SearchProvider() {
    searching_.store(false);
    if (search_thread_.joinable()) {
        search_thread_.join();
    }
}

// ═══════════════════════════════════════════════════════════════
// FFI query conversion
// ═══════════════════════════════════════════════════════════════

DbSearchQuery SearchProvider::to_ffi_query(const SearchParams& params) {
    DbSearchQuery q{};
    std::strncpy(q.text, params.text.c_str(), sizeof(q.text) - 1);
    std::strncpy(q.category, params.category.c_str(), sizeof(q.category) - 1);
    std::strncpy(q.musical_key, params.musical_key.c_str(), sizeof(q.musical_key) - 1);
    q.bpm_min       = params.bpm_min;
    q.bpm_max       = params.bpm_max;
    q.favorite_only = params.favorite_only ? 1 : 0;
    q.min_rating    = params.min_rating;
    q.limit         = params.limit;
    q.offset        = params.offset;
    q.sort_by       = params.sort_by;
    q.sort_ascending = params.sort_ascending ? 1 : 0;
    return q;
}

// ═══════════════════════════════════════════════════════════════
// Cache key
// ═══════════════════════════════════════════════════════════════

std::string SearchProvider::make_cache_key(const SearchParams& params) {
    std::ostringstream key;
    key << params.text << "|"
        << params.category << "|"
        << params.bpm_min << "-" << params.bpm_max << "|"
        << params.musical_key << "|"
        << (params.favorite_only ? "1" : "0") << "|"
        << params.min_rating << "|"
        << params.limit << "|"
        << params.offset << "|"
        << params.sort_by << "|"
        << (params.sort_ascending ? "1" : "0");
    return key.str();
}

// ═══════════════════════════════════════════════════════════════
// Synchronous search
// ═══════════════════════════════════════════════════════════════

SearchResult SearchProvider::search_sync(const SearchParams& params) {
    SearchResult result;

    if (!db_) return result;

    // Check cache first
    std::string cache_key = make_cache_key(params);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(cache_key);
        if (it != cache_.end()) {
            auto age = std::chrono::steady_clock::now() - it->second.cached_at;
            if (age < CACHE_TTL) {
                cache_hits_++;
                return it->second.result;
            }
            cache_.erase(it);
        }
        cache_misses_++;
    }

    // Call FFI
    auto start = std::chrono::steady_clock::now();

    DbSearchQuery ffi_q = to_ffi_query(params);
    std::vector<SampleResult> buffer(static_cast<size_t>(params.limit));
    int32_t count = aria_db_search_samples(
        db_, &ffi_q, buffer.data(), params.limit);

    result.samples.assign(buffer.begin(), buffer.begin() + count);
    result.total_count = count;
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    // Cache result
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        // Evict oldest if at capacity
        if (cache_.size() >= MAX_CACHE_ENTRIES) {
            // Find and remove oldest entry
            auto oldest = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.cached_at < oldest->second.cached_at) {
                    oldest = it;
                }
            }
            if (oldest != cache_.end()) {
                cache_.erase(oldest);
            }
        }
        cache_[cache_key] = {result, std::chrono::steady_clock::now()};
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
// Asynchronous search
// ═══════════════════════════════════════════════════════════════

void SearchProvider::search_async(const SearchParams& params,
                                   SearchCallback callback) {
    if (searching_.load()) return; // Already searching

    searching_.store(true);

    if (search_thread_.joinable()) {
        search_thread_.join();
    }

    search_thread_ = std::thread([this, params, callback = std::move(callback)]() {
        SearchResult result = search_sync(params);

        {
            std::lock_guard<std::mutex> lock(result_mutex_);
            pending_result_ = result;
        }

        searching_.store(false);

        if (callback) {
            callback(result);
        }
    });
}

std::optional<SearchResult> SearchProvider::try_get_result() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    auto result = pending_result_;
    pending_result_.reset();
    return result;
}

// ═══════════════════════════════════════════════════════════════
// Cache management
// ═══════════════════════════════════════════════════════════════

void SearchProvider::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
}

auto SearchProvider::cache_stats() const -> CacheStats {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return {cache_.size(), cache_hits_, cache_misses_};
}

} // namespace browser
} // namespace aria
