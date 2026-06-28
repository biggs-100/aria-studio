#include "jitter_compensator.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace aria {

void JitterCompensator::calibrate(const std::string& /*device_id*/) {
    // Reset previous calibration
    calibration_samples_.clear();
    calibrated_ = false;
    device_latency_us_ = 0;
    jitter_stddev_us_ = 0;
    avg_delta_us_ = 0;
}

uint64_t JitterCompensator::compensate(uint64_t raw_timestamp_us) const {
    if (!calibrated_) {
        return raw_timestamp_us; // Pass through during calibration
    }

    // Subtract estimated device latency
    int64_t corrected = static_cast<int64_t>(raw_timestamp_us) -
                        static_cast<int64_t>(device_latency_us_);

    // Clamp to non-negative
    if (corrected < 0) corrected = 0;

    return static_cast<uint64_t>(corrected);
}

void JitterCompensator::reset() {
    calibration_samples_.clear();
    calibrated_ = false;
    device_latency_us_ = 0;
    jitter_stddev_us_ = 0;
    avg_delta_us_ = 0;
}

} // namespace aria
