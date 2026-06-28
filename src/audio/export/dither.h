#ifndef ARIA_DITHER_H
#define ARIA_DITHER_H

#include <cstdint>

namespace aria {

/// Dither type selection for audio export.
enum class DitherType {
    None,        ///< No dithering — truncate directly.
    Triangular,  ///< Triangular Probability Density Function (TPDF).
    Shaped       ///< Noise-shaped dither (4-pole Earle's filter).
};

/// Dither noise generator for reducing quantization distortion.
///
/// Applies noise before reducing bit depth to decorrelate quantization
/// error from the signal. Triangular dither shapes noise as uniform
/// ±1 LSB, while shaped dither pushes noise energy into less audible
/// frequency ranges.
class Dither {
public:
    /// No dither — performs direct truncation only.
    static void apply_none(float* data, uint32_t frames, uint32_t channels);

    /// Triangular PDF dither — ±1 LSB uniform triangular noise.
    /// The noise spans [−1, +1] LSB with triangular distribution,
    /// which gives a flat noise floor with zero mean and no DC offset.
    static void apply_triangular(float* data, uint32_t frames, uint32_t channels);

    /// Noise-shaped dither using a 4-pole Earle's filter.
    /// Shapes quantization noise into higher frequencies where human
    /// hearing is less sensitive (above ~15 kHz).
    static void apply_shaped(float* data, uint32_t frames, uint32_t channels);

    /// Reset internal state for shaped dither.
    static void reset();

private:
    /// Get the next triangular random value (±1.0 range).
    static float triangular_rng();

    /// Thread-local storage for the shaped dither filter state.
    struct ShapedState {
        double err1 = 0.0, err2 = 0.0, err3 = 0.0, err4 = 0.0;
        double r1 = 0.0, r2 = 0.0;
    };
};

} // namespace aria

#endif // ARIA_DITHER_H
