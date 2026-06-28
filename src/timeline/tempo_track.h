#ifndef ARIA_TEMPO_TRACK_H
#define ARIA_TEMPO_TRACK_H

#include "audio/tempo_map.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace aria {

/// A time-signature change event on the timeline.
struct TimeSignatureEvent {
    uint64_t ppqn        = 0;  ///< PPQN position.
    int      numerator   = 4;  ///< e.g. 4 for 4/4.
    int      denominator = 4;  ///< e.g. 4 for 4/4.
};

/// Tempo track — manages tempo (BPM) and time-signature events
/// keyed by PPQN position.
///
/// Reuses the existing TempoMap internally for BPM binary-search
/// queries. The TempoEvent::sample_position field stores the PPQN
/// position (not a sample count), which works because the binary
/// search operates on uint64_t keys.
class TempoTrack {
public:
    /// Add or update a tempo change at the given PPQN position.
    void add_tempo_event(uint64_t ppqn, double bpm);

    /// Add or update a time-signature change.
    void add_time_sig_event(uint64_t ppqn, int num, int den);

    /// Query the effective BPM at a PPQN position.
    double bpm_at(uint64_t ppqn) const;

    /// Query the effective time signature at a PPQN position.
    TimeSignatureEvent time_sig_at(uint64_t ppqn) const;

    /// Access the underlying BPM map for conversion routines.
    const TempoMap& tempo_map() const { return tempo_map_; }

    /// Read-only access to all time-signature events.
    const std::vector<TimeSignatureEvent>& time_sigs() const {
        return time_sigs_;
    }

    /// Number of tempo events.
    size_t event_count() const { return tempo_map_.event_count(); }

    /// Number of time-signature events.
    size_t time_sig_count() const { return time_sigs_.size(); }

    /// Set default BPM used when no events cover a position.
    void set_default_bpm(double bpm) { tempo_map_.set_default_bpm(bpm); }

    /// Default BPM.
    double default_bpm() const { return tempo_map_.default_bpm(); }

private:
    TempoMap                     tempo_map_;   ///< Reused for BPM binary search.
    std::vector<TimeSignatureEvent> time_sigs_; ///< Sorted by ppqn.
};

} // namespace aria

#endif // ARIA_TEMPO_TRACK_H
