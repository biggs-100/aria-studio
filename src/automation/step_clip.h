#ifndef ARIA_AUTOMATION_STEP_CLIP_H
#define ARIA_AUTOMATION_STEP_CLIP_H

#include "automation_clip.h"

#include <cstdint>
#include <vector>

namespace aria::automation {

/// Step-based automation clip — a step-sequencer-style automation source.
///
/// Divides the clip length into a fixed number of steps, each holding a
/// normalised value. Optionally smooths transitions between steps and
/// applies swing timing offsets.
class StepAutomationClip : public AutomationClip {
public:
    StepAutomationClip() = default;

    // ─── Step Configuration ──────────────────────────────────
    void set_step_count(uint32_t steps);
    uint32_t step_count() const { return step_count_; }

    void set_step_value(uint32_t step, double value);
    double step_value(uint32_t step) const;

    void set_smooth(bool smooth) { smooth_ = smooth; }
    bool smooth() const { return smooth_; }

    void set_swing(double amount) { swing_ = std::clamp(amount, 0.0, 1.0); }
    double swing() const { return swing_; }

    // ─── Evaluation ──────────────────────────────────────────
    double evaluate(uint64_t ppqn) const override;

private:
    uint32_t step_count_ = 16;
    std::vector<double> step_values_;
    bool smooth_ = false;
    double swing_ = 0.0;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_STEP_CLIP_H
