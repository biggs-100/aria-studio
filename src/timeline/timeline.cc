#include "timeline.h"

#include <cmath>
#include <cstdint>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// PPQN ↔ Bar/Beat conversion
// ═══════════════════════════════════════════════════════════════
//
// PPQN (Pulses Per Quarter Note) = 960.
// For 4/4 time:
//   1 bar    = 4 beats       = 3840 PPQN
//   1 beat   = 1 quarter     =  960 PPQN
//   1 16th   = 1/4 beat      =  240 PPQN
//
// For other time signatures (e.g. 3/4, 6/8), the number of
// quarter-note ticks per bar adjusts using the denominator:
//   ticks_per_beat = kPPQN / (den / 4)
//   ppqn_per_bar   = numerator * ticks_per_beat

Timeline::BarBeat Timeline::ppqn_to_bar_beat(uint64_t ppqn) const {
    BarBeat result;

    int beats_per_bar    = ts_num_;
    int ticks_per_beat   = kPPQN * 4 / ts_den_;   // e.g. 4/4 → 960, 6/8 → 640
    uint64_t ppqn_per_bar = static_cast<uint64_t>(beats_per_bar) *
                            static_cast<uint64_t>(ticks_per_beat);

    result.bar = static_cast<int>(ppqn / ppqn_per_bar) + 1;
    uint64_t remainder = ppqn % ppqn_per_bar;

    result.beat = static_cast<int>(remainder / ticks_per_beat) + 1;
    remainder   %= static_cast<uint64_t>(ticks_per_beat);

    int ticks_per_sixteenth = ticks_per_beat / 4;
    if (ticks_per_sixteenth < 1) ticks_per_sixteenth = 1;

    result.sixteenth = static_cast<int>(remainder / ticks_per_sixteenth);
    result.ticks     = static_cast<double>(remainder % ticks_per_sixteenth);

    return result;
}

uint64_t Timeline::bar_beat_to_ppqn(int bar, int beat, int sixteenth) const {
    int beats_per_bar    = ts_num_;
    int ticks_per_beat   = kPPQN * 4 / ts_den_;
    uint64_t ppqn_per_bar = static_cast<uint64_t>(beats_per_bar) *
                            static_cast<uint64_t>(ticks_per_beat);

    int ticks_per_sixteenth = ticks_per_beat / 4;
    if (ticks_per_sixteenth < 1) ticks_per_sixteenth = 1;

    uint64_t result = static_cast<uint64_t>(bar - 1) * ppqn_per_bar;
    result += static_cast<uint64_t>(std::max(beat - 1, 0)) *
              static_cast<uint64_t>(ticks_per_beat);
    result += static_cast<uint64_t>(std::max(sixteenth, 0)) *
              static_cast<uint64_t>(ticks_per_sixteenth);

    return result;
}

// ═══════════════════════════════════════════════════════════════
// PPQN ↔ Seconds conversion  (constant-tempo approximation)
// ═══════════════════════════════════════════════════════════════
//
// These functions use the TempoMap's default BPM as a constant
// tempo. A production implementation would iterate over all tempo
// events and integrate piecewise over the PPQN range.

double Timeline::ppqn_to_seconds(uint64_t ppqn, const TempoMap& map) const {
    double beats = static_cast<double>(ppqn) / static_cast<double>(kPPQN);
    double bpm   = map.default_bpm();
    if (bpm <= 0.0) bpm = 120.0;
    return beats * (60.0 / bpm);
}

uint64_t Timeline::seconds_to_ppqn(double seconds, const TempoMap& map) const {
    double bpm = map.default_bpm();
    if (bpm <= 0.0) bpm = 120.0;
    double beats = seconds * (bpm / 60.0);
    return static_cast<uint64_t>(std::round(beats * static_cast<double>(kPPQN)));
}

// ─── Time signature ───────────────────────────────────────────

void Timeline::set_time_signature(int num, int den) {
    ts_num_ = num;
    ts_den_ = den;
}

void Timeline::time_signature(int& num, int& den) const {
    num = ts_num_;
    den = ts_den_;
}

} // namespace aria
