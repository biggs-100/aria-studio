#ifndef ARIA_AUDIO_WSOLA_H
#define ARIA_AUDIO_WSOLA_H

#include <cstdint>
#include <vector>

namespace aria {

// ─── TimeStretchAlgorithm (abstract base) ─────────────────────

/// Abstract interface for time-stretching algorithms.
/// Enables polymorphic swap-in of Rubber Band, Elastique, etc.
///
/// Uses a feed-model: the caller provides input samples and receives
/// stretched output. The return value indicates how many output
/// frames were produced (may differ from input_frames due to
/// time compression/expansion).
class TimeStretchAlgorithm {
public:
    virtual ~TimeStretchAlgorithm() = default;

    /// Set the stretch ratio (0.5 = half speed, 2.0 = double speed).
    virtual void set_ratio(double ratio) = 0;

    /// Process input audio and produce stretched output.
    ///
    /// @param input           Input samples, planar: ch0[0..N-1], ch1[0..N-1], …
    /// @param output          Output buffer, same channel layout.
    /// @param input_frames    Number of input frames (per channel).
    /// @param max_output_frames  Capacity of output buffer (per channel).
    /// @return Number of output frames actually written (per channel).
    ///         0 means not enough internal state yet (initial latency).
    virtual uint32_t process(const float* input, float* output,
                              uint32_t input_frames,
                              uint32_t max_output_frames) = 0;

    /// Reset internal state (flush buffers, clear overlap).
    virtual void reset() = 0;

    /// Latency introduced by the algorithm in samples (per channel).
    virtual uint32_t latency() const = 0;
};

// ─── WSOLAStretch ─────────────────────────────────────────────

/// Waveform Similarity Overlap-Add time stretcher.
///
/// Implements WSOLA for real-time variable-speed playback.
/// - Hann window with 75% overlap for smooth cross-fades.
/// - Similarity search via cross-correlation in the overlap region.
/// - All buffers pre-allocated — zero allocations after construction.
/// - Ratio clamped to [0.5, 2.0].
class WSOLAStretch : public TimeStretchAlgorithm {
public:
    /// @param sample_rate  Sample rate in Hz.
    /// @param channels     Number of channels.
    /// @param window_ms    Analysis window duration in ms (default 60).
    /// @param overlap      Synthesis overlap fraction (default 0.75).
    WSOLAStretch(double sample_rate, uint32_t channels,
                 double window_ms = 60.0, double overlap = 0.75);
    ~WSOLAStretch() override = default;

    WSOLAStretch(const WSOLAStretch&) = delete;
    WSOLAStretch& operator=(const WSOLAStretch&) = delete;
    WSOLAStretch(WSOLAStretch&&) = default;
    WSOLAStretch& operator=(WSOLAStretch&&) = default;

    void set_ratio(double ratio) override;
    uint32_t process(const float* input, float* output,
                      uint32_t input_frames,
                      uint32_t max_output_frames) override;
    void reset() override;
    uint32_t latency() const override;

    double ratio() const { return ratio_; }

private:
    void build_window();
    uint32_t find_best_overlap(const float* analysis,
                                const float* overlap,
                                uint32_t len) const;

    double sample_rate_;
    uint32_t channels_;
    double ratio_ = 1.0;
    double window_ms_;
    double overlap_;

    uint32_t window_size_;
    uint32_t synthesis_hop_;
    double analysis_hop_;

    std::vector<float> window_;

    // Input ring buffer (per-channel planar)
    std::vector<float> input_buf_;
    uint32_t input_capacity_ = 0;

    // Output ring + weight accumulator
    std::vector<float> output_buf_;
    std::vector<float> weight_buf_;
    uint32_t output_capacity_ = 0;

    // Monotonically increasing frame counters
    uint64_t input_frames_written_ = 0;
    double   input_analysis_pos_ = 0.0;
    uint64_t output_frames_written_ = 0;
    uint64_t output_frames_read_ = 0;

    uint32_t latency_samples_ = 0;
    bool first_frame_ = true;
};

/// The process() function writes to output at the current read position.
/// The read position advances by input_frames each call, regardless of
/// how many output frames were actually produced. This means the caller
/// may receive a mix of stretched data and silence, depending on the
/// ratio and internal state.
///
/// For continuous streaming, call process() repeatedly with block-sized
/// input. The algorithm maintains internal buffers and produces/gates
/// output as it becomes available.

} // namespace aria

#endif // ARIA_AUDIO_WSOLA_H
