#include "model/clip.h"
#include <cmath>

namespace aria {

// ═════════════════════════════════════════════════════════════════
// clip_time_at
// ═════════════════════════════════════════════════════════════════

uint64_t Clip::clip_time_at(uint64_t playback_ppqn) const {
    if (playback_ppqn < start_ppqn_) return 0;

    uint64_t offset = playback_ppqn - start_ppqn_;

    if (looping_ && loop_end_ppqn_ > loop_start_ppqn_) {
        uint64_t loop_len = loop_end_ppqn_ - loop_start_ppqn_;
        if (offset >= loop_end_ppqn_) {
            offset = loop_start_ppqn_ + ((offset - loop_start_ppqn_) % loop_len);
        }
    }

    return offset;
}

// ═════════════════════════════════════════════════════════════════
// evaluate_fade
// ═════════════════════════════════════════════════════════════════

double Clip::evaluate_fade(FadeShape shape, double t) {
    // Clamp t to [0, 1]
    if (t <= 0.0) t = 0.0;
    if (t >= 1.0) t = 1.0;

    switch (shape) {
    case FadeShape::None:
        return 1.0;

    case FadeShape::LinearIn:
        return t;

    case FadeShape::LinearOut:
        return 1.0 - t;

    case FadeShape::EqualPowerIn:
        return std::sqrt(t);

    case FadeShape::EqualPowerOut:
        return std::sqrt(1.0 - t);

    case FadeShape::ExponentialIn:
        return t * t;

    case FadeShape::ExponentialOut: {
        double u = 1.0 - t;
        return u * u;
    }

    default:
        return 1.0;
    }
}

} // namespace aria
