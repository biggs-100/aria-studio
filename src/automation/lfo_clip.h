#ifndef ARIA_AUTOMATION_LFO_CLIP_H
#define ARIA_AUTOMATION_LFO_CLIP_H

#include "automation_clip.h"

#include <cstdint>

namespace aria::automation {

/// LFO-based automation clip — waveform-based cyclic modulation.
///
/// Generates automation values from standard LFO waveforms. Supports
/// note-synced or free-running rate, phase offset, bipolar output,
/// and pulse-width modulation for applicable waveforms.
class LFOAutomationClip : public AutomationClip {
public:
    LFOAutomationClip() = default;

    // ─── Waveform ────────────────────────────────────────────
    enum class Waveform {
        Sine,
        Triangle,
        Saw,
        SawInv,
        Square,
        SampleAndHold,
        Noise,
        RampUp,
        RampDown
    };

    void set_waveform(Waveform wf) { waveform_ = wf; }
    Waveform waveform() const { return waveform_; }

    // ─── Rate ────────────────────────────────────────────────
    void set_rate_hz(double hz) { rate_hz_ = std::max(0.0, hz); rate_synced_ = false; }
    double rate_hz() const { return rate_hz_; }

    void set_rate_sync(double note_div) { rate_note_ = std::max(0.0, note_div); rate_synced_ = true; }
    double rate_sync() const { return rate_note_; }
    bool is_rate_synced() const { return rate_synced_; }

    // ─── Phase / Shape ───────────────────────────────────────
    void set_phase(double phase) { phase_ = std::clamp(phase, 0.0, 1.0); }
    double phase() const { return phase_; }

    void set_pulse_width(double width) { pulse_width_ = std::clamp(width, 0.0, 1.0); }
    double pulse_width() const { return pulse_width_; }

    void set_offset(double offset) { offset_ = std::clamp(offset, 0.0, 1.0); }
    double offset() const { return offset_; }

    void set_bipolar(bool bipolar) { bipolar_ = bipolar; }
    bool bipolar() const { return bipolar_; }

    // ─── Evaluation ──────────────────────────────────────────
    double evaluate(uint64_t ppqn) const override;

private:
    double evaluate_waveform(double phase) const;

    Waveform waveform_ = Waveform::Sine;
    double rate_hz_ = 1.0;
    double rate_note_ = 1.0 / 4.0;   // Quarter note default
    bool rate_synced_ = true;
    double phase_ = 0.0;
    double pulse_width_ = 0.5;
    double offset_ = 0.5;
    bool bipolar_ = false;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_LFO_CLIP_H
