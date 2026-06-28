#include "marker.h"

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════

void MarkerList::ensure_sorted() {
    std::sort(markers_.begin(), markers_.end(),
              [](const Marker& a, const Marker& b) {
                  return a.ppqn < b.ppqn;
              });
}

auto MarkerList::find_it(MarkerID id) -> decltype(markers_.begin()) {
    return std::find_if(markers_.begin(), markers_.end(),
                        [id](const Marker& m) { return m.id == id; });
}

auto MarkerList::find_it(MarkerID id) const -> decltype(markers_.cbegin()) {
    return std::find_if(markers_.cbegin(), markers_.cend(),
                        [id](const Marker& m) { return m.id == id; });
}

// ═══════════════════════════════════════════════════════════════
// CRUD
// ═══════════════════════════════════════════════════════════════

MarkerID MarkerList::add(const std::string& name, uint64_t ppqn) {
    MarkerID id{next_id_++};
    markers_.push_back({id, name, ppqn, {}, {}});
    ensure_sorted();
    return id;
}

bool MarkerList::remove(MarkerID id) {
    auto it = find_it(id);
    if (it == markers_.end()) return false;
    markers_.erase(it);
    return true;
}

bool MarkerList::move(MarkerID id, uint64_t new_ppqn) {
    auto it = find_it(id);
    if (it == markers_.end()) return false;
    it->ppqn = new_ppqn;
    ensure_sorted();
    return true;
}

bool MarkerList::rename(MarkerID id, const std::string& name) {
    auto it = find_it(id);
    if (it == markers_.end()) return false;
    it->name = name;
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Query
// ═══════════════════════════════════════════════════════════════

Marker* MarkerList::find(MarkerID id) {
    auto it = find_it(id);
    return (it != markers_.end()) ? &(*it) : nullptr;
}

const Marker* MarkerList::find(MarkerID id) const {
    auto it = find_it(id);
    return (it != markers_.cend()) ? &(*it) : nullptr;
}

Marker* MarkerList::find_at(uint64_t ppqn) {
    for (auto& m : markers_) {
        if (m.ppqn == ppqn) return &m;
    }
    return nullptr;
}

std::vector<Marker> MarkerList::all() const {
    return markers_;  // already sorted
}

std::vector<Marker> MarkerList::in_range(uint64_t start, uint64_t end) const {
    std::vector<Marker> result;
    // markers_ is sorted by PPQN → early exit
    for (const auto& m : markers_) {
        if (m.ppqn > end) break;
        if (m.ppqn >= start) result.push_back(m);
    }
    return result;
}

} // namespace aria
