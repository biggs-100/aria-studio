#include "phase_meter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aria::metering {

PhaseMeter::PhaseMeter() {
    reset();
}

void PhaseMeter::reset() {
    std::memset(left_ring_, 0, sizeof(left_ring_));
    std::memset(right_ring_, 0, sizeof(right_ring_));
    ring_pos_ = 0;
    ring_filled_ = 0;
    sum_l_ = sum_r_ = sum_lr_ = sum_l2_ = sum_r2_ = 0.0;
    correlation_ = 1.0f;
}

void PhaseMeter::process(const float* left, const float* right,
                          uint32_t frames)
{
    if (!left || !right || frames == 0) return;

    for (uint32_t i = 0; i < frames; ++i) {
        double l_old = static_cast<double>(left_ring_[ring_pos_]);
        double r_old = static_cast<double>(right_ring_[ring_pos_]);

        double l_new = static_cast<double>(left[i]);
        double r_new = static_cast<double>(right[i]);

        // Update ring buffer
        left_ring_[ring_pos_] = left[i];
        right_ring_[ring_pos_] = right[i];
        ring_pos_ = (ring_pos_ + 1) % kWindowFrames;
        if (ring_filled_ < kWindowFrames) ++ring_filled_;

        if (ring_filled_ > 1) {
            // Running sums: subtract old, add new
            sum_l_  += l_new - l_old;
            sum_r_  += r_new - r_old;
            sum_lr_ += l_new * r_new - l_old * r_old;
            sum_l2_ += l_new * l_new - l_old * l_old;
            sum_r2_ += r_new * r_new - r_old * r_old;
        } else {
            sum_l_  += l_new;
            sum_r_  += r_new;
            sum_lr_ += l_new * r_new;
            sum_l2_ += l_new * l_new;
            sum_r2_ += r_new * r_new;
        }
    }

    // Compute correlation coefficient
    if (ring_filled_ > 0) {
        double n = static_cast<double>(ring_filled_);
        double cov = sum_lr_ - (sum_l_ * sum_r_) / n;
        double var_l = sum_l2_ - (sum_l_ * sum_l_) / n;
        double var_r = sum_r2_ - (sum_r_ * sum_r_) / n;

        double denom = std::sqrt(var_l * var_r);
        if (denom > 0.0) {
            correlation_ = static_cast<float>(std::clamp(cov / denom, -1.0, 1.0));
        }
    }
}

} // namespace aria::metering
