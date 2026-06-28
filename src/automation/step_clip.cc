#include "step_clip.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aria::automation {

void StepAutomationClip::set_step_count(uint32_t steps) {
    step_count_ = std::clamp(steps, 1u, 64u);
    step_values_.resize(step_count_, 0.5);
}

void StepAutomationClip::set_step_value(uint32_t step, double value) {
    if (step < step_values_.size()) {
        step_values_[step] = std::clamp(value, 0.0, 1.0);
    }
}

double StepAutomationClip::step_value(uint32_t step) const {
    if (step < step_values_.size()) {
        return step_values_[step];
    }
    return 0.0;
}

double StepAutomationClip::evaluate(uint64_t ppqn) const {
    if (step_count_ == 0 || step_values_.empty()) return 0.0;
    if (length_ppqn_ == 0) return step_values_.front();

    // Wrap PPQN within the clip length
    if (length_ppqn_ > 0) {
        ppqn = ppqn % length_ppqn_;
    }

    // Compute step width in PPQN
    const double step_width = static_cast<double>(length_ppqn_)
                            / static_cast<double>(step_count_);

    // Apply swing: offset the PPQN position based on which step we're on
    double adjusted_ppqn = static_cast<double>(ppqn);
    if (swing_ > 0.0) {
        // Swing affects even-numbered steps (0-indexed)
        const double raw_step = adjusted_ppqn / step_width;
        const uint32_t step_idx = static_cast<uint32_t>(raw_step) % step_count_;
        if (step_idx % 2 == 1) {
            // Shift even steps forward by swing amount
            adjusted_ppqn += swing_ * step_width * 0.5;
        }
    }

    // Determine current step index
    const double raw_step = adjusted_ppqn / step_width;
    const uint32_t step_idx = std::min(static_cast<uint32_t>(raw_step), step_count_ - 1);

    if (!smooth_) {
        // Discrete step value
        return step_values_[step_idx];
    }

    // Smooth interpolation between adjacent steps
    const double frac = raw_step - std::floor(raw_step);
    const uint32_t next_idx = std::min(step_idx + 1, step_count_ - 1);

    const double v0 = step_values_[step_idx];
    const double v1 = step_values_[next_idx];

    // Linear crossfade between steps
    return v0 + (v1 - v0) * frac;
}

} // namespace aria::automation
