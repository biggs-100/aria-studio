#include "mixer/builtin_eq.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace aria {

// ═══════════════════════════════════════════════════════════════════
//  BiquadFilter
// ═══════════════════════════════════════════════════════════════════

BiquadFilter::BiquadFilter() = default;

void BiquadFilter::set_type(Type type) {
    if (type_ != type) {
        type_ = type;
        update_coefficients();
    }
}

void BiquadFilter::set_cutoff(double frequency_hz) {
    double f = std::clamp(frequency_hz, 20.0, 20000.0);
    if (cutoff_ != f) {
        cutoff_ = f;
        update_coefficients();
    }
}

void BiquadFilter::set_q(double q) {
    double qq = std::clamp(q, 0.1, 10.0);
    if (q_ != qq) {
        q_ = qq;
        update_coefficients();
    }
}

void BiquadFilter::set_gain_db(double gain_db) {
    double g = std::clamp(gain_db, -36.0, 36.0);
    if (gain_db_ != g) {
        gain_db_ = g;
        update_coefficients();
    }
}

void BiquadFilter::set_sample_rate(double sample_rate) {
    if (sample_rate_ != sample_rate) {
        sample_rate_ = sample_rate;
        update_coefficients();
    }
}

void BiquadFilter::ensure_channels(uint32_t num_channels) {
    if (num_channels > states_.size()) {
        states_.resize(num_channels);
    }
}

void BiquadFilter::reset() {
    for (auto& st : states_) {
        st.x1 = 0.0; st.x2 = 0.0;
        st.y1 = 0.0; st.y2 = 0.0;
    }
}

