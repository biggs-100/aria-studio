#ifndef ARIA_PHASE_METER_H
#define ARIA_PHASE_METER_H

#include <cstdint>

namespace aria::metering {

/// Stereo phase correlation meter.
///
/// Computes the correlation coefficient between left and right channels
/// over a sliding window. The result ranges from -1.0 (completely out
/// of phase) to +1.0 (perfectly mono-compatible). Values near 0 indicate
/// uncorrelated stereo material.
///
/// Thread safety: designed for single-threaded use.
class PhaseMeter {
public:
    PhaseMeter();

    /// Process a block of stereo samples.
    /// @param left    Left channel buffer (non-interleaved).
    /// @param right   Right channel buffer (non-interleaved).
    /// @param frames  Number of frames.
    void process(const float* left, const float* right, uint32_t frames);

    /// Current correlation value (-1.0 to 1.0).
    float correlation() const { return correlation_; }

    /// Reset internal state.
    void reset();

private:
    static constexpr uint32_t kWindowFrames = 2048;

    float left_ring_[kWindowFrames]{};
    float right_ring_[kWindowFrames]{};
    uint32_t ring_pos_ = 0;
    uint32_t ring_filled_ = 0;

    double sum_l_ = 0.0;
    double sum_r_ = 0.0;
    double sum_lr_ = 0.0;
    double sum_l2_ = 0.0;
    double sum_r2_ = 0.0;

    float correlation_ = 1.0f;
};

} // namespace aria::metering

#endif // ARIA_PHASE_METER_H
