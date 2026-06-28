#include "src_converter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// SSE2 intrinsics for High quality
#if defined(_MSC_VER)
#  include <intrin.h>
#else
#  include <xmmintrin.h>
#  include <emmintrin.h>
#endif

namespace aria {

// ═══════════════════════════════════════════════════════════════════
// Sinc window function (Kaiser or Blackman-Harris)
// ═══════════════════════════════════════════════════════════════════

namespace {

/// Windowed sinc: sin(x)/x * window(x)
/// Uses a Kaiser window with beta ≈ 8 for Best, beta ≈ 4 for High.
double windowed_sinc(double t, double half_len, double beta) {
    if (std::abs(t) < 1e-12) return 1.0;

    double sinc_val = std::sin(3.14159265358979323846 * t) /
                      (3.14159265358979323846 * t);

    // Kaiser window
    // I0(beta * sqrt(1 - (t/half_len)^2)) / I0(beta)
    double ratio = t / half_len;
    if (std::abs(ratio) > 1.0) return 0.0;

    double arg = beta * std::sqrt(1.0 - ratio * ratio);
    // Bessel I0 approximation (for beta up to 12)
    double num = 1.0;
    double term = 1.0;
    for (int k = 1; k <= 12; ++k) {
        term *= (arg / static_cast<double>(k)) * (arg / static_cast<double>(k)) * 0.25;
        num += term;
    }

    // I0(beta)
    double i0_beta = 1.0;
    term = 1.0;
    for (int k = 1; k <= 12; ++k) {
        term *= (beta / static_cast<double>(k)) * (beta / static_cast<double>(k)) * 0.25;
        i0_beta += term;
    }

    return sinc_val * (num / i0_beta);
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════

SRCConverter::SRCConverter() {
    update_sinc_table();
}

void SRCConverter::reset() {
    frac_pos_ = 0.0;
}

uint32_t SRCConverter::max_output_frames(uint32_t input_frames) const {
    // Estimate: output = ceil(input * ratio) + filter_half_length
    uint32_t half_len = 0;
    switch (quality_) {
    case Quality::Fast:   half_len = 0; break;
    case Quality::Medium: half_len = 1; break;
    case Quality::High:   half_len = 4; break;
    case Quality::Best:   half_len = 32; break;
    }
    return static_cast<uint32_t>(std::ceil(input_frames * ratio_)) + half_len + 2;
}

void SRCConverter::update_sinc_table() {
    ratio_ = static_cast<double>(output_rate_) /
             static_cast<double>(input_rate_);

    if (quality_ != Quality::Best && quality_ != Quality::High) return;
    if (sinc_table_initialized_) return;

    const uint32_t num_points = (quality_ == Quality::Best) ? kSincPointsBest
                                                            : kSincPointsHigh;
    const double half_len = static_cast<double>(num_points) / 2.0;
    const double beta = (quality_ == Quality::Best) ? 8.0 : 4.0;

    auto& table = (quality_ == Quality::Best) ? sinc_table_best_[0]
                                              : sinc_table_high_[0];

    for (uint32_t phase = 0; phase < kSincTableSize; ++phase) {
        double frac = static_cast<double>(phase) /
                      static_cast<double>(kSincTableSize);

        for (uint32_t p = 0; p < num_points; ++p) {
            // Center sinc around index num_points/2 - 1
            double t = static_cast<double>(p) - half_len + 0.5 - frac;
            double coeff = windowed_sinc(t, half_len, beta);

            // Normalize so sum equals 1/ratio (gain preservation)
            table[phase * num_points + p] = static_cast<float>(coeff);
        }

        // Normalize
        double sum = 0.0;
        for (uint32_t p = 0; p < num_points; ++p) {
            sum += table[phase * num_points + p];
        }
        double norm = 1.0 / (sum + 1e-20);
        for (uint32_t p = 0; p < num_points; ++p) {
            table[phase * num_points + p] =
                static_cast<float>(table[phase * num_points + p] * norm);
        }
    }

    sinc_table_initialized_ = true;
}

// ═══════════════════════════════════════════════════════════════════

uint32_t SRCConverter::process(const float* input, uint32_t input_frames,
                                float* output, uint32_t max_output_frames)
{
    if (!input || !output || input_frames == 0 || max_output_frames == 0) return 0;
    if (input_rate_ == 0 || output_rate_ == 0) return 0;

    ratio_ = static_cast<double>(output_rate_) /
             static_cast<double>(input_rate_);

    switch (quality_) {
    case Quality::Fast:
        return process_linear(input, input_frames, output, max_output_frames);
    case Quality::Medium:
        return process_cubic(input, input_frames, output, max_output_frames);
    case Quality::High:
    case Quality::Best:
        return process_sinc(input, input_frames, output, max_output_frames);
    default:
        return process_linear(input, input_frames, output, max_output_frames);
    }
}

// ═══════════════════════════════════════════════════════════════════
// Linear interpolation (Fast)
// ═══════════════════════════════════════════════════════════════════

uint32_t SRCConverter::process_linear(const float* input, uint32_t input_frames,
                                       float* output, uint32_t max_output_frames)
{
    uint32_t out_count = 0;
    double pos = frac_pos_;
    const double step = 1.0 / ratio_;

    while (out_count < max_output_frames) {
        uint32_t idx = static_cast<uint32_t>(pos);
        double frac = pos - static_cast<double>(idx);

        if (idx + 1 >= input_frames) break;

        // Linear interpolation
        double sample = (1.0 - frac) * input[idx] + frac * input[idx + 1];
        output[out_count++] = static_cast<float>(sample);
        pos += step;
    }

    frac_pos_ = pos - static_cast<double>(input_frames);
    return out_count;
}

// ═══════════════════════════════════════════════════════════════════
// Cubic Hermite interpolation (Medium)
// ═══════════════════════════════════════════════════════════════════

uint32_t SRCConverter::process_cubic(const float* input, uint32_t input_frames,
                                      float* output, uint32_t max_output_frames)
{
    uint32_t out_count = 0;
    double pos = frac_pos_;
    const double step = 1.0 / ratio_;

    while (out_count < max_output_frames) {
        uint32_t idx = static_cast<uint32_t>(pos);
        double frac = pos - static_cast<double>(idx);

        // Need 4 points: idx-1, idx, idx+1, idx+2
        if (idx == 0 || idx + 2 >= input_frames) {
            // Edge: fall back to linear
            if (idx + 1 >= input_frames) break;
            double sample = (1.0 - frac) * input[idx] + frac * input[idx + 1];
            output[out_count++] = static_cast<float>(sample);
            pos += step;
            continue;
        }

        double xm1 = static_cast<double>(input[idx - 1]);
        double x0  = static_cast<double>(input[idx]);
        double x1  = static_cast<double>(input[idx + 1]);
        double x2  = static_cast<double>(input[idx + 2]);

        // Catmull-Rom spline (cubic Hermite with tension 0.5)
        // From: https://en.wikipedia.org/wiki/Cubic_Hermite_spline
        double c0 = x0;
        double c1 = (x1 - xm1) * 0.5;
        double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);

        double sample = c0 + c1 * frac + c2 * frac * frac + c3 * frac * frac * frac;
        output[out_count++] = static_cast<float>(sample);
        pos += step;
    }

    frac_pos_ = pos - static_cast<double>(input_frames);
    return out_count;
}

// ═══════════════════════════════════════════════════════════════════
// Windowed-sinc interpolation (High / Best)
// ═══════════════════════════════════════════════════════════════════

uint32_t SRCConverter::process_sinc(const float* input, uint32_t input_frames,
                                     float* output, uint32_t max_output_frames)
{
    const uint32_t num_points = (quality_ == Quality::Best) ? kSincPointsBest
                                                           : kSincPointsHigh;
    const uint32_t half_len = num_points / 2;
    auto& table = (quality_ == Quality::Best) ? sinc_table_best_[0]
                                              : sinc_table_high_[0];

    uint32_t out_count = 0;
    double pos = frac_pos_;
    const double step = 1.0 / ratio_;

    while (out_count < max_output_frames) {
        uint32_t idx = static_cast<uint32_t>(pos);
        double frac = pos - static_cast<double>(idx);

        // Check bounds: need half_len samples on each side
        if (idx < half_len || idx + half_len >= input_frames) {
            if (idx + 1 >= input_frames) break;

            // Edge: fall back to linear
            output[out_count++] = static_cast<float>(
                (1.0 - frac) * input[idx] + frac * input[idx + 1]);
            pos += step;
            continue;
        }

        // Look up sinc coefficients for this phase
        uint32_t phase = static_cast<uint32_t>(frac * kSincTableSize);
        if (phase >= kSincTableSize) phase = kSincTableSize - 1;

        double sample = 0.0;
        const float* coeffs = &table[phase * num_points];

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
        if (quality_ == Quality::High && num_points == 8) {
            // SSE2-accelerated 8-point sinc
            __m128 sum1 = _mm_setzero_ps();
            __m128 sum2 = _mm_setzero_ps();

            // Load first 4 coefficients and samples
            __m128 c1 = _mm_loadu_ps(&coeffs[0]);
            __m128 s1 = _mm_loadu_ps(&input[idx - half_len]);
            sum1 = _mm_add_ps(sum1, _mm_mul_ps(c1, s1));

            // Load next 4 coefficients and samples
            __m128 c2 = _mm_loadu_ps(&coeffs[4]);
            __m128 s2 = _mm_loadu_ps(&input[idx - half_len + 4]);
            sum2 = _mm_add_ps(sum2, _mm_mul_ps(c2, s2));

            // Horizontal sum
            __m128 total = _mm_add_ps(sum1, sum2);
            total = _mm_hadd_ps(total, total);
            total = _mm_hadd_ps(total, total);
            _mm_store_ss(&sample, total);
        } else
#endif
        {
            // Scalar path
            for (uint32_t p = 0; p < num_points; ++p) {
                sample += static_cast<double>(coeffs[p]) *
                          static_cast<double>(input[idx - half_len + p]);
            }
        }

        output[out_count++] = static_cast<float>(sample);
        pos += step;
    }

    frac_pos_ = pos - static_cast<double>(input_frames);
    return out_count;
}

} // namespace aria
