#ifndef ARIA_PEAK_METER_H
#define ARIA_PEAK_METER_H

#include <cstdint>

namespace aria::metering {

/// Peak level meter with decay and hold.
///
/// Tracks the maximum absolute sample value per channel over time,
/// with a configurable decay rate for visual fall-back and a hold
/// peak that remembers the maximum value until reset.
///
/// Thread safety: all methods are intended for single-threaded use
/// on the audio or analysis thread. process() should be called from
/// the audio thread; readouts can be taken from the control thread
/// at any time (atomic reads of float values).
class PeakMeter {
public:
    PeakMeter();

    /// Process a block of samples for one channel.
    /// @param samples  Sample buffer (non-interleaved).
    /// @param frames   Number of frames.
    /// @param channel  Channel index (0-based).
    void process(const float* samples, uint32_t frames, uint32_t channel);

    /// Current instantaneous peak level (linear).
    float current_peak() const { return current_peak_; }

    /// Maximum peak since last reset (linear).
    float hold_peak() const { return hold_peak_; }

    /// Reset the hold peak to current peak.
    void reset_hold();

    /// Set the decay rate in dB per second.
    /// Default: 40 dB/s (standard VU-style fall-back).
    void set_decay_rate(double db_per_second);

private:
    static constexpr uint32_t kMaxChannels = 32;

    float current_peak_ = 0.0f;
    float hold_peak_    = 0.0f;
    float decay_factor_ = 0.995f; // ~40 dB/s @ 48 kHz
    float peak_per_ch_[kMaxChannels]{};
};

} // namespace aria::metering

#endif // ARIA_PEAK_METER_H
