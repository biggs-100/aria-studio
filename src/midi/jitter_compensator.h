#ifndef ARIA_JITTER_COMPENSATOR_H
#define ARIA_JITTER_COMPENSATOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace aria {

/// MIDI jitter compensator.
///
/// Measures and compensates for input timing jitter and device latency.
/// Uses a calibration phase to estimate the device's average latency and
/// jitter stddev, then applies a correction to incoming timestamps.
///
/// The compensator maintains a running average and standard deviation of
/// timestamp deltas during calibration, then subtracts the estimated
/// latency and clamps correction to within 3 standard deviations.
class JitterCompensator {
public:
    JitterCompensator() = default;

    /// Calibrate the compensator for a specific device.
    /// Collects timing samples to estimate latency and jitter.
    void calibrate(const std::string& device_id);

    /// Compensate a raw timestamp (microseconds) and return the corrected value.
    uint64_t compensate(uint64_t raw_timestamp_us) const;

    /// Manually set the estimated device latency in microseconds.
    void set_device_latency_us(uint32_t us) { device_latency_us_ = us; }

    /// Get the current device latency estimate.
    uint32_t device_latency_us() const { return device_latency_us_; }

    /// Get the estimated jitter standard deviation in microseconds.
    uint32_t jitter_stddev_us() const { return jitter_stddev_us_; }

    /// Get the number of calibration samples collected.
    size_t calibration_samples() const { return calibration_samples_.size(); }

    /// Reset calibration data and compensation state.
    void reset();

    /// Whether the compensator has been calibrated.
    bool is_calibrated() const { return calibrated_; }

private:
    bool calibrated_ = false;
    uint32_t device_latency_us_ = 0;
    uint32_t jitter_stddev_us_ = 0;
    uint64_t avg_delta_us_ = 0;

    // Calibration samples (timestamps in microseconds)
    std::vector<uint64_t> calibration_samples_;

    static constexpr size_t kMinCalibrationSamples = 10;
    static constexpr size_t kMaxCalibrationSamples = 1000;
};

} // namespace aria

#endif // ARIA_JITTER_COMPENSATOR_H
