#include "step_sequencer_source.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace aria::automation {

void StepSequencerSource::set_step_count(uint32_t steps) {
    steps = std::clamp(steps, 1u, 64u);
    step_values_.resize(steps, 0.0);
    step_count_ = steps;
    current_step_ = std::min(current_step_, step_count_ - 1);
}

void StepSequencerSource::set_step_value(uint32_t step, double value) {
    if (step < step_values_.size()) {
        step_values_[step] = std::clamp(value, 0.0, 1.0);
    }
}

double StepSequencerSource::step_value(uint32_t step) const {
    if (step < step_values_.size()) return step_values_[step];
    return 0.0;
}

double StepSequencerSource::evaluate(uint64_t ppqn,
                                     const ModulationContext& /*ctx*/) const
{
    if (step_values_.empty()) return 0.0;

    // Advance step for Free / Transport triggers based on PPQN
    if (trigger_ == Trigger::Free || trigger_ == Trigger::Transport) {
        // Steps advance every `steps_per_beat_` step per beat
        // i.e., at PPQN intervals of 960 / steps_per_beat_
        constexpr double kPPQNPerBeat = 960.0;
        double step_interval = kPPQNPerBeat / static_cast<double>(steps_per_beat_);
        if (step_interval > 0.0) {
            uint32_t target_step = static_cast<uint32_t>(
                static_cast<double>(ppqn) / step_interval) % step_count_;
            if (target_step != current_step_) {
                last_step_ppqn_ = ppqn;
                current_step_ = target_step;
            }
        }
    }
    // NoteOn / NoteGate advance is triggered externally via
    // advance_step() / reset_to_step().

    double raw = step_values_[current_step_];

    // Apply randomize variation
    if (randomize_ > 0.0) {
        std::minstd_rand gen(static_cast<std::minstd_rand::result_type>(
            current_step_ + 1));
        std::uniform_real_distribution<double> dist(-randomize_, randomize_);
        raw = std::clamp(raw + dist(gen), 0.0, 1.0);
    }

    // Apply swing (time offset not applied here — swing affects step timing
    // in the transport layer; value swing is a simple offset)
    if (swing_ > 0.0 && current_step_ % 2 == 1) {
        raw = std::clamp(raw + swing_ * 0.1, 0.0, 1.0);
    }

    // Smooth interpolation between current and next step
    if (smooth_) {
        uint32_t next_step = (current_step_ + 1) % step_count_;
        double next_raw = step_values_[next_step];
        if (randomize_ > 0.0) {
            std::minstd_rand gen(static_cast<std::minstd_rand::result_type>(
                next_step + 1));
            std::uniform_real_distribution<double> dist(-randomize_, randomize_);
            next_raw = std::clamp(next_raw + dist(gen), 0.0, 1.0);
        }

        // Interpolate based on position within the current step
        constexpr double kPPQNPerBeat = 960.0;
        double step_interval = kPPQNPerBeat / static_cast<double>(steps_per_beat_);
        if (step_interval > 0.0) {
            double t = static_cast<double>(ppqn - last_step_ppqn_) / step_interval;
            t = std::clamp(t, 0.0, 1.0);
            raw = raw + (next_raw - raw) * t;
        }
    }

    return std::clamp(raw, 0.0, 1.0);
}

} // namespace aria::automation
