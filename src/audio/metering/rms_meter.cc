#include "rms_meter.h"

#include <algorithm>
#include <cstring>

namespace aria::metering {

RMSMeter::RMSMeter() {
    std::memset(ring_buf_, 0, sizeof(ring_buf_));
    std::memset(sum_sq_, 0, sizeof(sum_sq_));
    std::memset(ring_pos_, 0, sizeof(ring_pos_));
    std::memset(ring_filled_, 0, sizeof(ring_filled_));
    // Default window: 300ms @ 48 kHz
    window_frames_ = static_cast<uint32_t>(window_ms_ * 48.0);
}

void RMSMeter::set_window_ms(double ms) {
    window_ms_ = ms;
    // We assume 48 kHz for pre-calculation, actual rate is adaptive.
    // The process() function should ideally take sample rate as a parameter.
    window_frames_ = static_cast<uint32_t>(ms * 48.0);
    if (window_frames_ > kMaxWindowFrames) {
        window_frames_ = kMaxWindowFrames;
    }
    reset();
}

void RMSMeter::reset() {
    std::memset(ring_buf_, 0, sizeof(ring_buf_));
    std::memset(sum_sq_, 0, sizeof(sum_sq_));
    std::memset(ring_pos_, 0, sizeof(ring_pos_));
    std::memset(ring_filled_, 0, sizeof(ring_filled_));
    current_rms_ = 0.0f;
}

void RMSMeter::process(const float* samples, uint32_t frames, uint32_t channel) {
    if (!samples || frames == 0 || channel >= kMaxChannels) return;

    auto& sum = sum_sq_[channel];
    auto& pos = ring_pos_[channel];
    auto& filled = ring_filled_[channel];
    auto* ring = ring_buf_[channel];

    for (uint32_t i = 0; i < frames; ++i) {
        // Remove old value from sum
        sum -= static_cast<double>(ring[pos]) * static_cast<double>(ring[pos]);

        // Add new value
        float val = samples[i];
        ring[pos] = val;
        sum += static_cast<double>(val) * static_cast<double>(val);

        // Advance position
        pos = (pos + 1) % window_frames_;
        if (filled < window_frames_) ++filled;
    }

    // Compute RMS from window
    if (filled > 0) {
        current_rms_ = static_cast<float>(std::sqrt(sum / filled));
    }
}

} // namespace aria::metering
