#include "arrangement.h"

#include <algorithm>
#include <utility>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Track ordering
// ═══════════════════════════════════════════════════════════════

std::vector<TrackID> Arrangement::track_order() const {
    return track_order_;
}

void Arrangement::move_track(TrackID id, uint32_t new_index) {
    auto it = std::find(track_order_.begin(), track_order_.end(), id);
    if (it == track_order_.end()) return;

    if (new_index >= track_order_.size()) {
        new_index = static_cast<uint32_t>(track_order_.size()) - 1;
    }

    track_order_.erase(it);
    track_order_.insert(track_order_.begin() + new_index, id);
}

void Arrangement::insert_track(TrackID id, uint32_t index) {
    if (index > track_order_.size()) {
        track_order_.push_back(id);
    } else {
        track_order_.insert(track_order_.begin() + index, id);
    }
}

void Arrangement::remove_track(TrackID id) {
    auto it = std::remove(track_order_.begin(), track_order_.end(), id);
    track_order_.erase(it, track_order_.end());
}

// ═══════════════════════════════════════════════════════════════
// Project length
// ═══════════════════════════════════════════════════════════════

void Arrangement::set_length(uint64_t ppqn) {
    length_ppqn_ = ppqn;
}

uint64_t Arrangement::length() const {
    return length_ppqn_;
}

// ═══════════════════════════════════════════════════════════════
// Loop range
// ═══════════════════════════════════════════════════════════════

void Arrangement::set_loop_range(uint64_t start, uint64_t end) {
    loop_start_ppqn_ = start;
    loop_end_ppqn_   = end;
}

std::pair<uint64_t, uint64_t> Arrangement::loop_range() const {
    return {loop_start_ppqn_, loop_end_ppqn_};
}

void Arrangement::set_loop_enabled(bool enabled) {
    loop_enabled_ = enabled;
}

bool Arrangement::loop_enabled() const {
    return loop_enabled_;
}

// ═══════════════════════════════════════════════════════════════
// Markers
// ═══════════════════════════════════════════════════════════════

MarkerList& Arrangement::markers() {
    return markers_;
}

const MarkerList& Arrangement::markers() const {
    return markers_;
}

// ═══════════════════════════════════════════════════════════════
// Tempo
// ═══════════════════════════════════════════════════════════════

TempoTrack& Arrangement::tempo_track() {
    return tempo_track_;
}

const TempoTrack& Arrangement::tempo_track() const {
    return tempo_track_;
}

} // namespace aria
