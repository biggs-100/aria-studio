#include "model/audio_clip.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace aria {

// ═════════════════════════════════════════════════════════════════
// WaveformCache
// ═════════════════════════════════════════════════════════════════

const WaveformLOD& WaveformCache::get_lod(uint32_t samples_per_point,
                                           uint64_t total_samples) {
    // Check if already cached
    for (const auto& lod : lods_) {
        // For now, simple linear scan — will optimise if needed
        if (!lod.peaks.empty() &&
            lod.peaks.size() == (total_samples / samples_per_point)) {
            return lod;
        }
    }

    // Generate new LOD
    WaveformLOD lod;
    if (samples_per_point == 0 || total_samples == 0) {
        lods_.push_back(std::move(lod));
        return lods_.back();
    }

    size_t num_points = static_cast<size_t>(total_samples / samples_per_point);
    if (num_points == 0) num_points = 1;

    lod.peaks.resize(num_points);
    lod.valleys.resize(num_points);

    // In the real DAW, this reads actual sample data to compute peaks/valleys.
    // For now, generate synthetic data so the cache returns bounded values.
    for (size_t i = 0; i < num_points; ++i) {
        double phase = static_cast<double>(i) / static_cast<double>(num_points);
        // Simple sine-based synthetic waveform for testing
        lod.peaks[i]   = static_cast<float>(0.5 + 0.5 * std::sin(phase * 6.2832));
        lod.valleys[i] = static_cast<float>(0.5 + 0.5 * std::sin(phase * 6.2832 + 3.1416));
    }

    lods_.push_back(std::move(lod));
    return lods_.back();
}

// ═════════════════════════════════════════════════════════════════
// AudioClip
// ═════════════════════════════════════════════════════════════════

uint64_t AudioClip::playback_samples() const {
    if (sample_offset_ >= total_samples_) return 0;
    return total_samples_ - sample_offset_;
}

const WaveformLOD& AudioClip::get_waveform(uint32_t samples_per_point) {
    return waveform_cache_.get_lod(samples_per_point, total_samples_);
}

double AudioClip::gain_at_ppqn(uint64_t ppqn) const {
    if (gain_envelope_.empty()) return 1.0;

    // Before first point
    if (ppqn <= gain_envelope_.front().ppqn) {
        return gain_envelope_.front().gain;
    }

    // After last point
    if (ppqn >= gain_envelope_.back().ppqn) {
        return gain_envelope_.back().gain;
    }

    // Binary search for segment
    auto it = std::upper_bound(
        gain_envelope_.begin(), gain_envelope_.end(), ppqn,
        [](uint64_t q, const GainPoint& pt) { return q < pt.ppqn; });

    const auto& left  = *(it - 1);
    const auto& right = *it;

    uint64_t seg_len = right.ppqn - left.ppqn;
    if (seg_len == 0) return left.gain;

    double t = static_cast<double>(ppqn - left.ppqn)
             / static_cast<double>(seg_len);
    return left.gain + t * (right.gain - left.gain);
}

} // namespace aria
