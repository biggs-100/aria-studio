#include "cache.h"

#include <utility>

namespace aria::automation {

// ─── Write API (control thread) ────────────────────────────

void ParameterCache::update_value(ParameterID id, double value) {
    auto it = write_buffer_.find(id);
    if (it != write_buffer_.end()) {
        it->second.value.store(value, std::memory_order_relaxed);
    } else {
        // operator[] default-constructs an Entry, then we set the value
        Entry& e = write_buffer_[id];
        e.value.store(value, std::memory_order_relaxed);
    }
}

void ParameterCache::update_range(ParameterID id, double min, double max) {
    auto it = write_buffer_.find(id);
    if (it != write_buffer_.end()) {
        it->second.min = min;
        it->second.max = max;
    } else {
        Entry& e = write_buffer_[id];
        e.value.store(0.0, std::memory_order_relaxed);
        e.min = min;
        e.max = max;
    }
}

void ParameterCache::swap_buffers() {
    std::swap(write_buffer_, read_buffer_);
    pending_swaps_.fetch_add(1, std::memory_order_release);
}

// ─── Read API (audio thread) ───────────────────────────────

double ParameterCache::read_value(ParameterID id) const {
    auto it = read_buffer_.find(id);
    if (it != read_buffer_.end()) {
        return it->second.value.load(std::memory_order_relaxed);
    }
    return 0.0;
}

// ─── Utilities ─────────────────────────────────────────────

void ParameterCache::clear() {
    write_buffer_.clear();
    read_buffer_.clear();
    pending_swaps_.store(0, std::memory_order_relaxed);
}

} // namespace aria::automation
