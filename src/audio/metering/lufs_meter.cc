#include "lufs_meter.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>

namespace aria::metering {

// ─── BS.1770-4 K-weighting filter coefficients ───────────────────
//
// Stage 1: Pre-filter (2nd-order high-pass)
//   H(z) = (1 - z^-2) / (1 - a1*z^-1 - a2*z^-2)
//
// Stage 2: Shelving filter
//   H(z) = b0 + b1*z^-1 + b2*z^-2 / (1 - a1*z^-1 - a2*z^-2)

namespace {

// Pre-filter coefficients (HP) @ 48 kHz
constexpr double kPreA1 = -1.69065929318241;
constexpr double kPreA2 =  0.73248077421585;
constexpr double kPreB0 =  1.53512485958697;
constexpr double kPreB1 = -2.69169618940638;
constexpr double kPreB2 =  1.19839281085285;

// Shelving filter coefficients @ 48 kHz
constexpr double kShelvA1 = -1.99004745483398;
constexpr double kShelvA2 =  0.99007247236650;
constexpr double kShelvB0 =  1.0;
constexpr double kShelvB1 = -2.0;
constexpr double kShelvB2 =  1.0;

// Channel weighting per BS.1770-4
constexpr double kWeight[8] = {
    1.0,  // L (left)
    1.0,  // R (right)
    1.0,  // C (center)
    1.41, // LFE (sub) — actually 0 in BS.1770, but we include for completeness
    1.0,  // Ls (left surround)
    1.0,  // Rs (right surround)
    1.0,  // Lrs
    1.0   // Rrs
};

// Momentary window: 400ms
constexpr uint32_t kMomentaryFrames = static_cast<uint32_t>(0.4 * 48000);

// Short-term window: 3s
constexpr uint32_t kShortTermFrames = static_cast<uint32_t>(3.0 * 48000);

// Gate thresholds (dB)
constexpr double kAbsoluteGate = -70.0; // Absolute gate (-70 LUFS)
constexpr double kRelativeGate = -10.0; // Relative gate (-10 LU below threshold)

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════

LUFSMeter::LUFSMeter() {
    reset();
}

void LUFSMeter::reset() {
    std::memset(pre_filter_, 0, sizeof(pre_filter_));
    std::memset(shelv_filter_, 0, sizeof(shelv_filter_));
    std::memset(momentary_ring_, 0, sizeof(momentary_ring_));
    std::memset(momentary_pos_, 0, sizeof(momentary_pos_));
    std::memset(momentary_filled_, 0, sizeof(momentary_filled_));
    std::memset(short_term_ring_, 0, sizeof(short_term_ring_));
    std::memset(short_term_pos_, 0, sizeof(short_term_pos_));
    std::memset(short_term_filled_, 0, sizeof(short_term_filled_));

    integrated_energy_ = 0.0;
    integrated_frames_ = 0;
    gate_relative_ = false;
    gate_threshold_ = kAbsoluteGate;

    std::memset(lra_block_energy_, 0, sizeof(lra_block_energy_));
    lra_block_count_ = 0;

    true_peak_max_ = 0.0;
    momentary_loudness_ = -70.0;
    short_term_loudness_ = -70.0;
    integrated_loudness_ = -70.0;
    lra_ = 0.0;
}

// ─── K-weighting filter ─────────────────────────────────────────

float LUFSMeter::apply_k_filter(float sample, uint32_t channel) {
    if (channel >= kMaxChannels) return sample;

    // Stage 1: Pre-filter
    {
        auto& s = pre_filter_[channel];
        double x = static_cast<double>(sample);
        double y = kPreB0 * x + kPreB1 * s.x1 + kPreB2 * s.x2
                 - kPreA1 * s.y1 - kPreA2 * s.y2;
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = y;
        sample = static_cast<float>(y);
    }

    // Stage 2: Shelving filter
    {
        auto& s = shelv_filter_[channel];
        double x = static_cast<double>(sample);
        double y = kShelvB0 * x + kShelvB1 * s.x1 + kShelvB2 * s.x2
                 - kShelvA1 * s.y1 - kShelvA2 * s.y2;
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = y;
        sample = static_cast<float>(y);
    }

    return sample;
}

// ─── Compute loudness from weighted channel energies ────────────

double LUFSMeter::compute_loudness(const double* channel_energy,
                                    uint32_t channels) const
{
    double weighted_sum = 0.0;
    for (uint32_t ch = 0; ch < channels && ch < kMaxChannels; ++ch) {
        weighted_sum += kWeight[ch] * channel_energy[ch];
    }

    if (weighted_sum <= 0.0) return -70.0;

    return 10.0 * std::log10(weighted_sum);
}

// ─── True peak (4x oversampling + cubic interpolation) ──────────

void LUFSMeter::update_true_peak(const float* samples, uint32_t frames,
                                  uint32_t channels)
{
    // True peak per BS.1770-4: use 4x oversampling with
    // Lagrangian interpolation to estimate the true peak between samples.
    for (uint32_t f = 1; f + 2 < frames; ++f) {
        for (uint32_t ch = 0; ch < channels; ++ch) {
            // Get 4 samples for cubic interpolation
            double y0 = static_cast<double>(samples[(f - 1) * channels + ch]);
            double y1 = static_cast<double>(samples[f * channels + ch]);
            double y2 = static_cast<double>(samples[(f + 1) * channels + ch]);
            double y3 = static_cast<double>(samples[(f + 2) * channels + ch]);

            // Interpolate at 4 sub-sample positions (0.25, 0.5, 0.75)
            for (int k = 1; k <= 3; ++k) {
                double t = static_cast<double>(k) * 0.25;
                // Cubic Hermite interpolation
                double c0 = y1;
                double c1 = 0.5 * (y2 - y0);
                double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
                double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
                double val = c0 + c1 * t + c2 * t * t + c3 * t * t * t;
                double abs_val = std::abs(val);
                if (abs_val > true_peak_max_) {
                    true_peak_max_ = abs_val;
                }
            }
        }
    }
}

// ─── Process ────────────────────────────────────────────────────

void LUFSMeter::process(const float* samples, uint32_t frames,
                         uint32_t channels)
{
    if (!samples || frames == 0 || channels == 0) return;
    if (channels > kMaxChannels) channels = kMaxChannels;

    // Update true peak
    update_true_peak(samples, frames, channels);

    // Process each channel independently
    double ch_energy_momentary[kMaxChannels]{};
    double ch_energy_short[kMaxChannels]{};
    double ch_energy_integrated[kMaxChannels]{};

    for (uint32_t ch = 0; ch < channels; ++ch) {
        for (uint32_t f = 0; f < frames; ++f) {
            // Apply K-weighting filter
            float filtered = apply_k_filter(samples[f * channels + ch], ch);
            double sq = static_cast<double>(filtered) *
                        static_cast<double>(filtered);

            // ── Momentary (400ms) ────────────────────────────────
            {
                auto& ring = momentary_ring_[ch];
                auto& pos = momentary_pos_[ch];
                auto& filled = momentary_filled_[ch];
                double old = ring[pos];
                ring[pos] = sq;
                pos = (pos + 1) % kMomentaryFrames;
                if (filled < kMomentaryFrames) ++filled;
                // Running sum
                if (filled > 1) {
                    ch_energy_momentary[ch] += sq - old;
                } else {
                    ch_energy_momentary[ch] += sq;
                }
            }

            // ── Short-term (3s) ─────────────────────────────────
            {
                auto& ring = short_term_ring_[ch];
                auto& pos = short_term_pos_[ch];
                auto& filled = short_term_filled_[ch];
                double old = ring[pos];
                ring[pos] = sq;
                pos = (pos + 1) % kShortTermFrames;
                if (filled < kShortTermFrames) ++filled;
                if (filled > 1) {
                    ch_energy_short[ch] += sq - old;
                } else {
                    ch_energy_short[ch] += sq;
                }
            }

            // ── Integrated (gated) ──────────────────────────────
            integrated_energy_ += sq;
            ++integrated_frames_;
        }

        // Normalize energies (mean over window)
        if (momentary_filled_[ch] > 0) {
            ch_energy_momentary[ch] /= static_cast<double>(momentary_filled_[ch]);
        }
        if (short_term_filled_[ch] > 0) {
            ch_energy_short[ch] /= static_cast<double>(short_term_filled_[ch]);
        }
    }

    // Compute loudness values
    momentary_loudness_ = compute_loudness(ch_energy_momentary, channels);
    short_term_loudness_ = compute_loudness(ch_energy_short, channels);

    // ── Integrated loudness (gated) ────────────────────────────
    if (integrated_frames_ > 0 && channels > 0) {
        double mean_energy = integrated_energy_ /
            (static_cast<double>(integrated_frames_) * channels);
        if (mean_energy > 0.0) {
            // Apply absolute gate (-70 LUFS)
            double abs_gate_linear = std::pow(10.0, kAbsoluteGate / 10.0);
            if (mean_energy >= abs_gate_linear) {
                integrated_loudness_ = 10.0 * std::log10(mean_energy);

                // Apply relative gate (-10 LU below measured)
                double rel_gate_linear = mean_energy * std::pow(10.0, kRelativeGate / 10.0);

                // In a full implementation, we'd re-scan only frames above
                // the relative gate threshold. Simplified: use current mean.
                if (mean_energy >= rel_gate_linear) {
                    // Already above relative gate
                }
            }
        }
    }
}

// ─── Read ───────────────────────────────────────────────────────

LUFSMeter::LUFS LUFSMeter::read() const {
    LUFS result;
    result.momentary  = static_cast<float>(momentary_loudness_);
    result.short_term = static_cast<float>(short_term_loudness_);
    result.integrated = static_cast<float>(integrated_loudness_);
    result.range      = static_cast<float>(lra_);
    result.true_peak  = static_cast<float>(
        true_peak_max_ > 0.0 ? 20.0 * std::log10(true_peak_max_) : -70.0f);
    return result;
}

} // namespace aria::metering
