#ifndef ARIA_RMS_METER_H
#define ARIA_RMS_METER_H

#include <cstdint>
#include <cmath>

namespace aria::metering {

/// RMS (Root Mean Square) level meter with configurable window.
///
/// Uses a sliding window to compute the RMS energy of the incoming
/// audio signal. The window size is configurable in milliseconds.
///
/// Thread safety: process() should be called from the audio thread;
/// current_rms() can be read from any thread.
class RMSMeter {
public:
    RMSMeter();

    /// Process a block of samples.
    /// @param samples  Sample buffer (non-interleaved, single channel).
    /// @param frames   Number of frames.
    /// @param channel  Channel index (0-based) — unused in mono impl.
    void process(const float* samples, uint32_t frames, uint32_t channel);

    /// Current RMS level (linear amplitude).
    float current_rms() const { return current_rms_; }

    /// Set the analysis window in milliseconds.
    /// Default: 300 ms (standard for audio metering).
    /// Must be called before processing.
    void set_window_ms(double ms);

    /// Reset internal state.
    void reset();

private:
    static constexpr uint32_t kMaxChannels = 32;
    static constexpr uint32_t kMaxWindowFrames = 48000; // 1s @ 48 kHz

    double  window_ms_ = 300.0;
    uint32_t window_frames_ = 0;
    float   current_rms_ = 0.0f;

    // Per-channel ring buffer for sliding window
    float   ring_buf_[kMaxChannels][kMaxWindowFrames]{};
    double  sum_sq_[kMaxChannels]{};
    uint32_t ring_pos_[kMaxChannels]{};
    uint32_t ring_filled_[kMaxChannels]{};
};

} // namespace aria::metering

#endif // ARIA_RMS_METER_H
