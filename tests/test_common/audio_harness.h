#ifndef ARIA_AUDIO_HARNESS_H
#define ARIA_AUDIO_HARNESS_H

#include <cstdint>
#include <vector>
#include <cmath>

namespace aria::test {

/// Audio test utilities — signal generation and analysis.
class AudioHarness {
public:
    /// Generate a sine wave buffer.
    static std::vector<float> generate_sine(
        double frequency, double sample_rate,
        uint32_t frames, double amplitude = 0.5);

    /// Generate an impulse at the given position.
    static std::vector<float> generate_impulse(
        uint32_t frames, uint32_t position = 0);

    /// Generate white noise.
    static std::vector<float> generate_noise(
        uint32_t frames, double amplitude = 0.5);

    /// Calculate RMS level.
    static double calculate_rms(const float* buffer, uint32_t frames);

    /// Calculate peak level.
    static float calculate_peak(const float* buffer, uint32_t frames);
};

} // namespace aria::test

#endif // ARIA_AUDIO_HARNESS_H
