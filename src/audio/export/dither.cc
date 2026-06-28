#include "dither.h"

#include <cmath>
#include <cstring>
#include <random>

namespace aria {

namespace {

// Thread-local RNG for dither noise generation.
thread_local std::mt19937 tls_rng(std::random_device{}());
thread_local std::uniform_real_distribution<float> tls_uniform(-1.0f, 1.0f);

// Shaped dither state (thread-local).
thread_local double s_err1 = 0.0, s_err2 = 0.0, s_err3 = 0.0, s_err4 = 0.0;
thread_local double s_r1 = 0.0, s_r2 = 0.0;

// Earle's 4-pole noise-shaping filter coefficients.
// These push quantization noise into higher frequencies.
// The filter has the form: H(z) = 1 - A(z) where A(z) are the feedback poles.
static constexpr double kShapeA1 = -2.639;
static constexpr double kShapeA2 =  2.933;
static constexpr double kShapeA3 = -1.547;
static constexpr double kShapeA4 =  0.353;

} // anonymous namespace

// ─── Triangular RNG ─────────────────────────────────────────────

float Dither::triangular_rng() {
    // Sum of two uniform randoms gives triangular PDF centered at 0.
    return tls_uniform(tls_rng) + tls_uniform(tls_rng);
}

// ─── Dither: None ───────────────────────────────────────────────

void Dither::apply_none(float* /*data*/, uint32_t /*frames*/, uint32_t /*channels*/) {
    // No-op — truncation is implicit when converting to integer.
}

// ─── Dither: Triangular (TPDF) ──────────────────────────────────

void Dither::apply_triangular(float* data, uint32_t frames, uint32_t channels) {
    const uint32_t total = frames * channels;
    for (uint32_t i = 0; i < total; ++i) {
        data[i] += triangular_rng();
    }
}

// ─── Dither: Shaped (Noise-shaped) ──────────────────────────────

void Dither::apply_shaped(float* data, uint32_t frames, uint32_t channels) {
    constexpr double kScale = 0.5; // noise amplitude scaling

    for (uint32_t i = 0; i < frames * channels; ++i) {
        // Generate triangular noise
        double white = static_cast<double>(triangular_rng()) * kScale;
        double r = white + s_r1;

        // Shaped noise through 4-pole feedback filter
        double shaped = r
            + s_err1 * kShapeA1
            + s_err2 * kShapeA2
            + s_err3 * kShapeA3
            + s_err4 * kShapeA4;

        // Update error state (quantization error feedback)
        double err = shaped - r;
        s_err4 = s_err3;
        s_err3 = s_err2;
        s_err2 = s_err1;
        s_err1 = err;

        s_r1 = r;
        s_r2 = white;

        // Apply shaped noise
        data[i] += static_cast<float>(shaped);
    }
}

// ─── Reset ──────────────────────────────────────────────────────

void Dither::reset() {
    s_err1 = s_err2 = s_err3 = s_err4 = 0.0;
    s_r1 = s_r2 = 0.0;
}

} // namespace aria
