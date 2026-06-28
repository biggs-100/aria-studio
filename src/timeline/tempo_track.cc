#include "tempo_track.h"

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Tempo events
// ═══════════════════════════════════════════════════════════════

void TempoTrack::add_tempo_event(uint64_t ppqn, double bpm) {
    // Reuse TempoMap by storing PPQN in the sample_position field.
    TempoEvent ev;
    ev.sample_position = ppqn;
    ev.bpm             = bpm;

    auto events = tempo_map_.events();
    // Replace existing event at same PPQN
    for (auto& e : events) {
        if (e.sample_position == ppqn) {
            e.bpm = bpm;
            tempo_map_.set_events(events);
            return;
        }
    }
    events.push_back(ev);
    tempo_map_.set_events(events);
}

double TempoTrack::bpm_at(uint64_t ppqn) const {
    // TempoMap::bpm_at does binary search by uint64_t key — works
    // transparently with PPQN values in the sample_position field.
    return tempo_map_.bpm_at(ppqn);
}

// ═══════════════════════════════════════════════════════════════
// Time-signature events
// ═══════════════════════════════════════════════════════════════

void TempoTrack::add_time_sig_event(uint64_t ppqn, int num, int den) {
    // Replace existing at same PPQN
    for (auto& ts : time_sigs_) {
        if (ts.ppqn == ppqn) {
            ts.numerator   = num;
            ts.denominator = den;
            return;
        }
    }

    time_sigs_.push_back({ppqn, num, den});
    // Keep sorted by PPQN
    std::sort(time_sigs_.begin(), time_sigs_.end(),
              [](const TimeSignatureEvent& a, const TimeSignatureEvent& b) {
                  return a.ppqn < b.ppqn;
              });
}

TimeSignatureEvent TempoTrack::time_sig_at(uint64_t ppqn) const {
    if (time_sigs_.empty()) {
        return {0, 4, 4};
    }

    // Upper bound → last event <= ppqn
    auto it = std::upper_bound(
        time_sigs_.begin(), time_sigs_.end(), ppqn,
        [](uint64_t pos, const TimeSignatureEvent& ev) {
            return pos < ev.ppqn;
        });

    if (it == time_sigs_.begin()) {
        return {0, 4, 4};  // default 4/4 before any event
    }

    --it;
    return *it;
}

} // namespace aria
