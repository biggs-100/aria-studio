#ifndef ARIA_LUFS_METER_H
#define ARIA_LUFS_METER_H

#include <cstdint>
#include <cmath>

namespace aria::metering {

/// EBU R128 loudness meter (LUFS).
///
/// Implements loudness measurement per the EBU R128 / ITU-R BS.1770-4
/// standard, including K-weighting pre-filter, momentary (400ms),
/// short-term (3s), and integrated (gated) loudness measurements,
/// loudness range (LRA), and true peak estimation.
///
/// Thread safety: process() is intended for single-threaded use.
class LUFSMeter {
public:
    LUFSMeter();

    /// Process a block of interleaved samples.
    /// @param samples   Interleaved sample buffer.
    /// @param frames    Number of frames (per channel).
    /// @param channels  Number of channels.
    void process(const float* samples, uint32_t frames, uint32_t channels);

    /// Loudness measurement results.
    struct LUFS {
        float momentary   = -70.0f;  ///< M: 400 ms window
        float short_term  = -70.0f;  ///< S: 3 s window
        float integrated  = -70.0f;  ///< I: gated integrated loudness
        float range       = 0.0f;    ///< LRA: loudness range
        float true_peak   = -70.0f;  ///< True peak max (TP)
    };

    /// Read current loudness values.
    LUFS read() const;

    /// Reset all internal state.
    void reset();

private:
    static constexpr uint32_t kMaxChannels = 8;
    static constexpr uint32_t kMaxWindowFrames = 192000; // 4s @ 48 kHz

    /// K-weighting filter state (2nd-order IIR per channel).
    struct KFilterState {
        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;
    };

    /// Apply K-weighting pre-filter per BS.1770-4.
    /// Two-stage: first a high-pass (pre-filter), then a shelving filter.
    float apply_k_filter(float sample, uint32_t channel);

    // K-weighting filter states per channel
    KFilterState pre_filter_[kMaxChannels];   // Stage 1: pre-filter (HP)
    KFilterState shelv_filter_[kMaxChannels]; // Stage 2: shelving filter

    // Ring buffers for sliding windows
    double momentary_ring_[kMaxChannels][kMaxWindowFrames]{};
    uint32_t momentary_pos_[kMaxChannels]{};
    uint32_t momentary_filled_[kMaxChannels]{};

    double short_term_ring_[kMaxChannels][kMaxWindowFrames]{};
    uint32_t short_term_pos_[kMaxChannels]{};
    uint32_t short_term_filled_[kMaxChannels]{};

    // Integrated loudness gating state
    double integrated_energy_ = 0.0;
    uint64_t integrated_frames_ = 0;
    bool gate_relative_ = false;
    double gate_threshold_ = -70.0;

    // Loudness range state
    double lra_block_energy_[1000]{}; // histogram for LRA
    uint32_t lra_block_count_ = 0;

    // True peak (4x oversampled with interpolation)
    double true_peak_max_ = 0.0;

    // Current outputs
    double momentary_loudness_ = -70.0;
    double short_term_loudness_ = -70.0;
    double integrated_loudness_ = -70.0;
    double lra_ = 0.0;

    uint32_t sample_rate_ = 48000;

    /// Compute loudness from weighted channel energies.
    double compute_loudness(const double* channel_energy, uint32_t channels) const;

    /// Update true peak using 4x oversampling + cubic interpolation.
    void update_true_peak(const float* samples, uint32_t frames, uint32_t channels);
};

} // namespace aria::metering

#endif // ARIA_LUFS_METER_H
