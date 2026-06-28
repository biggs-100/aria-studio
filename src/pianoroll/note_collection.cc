#include "note_collection.h"

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// ID generation
// ═══════════════════════════════════════════════════════════════

NoteID NoteCollection::next_id() {
    // Format: 40-bit seconds + 24-bit counter
    auto now     = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                       now.time_since_epoch())
                       .count();
    uint64_t ts_part = static_cast<uint64_t>(seconds) & 0xFFFFFFFFFFULL;  // 40 bits
    uint64_t ct_part = id_counter_++ & 0xFFFFFFULL;                       // 24 bits
    return NoteID{(ts_part << 24) | ct_part};
}

// ═══════════════════════════════════════════════════════════════
// Index helpers
// ═══════════════════════════════════════════════════════════════

void NoteCollection::rebuild_index() {
    id_index_.clear();
    id_index_.reserve(notes_.size());
    for (size_t i = 0; i < notes_.size(); ++i) {
        id_index_[notes_[i].id.value] = i;
    }
}

// ═══════════════════════════════════════════════════════════════
// CRUD
// ═══════════════════════════════════════════════════════════════

NoteID NoteCollection::add(const Note& note) {
    Note n = note;
    if (n.id.value == 0) {
        n.id = next_id();
    }
    id_index_[n.id.value] = notes_.size();
    notes_.push_back(std::move(n));
    spatial_dirty_ = true;
    return n.id;
}

void NoteCollection::remove(NoteID id) {
    auto it = id_index_.find(id.value);
    if (it == id_index_.end()) return;

    size_t idx = it->second;
    // Swap with last element for O(1) removal.
    if (idx + 1 < notes_.size()) {
        notes_[idx] = std::move(notes_.back());
        id_index_[notes_[idx].id.value] = idx;
    }
    notes_.pop_back();
    id_index_.erase(it);
    selected_.erase(id);
    spatial_dirty_ = true;
}

void NoteCollection::update(NoteID id, const Note& note) {
    auto it = id_index_.find(id.value);
    if (it == id_index_.end()) return;

    size_t idx             = it->second;
    notes_[idx]            = note;
    notes_[idx].id         = id;  // Preserve the original ID.
    spatial_dirty_         = true;
}

void NoteCollection::clear() {
    notes_.clear();
    id_index_.clear();
    selected_.clear();
    spatial_.clear();
    spatial_dirty_ = false;
}

void NoteCollection::reserve(size_t capacity) {
    notes_.reserve(capacity);
}

// ═══════════════════════════════════════════════════════════════
// Queries
// ═══════════════════════════════════════════════════════════════

Note* NoteCollection::find(NoteID id) {
    auto it = id_index_.find(id.value);
    if (it == id_index_.end()) return nullptr;
    return &notes_[it->second];
}

const Note* NoteCollection::find(NoteID id) const {
    auto it = id_index_.find(id.value);
    if (it == id_index_.end()) return nullptr;
    return &notes_[it->second];
}

std::vector<Note*> NoteCollection::find_in_range(
    uint64_t start_ppqn, uint64_t end_ppqn,
    uint8_t key_low, uint8_t key_high)
{
    if (spatial_dirty_) {
        rebuild_spatial_index();
    }

    Rect query_rect{
        static_cast<double>(start_ppqn),
        static_cast<double>(key_low),
        static_cast<double>(end_ppqn - start_ppqn),
        static_cast<double>(key_high - key_low + 1)
    };

    // Use spatial index, then refine.
    auto candidates = spatial_.query(query_rect);

    // The spatial query already performs exact overlap, but we do a
    // secondary filter here for robustness.
    std::vector<Note*> result;
    result.reserve(candidates.size());
    for (Note* n : candidates) {
        uint64_t n_end = n->start_ppqn + n->duration_ppqn;
        // Half-open interval: start ≤ n->start < end, and key_low ≤ key ≤ key_high
        if (n_end > start_ppqn && n->start_ppqn < end_ppqn &&
            n->key >= key_low && n->key <= key_high) {
            result.push_back(n);
        }
    }
    return result;
}

std::vector<Note*> NoteCollection::find_by_key(uint8_t key) {
    std::vector<Note*> result;
    for (auto& n : notes_) {
        if (n.key == key) {
            result.push_back(&n);
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════
// Spatial index
// ═══════════════════════════════════════════════════════════════

void NoteCollection::rebuild_spatial_index() {
    spatial_.rebuild(notes_);
    spatial_dirty_ = false;
}

std::vector<Note*> NoteCollection::query_rect(const Rect& viewport_ppqn) {
    if (spatial_dirty_) {
        rebuild_spatial_index();
    }
    return spatial_.query(viewport_ppqn);
}

// ═══════════════════════════════════════════════════════════════
// Selection
// ═══════════════════════════════════════════════════════════════

void NoteCollection::select(NoteID id) {
    selected_.insert(id);
}

void NoteCollection::deselect(NoteID id) {
    selected_.erase(id);
}

void NoteCollection::select_all() {
    for (const auto& n : notes_) {
        selected_.insert(n.id);
    }
}

void NoteCollection::clear_selection() {
    selected_.clear();
}

void NoteCollection::toggle_selection(NoteID id) {
    if (selected_.count(id)) {
        selected_.erase(id);
    } else {
        selected_.insert(id);
    }
}

// ═══════════════════════════════════════════════════════════════
// Bulk operations
// ═══════════════════════════════════════════════════════════════

void NoteCollection::transpose(int8_t semitones) {
    if (semitones == 0) return;
    for (auto& n : notes_) {
        if (selected_.empty() || selected_.count(n.id)) {
            int new_key = static_cast<int>(n.key) + semitones;
            n.key = static_cast<uint8_t>(std::clamp(new_key, 0, 127));
        }
    }
    spatial_dirty_ = true;
}

void NoteCollection::shift_horizontal(int64_t ppqn) {
    if (ppqn == 0) return;
    for (auto& n : notes_) {
        if (selected_.empty() || selected_.count(n.id)) {
            int64_t new_start = static_cast<int64_t>(n.start_ppqn) + ppqn;
            n.start_ppqn = static_cast<uint64_t>(std::max<int64_t>(0, new_start));
        }
    }
    spatial_dirty_ = true;
}

void NoteCollection::multiply_velocity(double factor) {
    factor = std::clamp(factor, 0.0, 2.0);
    for (auto& n : notes_) {
        if (selected_.empty() || selected_.count(n.id)) {
            int new_vel = static_cast<int>(std::round(static_cast<double>(n.velocity) * factor));
            n.velocity = static_cast<uint8_t>(std::clamp(new_vel, 0, 127));
        }
    }
}

void NoteCollection::sort_by_start() {
    std::sort(notes_.begin(), notes_.end(),
              [](const Note& a, const Note& b) {
                  if (a.start_ppqn != b.start_ppqn) return a.start_ppqn < b.start_ppqn;
                  return a.key < b.key;
              });
    rebuild_index();
    spatial_dirty_ = true;
}

} // namespace aria
