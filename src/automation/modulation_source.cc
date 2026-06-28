#define _USE_MATH_DEFINES
#include "modulation_source.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <limits>

namespace aria::automation {

// ===================================================================
// LFOSource
// ===================================================================

namespace {

    /// Compute cycles-per-PPQN for note-synced LFO.
    /// At 960 PPQN per beat, a note division of 0.25 = quarter note.
    constexpr double kPPQNPerBeat = 960.0;

    /// For free-running mode, approximate PPQN→seconds at 120 BPM.
    constexpr double kPPQNToSeconds = 0.5 / 960.0;

    // Deterministic pseudo-random helpers for Noise / S&H
    struct SHState {
        double last_value = 0.5;
        mutable std::minstd_rand rng{42};
    };

    SHState& sh_state() {
        static SHState s;
        return s;
    }

} // anonymous namespace

double LFOSource::evaluate_waveform(double phase) const {
    // phase is in [0, 1) for one complete cycle
    phase = phase - std::floor(phase);

    switch (waveform_) {
    case Waveform::Sine:
        return 0.5 + 0.5 * std::sin(2.0 * M_PI * phase);

    case Waveform::Triangle:
        return phase < 0.5
            ? 2.0 * phase
            : 2.0 * (1.0 - phase);

    case Waveform::Saw:
        return phase;

    case Waveform::SawInv:
        return 1.0 - phase;

    case Waveform::Square:
        return phase < pulse_width_ ? 1.0 : 0.0;

    case Waveform::Pulse:
        // Pulse waveform: variable width pulse, opposite of square
        return phase < pulse_width_ ? 0.0 : 1.0;

    case Waveform::SampleAndHold: {
        // Hold a random value each pulse_width interval
        double bucket_width = pulse_width_ > 0.01 ? pulse_width_ : 0.5;
        uint64_t bucket = static_cast<uint64_t>(phase / bucket_width);
        uint64_t key = bucket;
        if (key != sh_state().rng()) { // force re-eval using deterministic seed
            std::minstd_rand gen(static_cast<std::minstd_rand::result_type>(key + 1));
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            sh_state().last_value = dist(gen);
        }
        return sh_state().last_value;
    }

    case Waveform::Noise: {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(sh_state().rng);
    }

    case Waveform::RampUp:
        return 1.0 - phase;

    case Waveform::RampDown:
        return phase;
    }

    return 0.5;
}

double LFOSource::evaluate(uint64_t ppqn, const ModulationContext& /*ctx*/) const {
    double cycles_per_ppqn;
    if (rate_synced_) {
        cycles_per_ppqn = rate_note_ / kPPQNPerBeat;
    } else {
        cycles_per_ppqn = rate_hz_ * kPPQNToSeconds;
    }

    // Apply key-sync offset if triggered
    uint64_t effective_ppqn = ppqn;
    if (key_sync_ && retrigger_ppqn_ > 0 && ppqn >= retrigger_ppqn_) {
        effective_ppqn = ppqn - retrigger_ppqn_;
    }

    double phase = std::fma(static_cast<double>(effective_ppqn),
                            cycles_per_ppqn, phase_);
    phase = phase - std::floor(phase);

    double value = evaluate_waveform(phase);

    if (bipolar_) {
        value = value * 2.0 - 1.0;               // [0, 1] → [-1, 1]
    }

    return value;
}

// ===================================================================
// EnvelopeFollowerSource
// ===================================================================

double EnvelopeFollowerSource::evaluate(uint64_t /*ppqn*/,
                                        const ModulationContext& ctx) const
{
    double input = ctx.sidechain_amplitude;
    input = std::clamp(input, 0.0, 1.0);

    // Compute filter coefficients from time constants
    const double attack_coeff  = attack_ms_  > 0.0
        ? std::exp(-1.0 / (attack_ms_  * 0.001 * sample_rate_))
        : 0.0;
    const double release_coeff = release_ms_ > 0.0
        ? std::exp(-1.0 / (release_ms_ * 0.001 * sample_rate_))
        : 0.0;

    // Track envelope with separate attack / release rates
    if (input > current_envelope_) {
        current_envelope_ = attack_coeff  * current_envelope_
                          + (1.0 - attack_coeff) * input;
    } else {
        current_envelope_ = release_coeff * current_envelope_
                          + (1.0 - release_coeff) * input;
    }

    switch (mode_) {
    case OutputMode::Amplitude:
        return input;
    case OutputMode::Envelope:
        return current_envelope_;
    case OutputMode::Peak:
        return std::max(input, current_envelope_);
    case OutputMode::RMS:
        // Approximate RMS from envelope tracking
        return std::sqrt(current_envelope_);
    }

    return current_envelope_;
}

} // namespace aria::automation