// ── RBJ Cookbook coefficient calculation ─────────────────────────
void BiquadFilter::calculate_coefficients() {
    double w0  = 2.0 * std::numbers::pi_v<double> * cutoff_ / sample_rate_;
    double cos_w0 = std::cos(w0);
    double sin_w0 = std::sin(w0);
    double alpha = sin_w0 / (2.0 * q_);
    double A     = std::pow(10.0, gain_db_ / 40.0);

    double a0 = 1.0;
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    switch (type_) {
    case Type::LowPass:
        b0 =  (1.0 - cos_w0) / 2.0;
        b1 =   1.0 - cos_w0;
        b2 =  (1.0 - cos_w0) / 2.0;
        a0 =   1.0 + alpha;
        a1 =  -2.0 * cos_w0;
        a2 =   1.0 - alpha;
        break;

    case Type::HighPass:
        b0 =  (1.0 + cos_w0) / 2.0;
        b1 = -(1.0 + cos_w0);
        b2 =  (1.0 + cos_w0) / 2.0;
        a0 =   1.0 + alpha;
        a1 =  -2.0 * cos_w0;
        a2 =   1.0 - alpha;
        break;

    case Type::BandPass:
        b0 =  alpha;
        b1 =  0.0;
        b2 = -alpha;
        a0 =  1.0 + alpha;
        a1 = -2.0 * cos_w0;
        a2 =  1.0 - alpha;
        break;

    case Type::Notch:
        b0 =  1.0;
        b1 = -2.0 * cos_w0;
        b2 =  1.0;
        a0 =  1.0 + alpha;
        a1 = -2.0 * cos_w0;
        a2 =  1.0 - alpha;
        break;

    case Type::AllPass:
        b0 =  1.0 - alpha;
        b1 = -2.0 * cos_w0;
        b2 =  1.0 + alpha;
        a0 =  1.0 + alpha;
        a1 = -2.0 * cos_w0;
        a2 =  1.0 - alpha;
        break;

    case Type::LowShelf: {
        double sqrtA = std::sqrt(A);
        // RBJ low-shelf: S=1 (Q-based slope)
        b0 = A * ((A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * sqrtA * alpha);
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w0);
        b2 = A * ((A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * sqrtA * alpha);
        a0 = (A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * sqrtA * alpha;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_w0);
        a2 = (A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * sqrtA * alpha;
        break;
    }

    case Type::HighShelf: {
        double sqrtA = std::sqrt(A);
        b0 = A * ((A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * sqrtA * alpha);
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0);
        b2 = A * ((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * sqrtA * alpha);
        a0 = (A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * sqrtA * alpha;
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos_w0);
        a2 = (A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * sqrtA * alpha;
        break;
    }

    case Type::Peak:
        b0 = 1.0 + alpha * A;
        b1 = -2.0 * cos_w0;
        b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A;
        a1 = -2.0 * cos_w0;
        a2 = 1.0 - alpha / A;
        break;
    }

    // Normalise by a0
    double inv_a0 = 1.0 / a0;
    b0_ = b0 * inv_a0;
    b1_ = b1 * inv_a0;
    b2_ = b2 * inv_a0;
    a1_ = a1 * inv_a0;
    a2_ = a2 * inv_a0;
}

void BiquadFilter::update_coefficients() {
    if (sample_rate_ <= 0.0) return;
    // Nyquist guard: avoid division by zero and unstable coefficients
    if (cutoff_ >= sample_rate_ * 0.49) {
        // Clamp cutoff to just below Nyquist
        cutoff_ = sample_rate_ * 0.49;
    }
    calculate_coefficients();
}

// ── Processing ───────────────────────────────────────────────────

void BiquadFilter::apply_filter(const float* in, float* out, uint32_t frames, State& st) {
    double x1 = st.x1, x2 = st.x2;
    double y1 = st.y1, y2 = st.y2;

    for (uint32_t i = 0; i < frames; ++i) {
        double x = static_cast<double>(in[i]);
        double y = b0_ * x + b1_ * x1 + b2_ * x2
                         - a1_ * y1 - a2_ * y2;
        out[i] = static_cast<float>(y);

        // Shift state
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
    }

    st.x1 = x1; st.x2 = x2;
    st.y1 = y1; st.y2 = y2;
}

void BiquadFilter::process(const float* input, float* output,
                           uint32_t frames, uint32_t channels) {
    ensure_channels(channels);

    for (uint32_t ch = 0; ch < channels; ++ch) {
        const float* in  = input  + ch * frames;
        float*       out = output + ch * frames;
        apply_filter(in, out, frames, states_[ch]);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  BuiltInEQ
// ═══════════════════════════════════════════════════════════════════

uint32_t BuiltInEQ::add_band(const Band& band) {
    bands_.push_back(band);
    rebuild_filters();
    return static_cast<uint32_t>(bands_.size()) - 1;
}

void BuiltInEQ::remove_band(uint32_t index) {
    if (index < bands_.size()) {
        bands_.erase(bands_.begin() + static_cast<ptrdiff_t>(index));
        rebuild_filters();
    }
}

void BuiltInEQ::modify_band(uint32_t index, const Band& band) {
    if (index < bands_.size()) {
        bands_[index] = band;
        if (index < filters_.size()) {
            for (auto& f : filters_[index]) {
                configure_band_filter(f, band);
            }
        }
    }
}

void BuiltInEQ::clear() {
    bands_.clear();
    filters_.clear();
}

const BuiltInEQ::Band& BuiltInEQ::get_band(uint32_t index) const {
    // Out-of-range returns last band (or a static default if empty)
    // Caller should check band_count() first.
    static const Band kDefault;
    if (index >= bands_.size()) return kDefault;
    return bands_[index];
}

void BuiltInEQ::set_sample_rate(double sample_rate) {
    sample_rate_ = sample_rate;
    for (auto& band_filters : filters_) {
        for (auto& f : band_filters) {
            f.set_sample_rate(sample_rate);
        }
    }
}

void BuiltInEQ::configure_band_filter(BiquadFilter& f, const Band& band) const {
    f.set_sample_rate(sample_rate_);

    // Map BandType → BiquadFilter::Type
    using BF = BiquadFilter::Type;
    switch (band.type) {
    case LowShelf:  f.set_type(BF::LowShelf);  break;
    case HighShelf: f.set_type(BF::HighShelf); break;
    case Peak:      f.set_type(BF::Peak);      break;
    case LowCut:    f.set_type(BF::HighPass);  break;
    case HighCut:   f.set_type(BF::LowPass);   break;
    case BandPass:  f.set_type(BF::BandPass);  break;
    case Notch:     f.set_type(BF::Notch);     break;
    }

    f.set_cutoff(band.frequency);
    f.set_gain_db(band.gain_db);
    f.set_q(band.q);
    f.reset();
}

void BuiltInEQ::rebuild_filters() {
    // Default stereo (2 channels)
    const uint32_t kChannels = 2;
    filters_.resize(bands_.size());

    for (uint32_t bi = 0; bi < bands_.size(); ++bi) {
        filters_[bi].resize(kChannels);
        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            configure_band_filter(filters_[bi][ch], bands_[bi]);
        }
    }
}

void BuiltInEQ::process(const float* input, float* output,
                        uint32_t frames, uint32_t channels) {
    if (bypassed_ || bands_.empty()) {
        // Pass-through
        if (input != output) {
            std::memcpy(output, input, frames * channels * sizeof(float));
        }
        return;
    }

    // Cascade: each band processes the previous band's output
    // First band reads from input
    // Intermediate bands process through a temp buffer
    // Last band writes to output

    // If only 1 band, process directly input → output
    if (bands_.size() == 1) {
        for (auto& f : filters_[0]) {
            f.reset();
        }
        filters_[0][0].process(input, output, frames, channels);
        return;
    }

    // Multi-band cascade: use temporary buffer for intermediate stages
    std::vector<float> temp(frames * channels);
    const float* src = input;

    for (uint32_t bi = 0; bi < bands_.size(); ++bi) {
        const auto& band = bands_[bi];
        if (!band.active) continue;

        float* dst = (bi == bands_.size() - 1) ? output : temp.data();

        // Process this band; filter state persists across calls for continuity
        for (uint32_t ch = 0; ch < channels; ++ch) {
            filters_[bi][ch].process(
                src + ch * frames,
                dst + ch * frames,
                frames, 1);
        }

        src = dst;
    }

    // If the last band was inactive or we had no active bands, copy
    if (bands_.empty()) {
        if (input != output) {
            std::memcpy(output, input, frames * channels * sizeof(float));
        }
    }
}

} // namespace aria
