#ifndef ARIA_AUTOMATION_ENVELOPE_H
#define ARIA_AUTOMATION_ENVELOPE_H

#include "automation_clip.h"

#include <cstdint>
#include <limits>

namespace aria::automation {

/// Envelope automation clip — ADSR / AHDSR / DADSR with gated evaluation.
///
/// Supports four envelope shapes:
///   - ADSR   : Attack → Decay → Sustain → Release
///   - AHDSR  : Attack → Hold → Decay → Sustain → Release
///   - AD     : Attack → Decay (no sustain; releases to 0)
///   - AR     : Attack → Release
///   - DADSR  : Delay → Attack → Decay → Sustain → Release
///
/// Each segment can use an independent InterpolationType curve.
class EnvelopeClip : public AutomationClip {
public:
    // ─── Envelope Shape ──────────────────────────────────────
    enum class EnvelopeType {
        ADSR,     ///< Attack-Decay-Sustain-Release
        AHDSR,    ///< Attack-Hold-Decay-Sustain-Release
        AD,       ///< Attack-Decay (no sustain)
        AR,       ///< Attack-Release
        DADSR     ///< Delay-Attack-Decay-Sustain-Release
    };

    void set_type(EnvelopeType type) { type_ = type; }
    EnvelopeType type() const { return type_; }

    // ─── Timing Parameters (milliseconds) ────────────────────
    void set_delay(double ms)      { delay_ms_ = std::max(0.0, ms); }
    void set_attack(double ms)     { attack_ms_ = std::max(0.0, ms); }
    void set_hold(double ms)       { hold_ms_ = std::max(0.0, ms); }
    void set_decay(double ms)      { decay_ms_ = std::max(0.0, ms); }
    void set_sustain(double level) { sustain_level_ = std::clamp(level, 0.0, 1.0); }
    void set_release(double ms)    { release_ms_ = std::max(0.0, ms); }

    double delay_ms()   const { return delay_ms_; }
    double attack_ms()  const { return attack_ms_; }
    double hold_ms()    const { return hold_ms_; }
    double decay_ms()   const { return decay_ms_; }
    double sustain()    const { return sustain_level_; }
    double release_ms() const { return release_ms_; }

    // ─── Per-Segment Curves ──────────────────────────────────
    void set_attack_curve(InterpolationType c)  { attack_curve_ = c; }
    void set_decay_curve(InterpolationType c)   { decay_curve_ = c; }
    void set_release_curve(InterpolationType c) { release_curve_ = c; }

    InterpolationType attack_curve()  const { return attack_curve_; }
    InterpolationType decay_curve()   const { return decay_curve_; }
    InterpolationType release_curve() const { return release_curve_; }

    // ─── Tempo Reference ─────────────────────────────────────
    void set_tempo_bpm(double bpm) { tempo_bpm_ = std::max(1.0, bpm); }
    double tempo_bpm() const { return tempo_bpm_; }

    // ─── Gated Evaluation ────────────────────────────────────
    /// Evaluate the envelope gated signal.
    ///
    /// @param ppqn            Current PPQN position
    /// @param gate_on         True while the gate (key/note-on) is held
    /// @param trigger_ppqn    PPQN when the gate turned ON (trigger / note-on)
    /// @param release_ppqn    PPQN when the gate turned OFF (note-off).
    ///                        Set to UINT64_MAX when no release has occurred
    ///                        (gate has been on since trigger).
    ///                        Must be <= ppqn when release is active.
    double evaluate_gated(uint64_t ppqn,
                           bool gate_on,
                           uint64_t trigger_ppqn,
                           uint64_t release_ppqn = std::numeric_limits<uint64_t>::max()) const;

    // ─── Convenience: evaluate at time_ms directly ───────────
    /// Evaluate using elapsed milliseconds instead of PPQN (for testing).
    double evaluate_ms(double elapsed_ms, bool gate_on,
                        double trigger_offset_ms,
                        double release_offset_ms = -1.0) const;

private:
    // Internal helpers
    double compute_value_at_trigger_ms(double elapsed_ms) const;
    double ppqn_to_ms(uint64_t delta_ppqn) const;

    EnvelopeType type_ = EnvelopeType::ADSR;
    double delay_ms_ = 0.0;
    double attack_ms_ = 10.0;
    double hold_ms_ = 0.0;
    double decay_ms_ = 300.0;
    double sustain_level_ = 0.7;
    double release_ms_ = 500.0;

    InterpolationType attack_curve_  = InterpolationType::EaseOut;
    InterpolationType decay_curve_   = InterpolationType::EaseIn;
    InterpolationType release_curve_ = InterpolationType::EaseOut;

    double tempo_bpm_ = 120.0;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_ENVELOPE_H
