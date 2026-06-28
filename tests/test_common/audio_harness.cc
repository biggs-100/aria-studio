#define _USE_MATH_DEFINES
#include "audio_harness.h"

#include <algorithm>
#include <numeric>
#include <random>

namespace aria::test {

std::vector<float> AudioHarness::generate_sine(
    double frequency, double sample_rate,
    uint32_t frames, double amplitude)
{
    std::vector<float> buffer(frames);
    for (uint32_t i = 0; i < frames; ++i) {
        buffer[i] = static_cast<float>(
            amplitude * std::sin(2.0 * M_PI * frequency * i / sample_rate));
    }
    return buffer;
}

std::vector<float> AudioHarness::generate_impulse(
    uint32_t frames, uint32_t position)
{
    std::vector<float> buffer(frames, 0.0f);
    if (position < frames) {
        buffer[position] = 1.0f;
    }
    return buffer;
}

std::vector<float> AudioHarness::generate_noise(
    uint32_t frames, double amplitude)
{
    std::vector<float> buffer(frames);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(
        static_cast<float>(-amplitude), static_cast<float>(amplitude));
    for (auto& sample : buffer) {
        sample = dist(gen);
    }
    return buffer;
}

double AudioHarness::calculate_rms(const float* buffer, uint32_t frames) {
    double sum = 0.0;
    for (uint32_t i = 0; i < frames; ++i) {
        sum += buffer[i] * buffer[i];
    }
    return std::sqrt(sum / frames);
}

float AudioHarness::calculate_peak(const float* buffer, uint32_t frames) {
    float peak = 0.0f;
    for (uint32_t i = 0; i < frames; ++i) {
        peak = std::max(peak, std::abs(buffer[i]));
    }
    return peak;
}

} // namespace aria::test
