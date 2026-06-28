#include "tempo_map.h"

#include <algorithm>

namespace aria {

void TempoMap::set_events(const std::vector<TempoEvent>& events) {
    events_ = events;

    // Sort by sample_position for binary search
    std::sort(events_.begin(), events_.end(),
              [](const TempoEvent& a, const TempoEvent& b) {
                  return a.sample_position < b.sample_position;
              });
}

double TempoMap::bpm_at(uint64_t sample_position) const {
    if (events_.empty()) {
        return default_bpm_;
    }

    // Binary search for the last event with position <= sample_position.
    // events_ is sorted ascending by sample_position.
    //
    // Example:
    //   events: [{0, 120}, {44100, 140}, {88200, 130}]
    //   bpm_at(0)      → 120
    //   bpm_at(22050)  → 120
    //   bpm_at(44100)  → 140
    //   bpm_at(100000) → 130
    //   bpm_at(50000)  → 140

    auto it = std::upper_bound(
        events_.begin(), events_.end(), sample_position,
        [](uint64_t pos, const TempoEvent& ev) {
            return pos < ev.sample_position;
        });

    if (it == events_.begin()) {
        // sample_position is before the first event — use default
        return default_bpm_;
    }

    // Move to the event just before the upper bound
    --it;
    return it->bpm;
}

} // namespace aria
