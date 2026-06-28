#ifndef ARIA_AUTOMATION_STEP_SEQUENCER_SOURCE_H
#define ARIA_AUTOMATION_STEP_SEQUENCER_SOURCE_H

#include "modulation_types.h"
#include "modulation_source.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace aria::automation {

/// Step-sequencer modulation source.
///
/// Produces stepped pattern values from 1 to 64 steps with configurable
/// steps-per-beat, smoothing, swing, randomise, and trigger behaviour
/// (Free, NoteOn, NoteGate, Transport).
class StepSequencerSource : public ModulationSource {
public:
    enum class Trigger {
        Free,        ///< Free-running, advances with transport
        NoteOn,      ///< Advances one step per note-on event
        NoteGate,    ///< Resets to step 0 on each note-on
        Transport    ///< Advances with transport, resets on play start
    };

    // ─── Steps ───────────────────────────────────────────────
    void set_step_count(uint32_t steps);
    uint32_t step_count() const { return step_count_; }

    void set_step_value(uint32_t step, double value);
    double step_value(uint32_t step) const;

    void set_steps_per_beat(uint32_t spb) { steps_per_beat_ = std::max(1u, spb); }
    uint32_t steps_per_beat() const { return steps_per_beat_; }

    // ─── Behaviour ────────────────────────────────────────────
    void set_smooth(bool smooth) { smooth_ = smooth; }
    bool smooth() const { return smooth_; }

    void set_swing(double amount) { swing_ = std::clamp(amount, 0.0, 1.0); }
    double swing() const { return swing_; }

    void set_randomize_amount(double amount) {
        randomize_ = std::clamp(amount, 0.0, 1.0);
    }
    double randomize_amount() const { return randomize_; }

    void set_trigger(Trigger t) { trigger_ = t; }
    Trigger trigger() const { return trigger_; }

    // ─── External triggers ────────────────────────────────────
    void advance_step() const { current_step_ = (current_step_ + 1) % step_count_; }
    void reset_to_step(uint32_t step = 0) const { current_step_ = step % step_count_; }

    // ─── Overrides ────────────────────────────────────────────
    double evaluate(uint64_t ppqn,
                    const ModulationContext& ctx) const override;
    void reset() override {
        current_step_ = 0;
    }

private:
    uint32_t step_count_ = 16;
    std::vector<double> step_values_ = std::vector<double>(16, 0.0);
    uint32_t steps_per_beat_ = 4;
    bool smooth_ = false;
    double swing_ = 0.0;
    double randomize_ = 0.0;
    Trigger trigger_ = Trigger::Free;

    /// Current step index. Mutable because evaluate() is const but
    /// transport-driven sequencers advance their step on each call.
    mutable uint32_t current_step_ = 0;
    /// PPQN of last step advance, used for smooth interpolation.
    mutable uint64_t last_step_ppqn_ = 0;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_STEP_SEQUENCER_SOURCE_H
