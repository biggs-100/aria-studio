#include "peak_meter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aria::metering {

PeakMeter::PeakMeter() {
    std::memset(peak_per_ch_, 0, sizeof(peak_per_ch_));
}

void PeakMeter::process(const float* samples, uint32_t frames, uint32_t channel) {
    if (!samples || frames == 0) return;
    if (channel >= kMaxChannels) return;

    // Find peak in this block
    float block_peak = 0.0f;
    for (uint32_t i = 0; i < frames; ++i) {
        float abs_val = std::abs(samples[i]);
        if (abs_val > block_peak) block_peak = abs_val;
    }

    // Per-channel peak (instantaneous)
    peak_per_ch_[channel] = std::max(peak_per_ch_[channel] * decay_factor_,
                                     block_peak);

    // Find global peak across all channels
    float global_peak = 0.0f;
    for (uint32_t c = 0; c < kMaxChannels; ++c) {
        if (peak_per_ch_[c] > global_peak) global_peak = peak_per_ch_[c];
    }
    current_peak_ = global_peak;

    // Update hold peak
    if (global_peak > hold_peak_) hold_peak_ = global_peak;
}

void PeakMeter::reset_hold() {
    hold_peak_ = current_peak_;
}

void PeakMeter::set_decay_rate(double db_per_second) {
    // Convert dB/s to per-sample factor (assuming 48 kHz reference)
    // factor = 10^(dB_per_sample / 20)
    // dB_per_sample = dB_per_second / sample_rate
    // For 48 kHz reference: decay_factor = 10^(-db_per_second / (20 * 48000))
    constexpr double kRefSampleRate = 48000.0;
    double db_per_sample = db_per_second / kRefSampleRate;
    decay_factor_ = static_cast<float>(std::pow(10.0, -db_per_sample / 20.0));
}

} // namespace aria::metering
