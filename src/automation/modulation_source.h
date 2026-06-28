#ifndef ARIA_AUTOMATION_MODULATION_SOURCE_H
#define ARIA_AUTOMATION_MODULATION_SOURCE_H

#include "modulation_types.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace aria::automation {

// ─── ModulationSource (Base) ──────────────────────────────

/// Abstract base class for all modulation sources.
///
/// Each source has a stable `SourceID`, a human-readable `name()`,
/// and an `evaluate()` method that returns the current output value
/// given a PPQN position and modulation context.
class ModulationSource {
public:
    virtual ~ModulationSource() = default;

    // ─── Identity ────────────────────────────────────────────
    virtual SourceID id() const { return id_; }
    virtual std::string name() const { return name_; }

    void set_id(SourceID id) { id_ = id; }
    void set_name(const std::string& name) { name_ = name; }

    // ─── Evaluation ──────────────────────────────────────────
    /// Evaluate the source at the given PPQN position.
    ///
    /// @param ppqn  Current PPQN position (for phase computation)
    /// @param ctx   Modulation context (note data, sidechain, engine)
    /// @return Source output. Unipolar sources return [0, 1],
    ///         bipolar sources return [-1, 1].
    virtual double evaluate(uint64_t ppqn,
                            const ModulationContext& ctx) const = 0;

    /// Reset internal state (e.g., on transport start or note-on).
    virtual void reset() {}

protected:
    SourceID id_ = 0;
    std::string name_;
};

// ─── LFOSource ────────────────────────────────────────────

/// Low-frequency oscillator modulation source.
///
/// Produces cyclic modulation from 12 waveform types. Supports
/// note-synced and free-running rates, key-sync, tempo-sync,
/// configurable phase, pulse width, and bipolar output.
class LFOSource : public ModulationSource {
public:
    enum class Waveform {
        Sine,
        Triangle,
        Saw,
        SawInv,
        Square,
        SampleAndHold,
        Noise,
        RampUp,
        RampDown,
        Pulse
    };

    // ─── Rate ────────────────────────────────────────────────
    void set_rate_sync(double note_div) {
        rate_note_ = std::max(0.0, note_div);
        rate_synced_ = true;
    }
    double rate_sync() const { return rate_note_; }

    void set_rate_hz(double hz) {
        rate_hz_ = std::max(0.0, hz);
        rate_synced_ = false;
    }
    double rate_hz() const { return rate_hz_; }
    bool is_rate_synced() const { return rate_synced_; }

    // ─── Shape ───────────────────────────────────────────────
    void set_waveform(Waveform wf) { waveform_ = wf; }
    Waveform waveform() const { return waveform_; }

    void set_pulse_width(double width) {
        pulse_width_ = std::clamp(width, 0.0, 1.0);
    }
    double pulse_width() const { return pulse_width_; }

    void set_phase(double phase) { phase_ = std::clamp(phase, 0.0, 1.0); }
    double phase() const { return phase_; }

    void set_bipolar(bool bipolar) { bipolar_ = bipolar; }
    bool bipolar() const { return bipolar_; }

    void set_smooth(double ms) { smoothing_ms_ = std::max(0.0, ms); }
    double smooth() const { return smoothing_ms_; }

    // ─── Sync ────────────────────────────────────────────────
    void set_key_sync(bool sync) { key_sync_ = sync; }
    bool key_sync() const { return key_sync_; }

    void set_tempo_sync(bool sync) { tempo_sync_ = sync; }
    bool tempo_sync() const { return tempo_sync_; }

    // ─── Key-sync trigger ────────────────────────────────────
    /// Called by the engine on note-on when `key_sync_` is true.
    void trigger_retrigger(uint64_t trigger_ppqn) {
        retrigger_ppqn_ = trigger_ppqn;
    }

    // ─── Overrides ───────────────────────────────────────────
    double evaluate(uint64_t ppqn,
                    const ModulationContext& ctx) const override;
    void reset() override {
        retrigger_ppqn_ = 0;
    }

private:
    double evaluate_waveform(double phase) const;

    Waveform waveform_ = Waveform::Sine;
    double rate_hz_ = 1.0;
    double rate_note_ = 1.0 / 4.0;    // Quarter note default
    bool rate_synced_ = true;
    double pulse_width_ = 0.5;
    double phase_ = 0.0;
    bool bipolar_ = false;
    double smoothing_ms_ = 0.0;
    bool key_sync_ = false;
    bool tempo_sync_ = true;

    /// PPQN of last key-sync retrigger.
    mutable uint64_t retrigger_ppqn_ = 0;
};

// ─── EnvelopeFollowerSource ───────────────────────────────

/// Extracts amplitude envelope from a sidechain audio signal.
///
/// Supports four output modes — Amplitude (raw), Envelope (smoothed),
/// Peak (hold + decay), and RMS (root-mean-square).
class EnvelopeFollowerSource : public ModulationSource {
public:
    enum class OutputMode {
        Amplitude,   ///< Raw instantaneous amplitude
        Envelope,    ///< Attack / release smoothed envelope
        Peak,        ///< Peak detection with hold
        RMS          ///< Root-mean-square level
    };

    // ─── Parameters ──────────────────────────────────────────
    void set_attack_ms(double ms) { attack_ms_ = std::clamp(ms, 0.0, 100.0); }
    double attack_ms() const { return attack_ms_; }

    void set_release_ms(double ms) { release_ms_ = std::clamp(ms, 0.0, 1000.0); }
    double release_ms() const { return release_ms_; }

    void set_output_mode(OutputMode mode) { mode_ = mode; }
    OutputMode output_mode() const { return mode_; }

    /// Set the sample rate for envelope coefficient computation.
    void set_sample_rate(double hz) { sample_rate_ = std::max(1.0, hz); }
    double sample_rate() const { return sample_rate_; }

    // ─── Overrides ───────────────────────────────────────────
    double evaluate(uint64_t ppqn,
                    const ModulationContext& ctx) const override;
    void reset() override { current_envelope_ = 0.0; }

private:
    double attack_ms_ = 5.0;
    double release_ms_ = 50.0;
    OutputMode mode_ = OutputMode::Envelope;
    double sample_rate_ = 48000.0;

    /// Internal envelope state, mutable because evaluate() is const
    /// but the envelope follower tracks a running value.
    mutable double current_envelope_ = 0.0;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_MODULATION_SOURCE_H
