#ifndef ARIA_AUTOMATION_RANDOM_SOURCE_H
#define ARIA_AUTOMATION_RANDOM_SOURCE_H

#include "modulation_types.h"
#include "modulation_source.h"

#include <cstdint>
#include <random>

namespace aria::automation {

/// Sample-and-hold / random modulation source.
///
/// Produces stepped random values at a configurable rate.
/// Supports smooth interpolation between values and a random
/// walk mode where each step moves incrementally from the last.
class RandomSource : public ModulationSource {
public:
    enum class Mode {
        SampleAndHold,   ///< Hold a random value between trigger events
        RandomWalk,      ///< Each step moves ±random amount from current
        Noise            ///< Fresh random value every evaluation
    };

    // ─── Parameters ──────────────────────────────────────────
    /// Set the rate in note divisions (e.g., 1.0/4 = quarter note).
    void set_rate_sync(double note_div) {
        rate_note_ = std::max(0.0, note_div);
        rate_synced_ = true;
    }
    double rate_sync() const { return rate_note_; }

    /// Set free-running rate in Hz.
    void set_rate_hz(double hz) {
        rate_hz_ = std::max(0.0, hz);
        rate_synced_ = false;
    }
    double rate_hz() const { return rate_hz_; }
    bool is_rate_synced() const { return rate_synced_; }

    void set_mode(Mode mode) { mode_ = mode; }
    Mode mode() const { return mode_; }

    void set_smooth(double ms) { smoothing_ms_ = std::max(0.0, ms); }
    double smooth() const { return smoothing_ms_; }

    void set_walk_step(double step) {
        walk_step_ = std::clamp(step, 0.0, 1.0);
    }
    double walk_step() const { return walk_step_; }

    // ─── Overrides ───────────────────────────────────────────
    double evaluate(uint64_t ppqn,
                    const ModulationContext& ctx) const override;
    void reset() override {
        held_value_ = 0.5;
        last_trigger_ppqn_ = 0;
    }

private:
    Mode mode_ = Mode::SampleAndHold;
    double rate_hz_ = 1.0;
    double rate_note_ = 1.0 / 4.0;
    bool rate_synced_ = true;
    double smoothing_ms_ = 0.0;
    double walk_step_ = 0.1;

    /// Current held value for S&H / current position for walk.
    mutable double held_value_ = 0.5;
    /// PPQN of the last trigger event.
    mutable uint64_t last_trigger_ppqn_ = 0;
    /// Deterministic RNG seeded by step index.
    mutable std::minstd_rand rng_{42};
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_RANDOM_SOURCE_H
