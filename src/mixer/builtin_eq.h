#ifndef ARIA_MIXER_BUILTIN_EQ_H
#define ARIA_MIXER_BUILTIN_EQ_H

#include <cstdint>
#include <vector>
#include <cmath>

namespace aria {

// ── Biquad Filter (RBJ cookbook, zero-latency) ──────────────────
///
/// Second-order IIR filter with per-channel state.
/// Coefficients computed via RBJ cookbook formulas.
class BiquadFilter {
public:
    enum class Type {
        LowPass,
        HighPass,
        BandPass,
        Notch,
        AllPass,
        LowShelf,
        HighShelf,
        Peak
    };

    BiquadFilter();

    /// Configure filter type and parameters.
    void set_type(Type type);
    void set_cutoff(double frequency_hz);   // 20 – 20000 Hz
    void set_q(double q);                    // 0.1 – 10
    void set_gain_db(double gain_db);        // -36 – +36 dB (shelf/peak)

    /// Sample rate must be set before processing.
    void set_sample_rate(double sample_rate);

    /// Recalculate coefficients from current parameters.
    void update_coefficients();

    /// Process interleaved or planar stereo. `channels` must match state count.
    void process(const float* input, float* output, uint32_t frames, uint32_t channels);

    /// Reset internal state (z⁻¹ registers).
    void reset();

    /// Ensure state exists for `num_channels`.
    void ensure_channels(uint32_t num_channels);

    /// Latency is always zero (minimum-phase).
    uint32_t latency() const { return 0; }

private:
    Type   type_ = Type::LowPass;
    double sample_rate_ = 48000.0;

    // Target parameters
    double cutoff_ = 1000.0;
    double q_ = 0.707;
    double gain_db_ = 0.0;

    // Coefficients (normalised — a0 is always 1.0)
    double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0;
    double a1_ = 0.0, a2_ = 0.0;

    // Per-channel delay states: x[n-1], x[n-2], y[n-1], y[n-2]
    struct State {
        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;
    };
    std::vector<State> states_;

    void calculate_coefficients();
    void apply_filter(const float* in, float* out, uint32_t frames, State& st);
};

// ── BuiltInEQ — 8-band Parametric EQ ──────────────────────────
///
/// Zero-latency cascade of biquad filters. Each band is a configurable
/// biquad section (Peak, LowShelf, HighShelf, LowCut, HighCut, etc.).
class BuiltInEQ {
public:
    enum BandType {
        LowShelf,
        HighShelf,
        Peak,
        LowCut,
        HighCut,
        BandPass,
        Notch
    };

    struct Band {
        BandType  type     = Peak;
        double    frequency = 1000.0;  // Hz
        double    gain_db   = 0.0;     // -36 to +36 dB
        double    q         = 0.707;   // 0.1 – 10
        bool      active    = true;
    };

    BuiltInEQ() = default;

    // ── Band management ────────────────────────────────────────
    uint32_t add_band(const Band& band);
    void     remove_band(uint32_t index);
    void     modify_band(uint32_t index, const Band& band);
    void     clear();
    uint32_t band_count() const { return static_cast<uint32_t>(bands_.size()); }
    const Band& get_band(uint32_t index) const;

    // ── Bypass ─────────────────────────────────────────────────
    void set_bypass(bool bypass) { bypassed_ = bypass; }
    bool is_bypassed() const     { return bypassed_; }

    // ── Sample rate ────────────────────────────────────────────
    void set_sample_rate(double sample_rate);

    // ── Processing ─────────────────────────────────────────────
    /// Process audio through the EQ cascade.
    /// When bypassed, copies input to output unchanged.
    void process(const float* input, float* output, uint32_t frames, uint32_t channels);

    // ── Latency ────────────────────────────────────────────────
    uint32_t latency() const { return 0; }

private:
    std::vector<Band> bands_;
    bool bypassed_ = false;
    double sample_rate_ = 48000.0;

    // Biquad cascade: filters_[band_index][channel_index]
    std::vector<std::vector<BiquadFilter>> filters_;

    void rebuild_filters();
    void configure_band_filter(BiquadFilter& f, const Band& band) const;
};

} // namespace aria

#endif // ARIA_MIXER_BUILTIN_EQ_H
