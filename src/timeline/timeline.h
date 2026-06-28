#ifndef ARIA_TIMELINE_H
#define ARIA_TIMELINE_H

#include "audio/tempo_map.h"

#include <cstdint>

namespace aria {

/// Timeline — PPQN-based display helpers and time conversion.
///
/// Provides conversion between PPQN ticks, bar/beat display positions,
/// and real-time seconds (via a TempoMap for BPM context).
///
/// All conversions assume 960 PPQN by default and respect the current
/// time signature for bar/beat calculations.
class Timeline {
public:
    static constexpr int kPPQN = 960;

    /// Bar, beat, sixteenth display position.
    struct BarBeat {
        int bar       = 1;  ///< 1-based bar number.
        int beat      = 1;  ///< 1-based beat within bar.
        int sixteenth = 0;  ///< 0-based sixteenth within beat.
        double ticks  = 0.0;///< Sub-sixteenth tick remainder.
    };

    /// Convert PPQN ticks to a bar/beat/sixteenth display position.
    /// Respects the current time signature (default 4/4).
    BarBeat ppqn_to_bar_beat(uint64_t ppqn) const;

    /// Convert bar/beat/sixteenth back to PPQN ticks.
    uint64_t bar_beat_to_ppqn(int bar, int beat, int sixteenth) const;

    /// Convert PPQN ticks to seconds using the given TempoMap.
    /// Uses constant BPM from the map's default — does NOT handle
    /// multi-tempo integration (future enhancement).
    double ppqn_to_seconds(uint64_t ppqn, const TempoMap& map) const;

    /// Convert seconds to PPQN ticks using the given TempoMap.
    uint64_t seconds_to_ppqn(double seconds, const TempoMap& map) const;

    // ─── Time signature ────────────────────────────────────────

    void set_time_signature(int num, int den);
    void time_signature(int& num, int& den) const;

private:
    int ts_num_ = 4;
    int ts_den_ = 4;
};

} // namespace aria

#endif // ARIA_TIMELINE_H
