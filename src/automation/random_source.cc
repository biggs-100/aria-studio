#include "random_source.h"

#include <algorithm>
#include <cmath>

namespace aria::automation {

namespace {
    constexpr double kPPQNPerBeat = 960.0;
    constexpr double kPPQNToSeconds = 0.5 / 960.0;
}

double RandomSource::evaluate(uint64_t ppqn,
                              const ModulationContext& /*ctx*/) const
{
    // Compute trigger interval in PPQN
    double interval_ppqn;
    if (rate_synced_) {
        interval_ppqn = kPPQNPerBeat / rate_note_;
    } else {
        // Free-running: approximate using PPQN→seconds
        interval_ppqn = 1.0 / (rate_hz_ * kPPQNToSeconds);
    }
    if (interval_ppqn <= 0.0) interval_ppqn = 960.0;  // fallback: 1 beat

    // Determine how many triggers have occurred
    uint64_t trigger_count = static_cast<uint64_t>(
        static_cast<double>(ppqn) / interval_ppqn);
    uint64_t trigger_ppqn = static_cast<uint64_t>(
        static_cast<double>(trigger_count) * interval_ppqn);

    // Check if we've crossed a trigger boundary since last call
    if (trigger_ppqn != last_trigger_ppqn_) {
        last_trigger_ppqn_ = trigger_ppqn;

        switch (mode_) {
        case Mode::SampleAndHold: {
            // Fresh random value
            rng_.seed(static_cast<std::minstd_rand::result_type>(trigger_count + 1));
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            held_value_ = dist(rng_);
            break;
        }
        case Mode::RandomWalk: {
            rng_.seed(static_cast<std::minstd_rand::result_type>(trigger_count + 1000));
            std::uniform_real_distribution<double> dist(-walk_step_, walk_step_);
            held_value_ = std::clamp(held_value_ + dist(rng_), 0.0, 1.0);
            break;
        }
        case Mode::Noise:
            // Noise mode is evaluated per-call below; no trigger needed
            break;
        }
    }

    if (mode_ == Mode::Noise) {
        // Fresh value every call
        rng_.seed(static_cast<std::minstd_rand::result_type>(ppqn));
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng_);
    }

    return held_value_;
}

} // namespace aria::automation
