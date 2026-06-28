#define _USE_MATH_DEFINES
#include "lfo_clip.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace aria::automation {

// ─── Internal: sample-and-hold / noise state ──────────────

namespace {
    // Simple deterministic pseudo-random for S&H and Noise
    struct SHState {
        double last_value = 0.5;
        uint64_t last_phase_ppqn = 0;
        std::minstd_rand rng{42};
    };

    SHState& sh_state() {
        static SHState s;
        return s;
    }
}

double LFOAutomationClip::evaluate_waveform(double phase) const {
    // phase is in [0, 1) for one complete cycle
    phase = phase - std::floor(phase);  // wrap to [0, 1)

    switch (waveform_) {
    case Waveform::Sine:
        return 0.5 + 0.5 * std::sin(2.0 * M_PI * phase);

    case Waveform::Triangle:
        return phase < 0.5
            ? 2.0 * phase          // rise 0→1
            : 2.0 * (1.0 - phase); // fall 1→0

    case Waveform::Saw:
        return phase;  // linear 0→1, instant reset

    case Waveform::SawInv:
        return 1.0 - phase;  // linear 1→0, instant reset

    case Waveform::Square:
        return phase < pulse_width_ ? 1.0 : 0.0;

    case Waveform::SampleAndHold: {
        // Hold a random value each pulse_width cycle
        auto& state = sh_state();
        // Determine which S&H bucket we're in
        double bucket_width = pulse_width_ > 0.01 ? pulse_width_ : 0.5;
        uint64_t bucket = static_cast<uint64_t>(phase / bucket_width);
        uint64_t key = bucket;  // deterministic key per bucket
        if (key != state.last_phase_ppqn) {
            state.last_phase_ppqn = key;
            // Deterministic pseudo-random via std::minstd_rand
            std::minstd_rand gen(static_cast<std::minstd_rand::result_type>(key + 1));
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            state.last_value = dist(gen);
        }
        return state.last_value;
    }

    case Waveform::Noise:
        return static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);

    case Waveform::RampUp:
        return 1.0 - phase;  // high → low (visual ramp up)

    case Waveform::RampDown:
        return phase;  // low → high (visual ramp down)
    }

    return 0.5;
}

double LFOAutomationClip::evaluate(uint64_t ppqn) const {
    if (length_ppqn_ == 0) return offset_;

    // Wrap PPQN within clip length
    ppqn = ppqn % length_ppqn_;

    // Calculate phase from PPQN
    double cycles_per_ppqn;
    if (rate_synced_) {
        // Note-synced: rate_note_ is fraction of a beat
        // 960 PPQN per beat; a note division of 0.25 = quarter note = 960 PPQN per cycle
        constexpr double kPPQNPerBeat = 960.0;
        cycles_per_ppqn = rate_note_ / kPPQNPerBeat;
    } else {
        // Free-running Hz — approximate using 120 BPM default for PPQN→seconds
        // At 120 BPM, 960 PPQN/beat, 1 PPQN = 0.5/960 ≈ 0.0005208 seconds
        constexpr double kPPQNToSeconds = 0.5 / 960.0;
        cycles_per_ppqn = rate_hz_ * kPPQNToSeconds;
    }

    double phase = std::fma(static_cast<double>(ppqn), cycles_per_ppqn, phase_);
    phase = phase - std::floor(phase);  // wrap to [0, 1)

    double value = evaluate_waveform(phase);

    // Apply bipolar conversion
    if (bipolar_) {
        value = value * 2.0 - 1.0;  // map [0,1] → [-1,1]
    } else {
        // Apply offset for unipolar
        value = value + offset_ - 0.5;  // centre the waveform at offset
        value = std::clamp(value, 0.0, 1.0);
    }

    return value;
}

} // namespace aria::automation
