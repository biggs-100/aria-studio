#include "envelope.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aria::automation {

// ─── PPQN → ms conversion ─────────────────────────────────

double EnvelopeClip::ppqn_to_ms(uint64_t delta_ppqn) const {
    // At `tempo_bpm_` BPM and 960 PPQN resolution:
    //   1 beat = 60000 / tempo_bpm_ ms
    //   960 PPQN = 60000 / tempo_bpm_ ms
    //   1 PPQN = 60000 / (960 * tempo_bpm_) ms
    constexpr double kPPQNPerBeat = 960.0;
    return static_cast<double>(delta_ppqn) * 60000.0
         / (kPPQNPerBeat * tempo_bpm_);
}

// ─── ADSR value at a given elapsed time (trigger-relative) ─

double EnvelopeClip::compute_value_at_trigger_ms(double elapsed_ms) const {
    if (elapsed_ms < 0.0) return 0.0;

    double t;
    double phase_start;

    switch (type_) {
    case EnvelopeType::DADSR: {
        // ── Delay phase ──
        if (elapsed_ms < delay_ms_)
            return 0.0;
        elapsed_ms -= delay_ms_;
        break;
    }
    default:
        break;
    }

    // ── Attack phase ──
    if (elapsed_ms < attack_ms_) {
        if (attack_ms_ <= 0.0) return 1.0;
        t = elapsed_ms / attack_ms_;
        return evaluate_segment(t, 0.0, 1.0, attack_curve_, {});
    }
    elapsed_ms -= attack_ms_;

    // ── Hold phase (AHDSR only) ──
    if (type_ == EnvelopeType::AHDSR) {
        if (elapsed_ms < hold_ms_) {
            return 1.0;
        }
        elapsed_ms -= hold_ms_;
    }

    // ── Decay phase ──
    if (type_ == EnvelopeType::AR) {
        // AR: no sustain, release immediately after attack completes
        // but evaluate_gated handles this differently. For compute, return 1.0 after attack.
        return 1.0;
    }

    if (decay_ms_ > 0.0 && elapsed_ms < decay_ms_) {
        t = elapsed_ms / decay_ms_;
        return evaluate_segment(t, 1.0, sustain_level_, decay_curve_, {});
    }
    elapsed_ms -= decay_ms_;

    // ── Sustain phase ──
    if (type_ == EnvelopeType::AD) {
        // AD: decays to 0, not sustain_level
        return 0.0;
    }

    return sustain_level_;
}

// ─── evaluate_gated ───────────────────────────────────────

double EnvelopeClip::evaluate_gated(uint64_t ppqn,
                                     bool gate_on,
                                     uint64_t trigger_ppqn,
                                     uint64_t release_ppqn) const
{
    if (!gate_on && release_ppqn == std::numeric_limits<uint64_t>::max()) {
        // No gate was ever active
        return 0.0;
    }

    if (gate_on) {
        // Gate is held — progress through ADSR from trigger
        const double elapsed_ms = ppqn_to_ms(
            ppqn >= trigger_ppqn ? ppqn - trigger_ppqn : 0);
        return compute_value_at_trigger_ms(elapsed_ms);
    }

    // Gate is off — we are in the release phase
    // Compute the value at the moment of release
    const double release_offset_ms = ppqn_to_ms(
        release_ppqn >= trigger_ppqn ? release_ppqn - trigger_ppqn : 0);
    const double value_at_release = compute_value_at_trigger_ms(release_offset_ms);

    // Now evaluate release from that value
    const double release_elapsed_ms = ppqn_to_ms(
        ppqn >= release_ppqn ? ppqn - release_ppqn : 0);

    if (release_elapsed_ms >= release_ms_)
        return 0.0;

    if (release_ms_ <= 0.0)
        return 0.0;

    const double t = release_elapsed_ms / release_ms_;
    return evaluate_segment(t, value_at_release, 0.0, release_curve_, {});
}

// ─── evaluate_ms (convenience for testing) ────────────────

double EnvelopeClip::evaluate_ms(double elapsed_ms,
                                  bool gate_on,
                                  double trigger_offset_ms,
                                  double release_offset_ms) const
{
    if (!gate_on && release_offset_ms < 0.0) {
        // No gate was ever active
        return 0.0;
    }

    if (gate_on) {
        const double t = elapsed_ms - trigger_offset_ms;
        return compute_value_at_trigger_ms(t);
    }

    // Release phase
    const double value_at_release = compute_value_at_trigger_ms(
        release_offset_ms - trigger_offset_ms);

    const double release_elapsed = elapsed_ms - release_offset_ms;

    if (release_elapsed >= release_ms_)
        return 0.0;
    if (release_ms_ <= 0.0)
        return 0.0;

    const double t = release_elapsed / release_ms_;
    return evaluate_segment(t, value_at_release, 0.0, release_curve_, {});
}

} // namespace aria::automation
