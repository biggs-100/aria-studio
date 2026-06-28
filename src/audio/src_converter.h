#ifndef ARIA_SRC_CONVERTER_H
#define ARIA_SRC_CONVERTER_H

#include <cstdint>
#include <vector>

namespace aria {

/// Sample Rate Converter with multiple quality levels.
///
/// Converts between arbitrary input and output sample rates using
/// interpolation methods of increasing quality:
///   - Fast:    Linear interpolation (2 points)
///   - Medium:  Cubic interpolation (4 points)
///   - High:    Windowed sinc interpolation (8 points)
///   - Best:    Windowed sinc interpolation (64 points, table-driven)
///
/// High quality uses SSE2 acceleration when available.
///
/// Thread safety: process() is NOT thread-safe. Use separate converters
/// per thread or add external synchronization.
class SRCConverter {
public:
    enum class Quality {
        Fast,   ///< Linear interpolation (cheapest)
        Medium, ///< Cubic Hermite interpolation
        High,   ///< 8-point sinc windowed (good quality, SSE2)
        Best    ///< 64-point sinc windowed (highest quality)
    };

    SRCConverter();

    /// Set the conversion quality.
    void set_quality(Quality q) { quality_ = q; update_sinc_table(); }

    /// Set input sample rate.
    void set_input_rate(uint32_t input_sr) { input_rate_ = input_sr; }

    /// Set output sample rate.
    void set_output_rate(uint32_t output_sr) { output_rate_ = output_sr; }

    /// Process a block of input data.
    /// @param input            Input buffer (single channel).
    /// @param input_frames     Number of input frames.
    /// @param output           Output buffer (pre-allocated).
    /// @param max_output_frames Maximum number of output frames that fit in output.
    /// @return Actual number of output frames produced.
    uint32_t process(const float* input, uint32_t input_frames,
                     float* output, uint32_t max_output_frames);

    /// Reset internal state (flushes fractional position).
    void reset();

    /// Estimate the maximum output frames for a given input count.
    uint32_t max_output_frames(uint32_t input_frames) const;

private:
    /// Update the sinc lookup table for the current quality level.
    void update_sinc_table();

    /// Linear interpolation (Fast).
    uint32_t process_linear(const float* input, uint32_t input_frames,
                            float* output, uint32_t max_output_frames);

    /// Cubic Hermite interpolation (Medium).
    uint32_t process_cubic(const float* input, uint32_t input_frames,
                           float* output, uint32_t max_output_frames);

    /// Windowed sinc interpolation (High: 8-point, Best: 64-point).
    uint32_t process_sinc(const float* input, uint32_t input_frames,
                          float* output, uint32_t max_output_frames);

    Quality  quality_ = Quality::Medium;
    uint32_t input_rate_ = 44100;
    uint32_t output_rate_ = 48000;

    // Ratio: output_rate / input_rate
    double ratio_ = 1.0;

    // Fractional position accumulator (for sample rate conversion)
    double frac_pos_ = 0.0;

    // Sinc table for windowed-sinc interpolation
    // Pre-computed sinc coefficients per zero-crossing
    static constexpr uint32_t kSincPointsBest = 64;
    static constexpr uint32_t kSincPointsHigh = 8;
    static constexpr uint32_t kSincTableSize = 1024; // sub-sample resolution

    // Sinc table: [phase][point] → coefficient
    float sinc_table_best_[kSincTableSize][kSincPointsBest]{};
    float sinc_table_high_[kSincTableSize][kSincPointsHigh]{};
    bool sinc_table_initialized_ = false;

    // Best-quality uses 64-point sinc; 2^16 phase resolution
    uint32_t sinc_phase_bits_ = 10; // 1024 phases
};

} // namespace aria

#endif // ARIA_SRC_CONVERTER_H
