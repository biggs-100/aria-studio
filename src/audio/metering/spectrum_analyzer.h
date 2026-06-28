#ifndef ARIA_SPECTRUM_ANALYZER_H
#define ARIA_SPECTRUM_ANALYZER_H

#include <cstdint>
#include <cmath>

namespace aria::metering {

/// FFT-based spectrum analyzer for real-time frequency analysis.
///
/// Uses a configurable FFT size and windowing function to compute
/// the magnitude spectrum of incoming audio. Designed for real-time
/// use with overlapping Hann windows.
///
/// Thread safety: process() and read() should not be called concurrently
/// from different threads. Use external synchronization if needed.
class SpectrumAnalyzer {
public:
    /// Window type for FFT pre-processing.
    enum class WindowType {
        Rectangular,
        Hanning,      ///< Default
        Hamming,
        Blackman,
        BlackmanHarris
    };

    static constexpr uint32_t kMaxFFTSize = 16384;
    static constexpr uint32_t kMaxBins    = kMaxFFTSize / 2;

    SpectrumAnalyzer();

    /// Process a block of samples.
    /// @param samples  Single-channel sample buffer.
    /// @param frames   Number of frames.
    void process(const float* samples, uint32_t frames);

    /// Set the FFT size (power of two, 512–16384).
    void set_fft_size(uint32_t size);

    /// Set the window function type.
    void set_window_type(WindowType type);

    /// Current spectrum data.
    struct Spectrum {
        float bins[kMaxBins]{};       ///< Magnitude spectrum (linear).
        uint32_t bin_count = 0;       ///< Number of valid bins (fft_size/2).
        float dominant_freq = 0.0f;   ///< Frequency with highest magnitude.
        float max_magnitude = 0.0f;   ///< Maximum magnitude value.
    };

    /// Read the current spectrum.
    Spectrum read() const;

    /// Reset internal state.
    void reset();

private:
    // FFT implementation (radix-2 Cooley-Tukey, in-place)
    void fft(float* real, float* imag, uint32_t n);

    // Bit-reversal permutation
    uint32_t bit_reverse(uint32_t x, uint32_t log2_n) const;

    // Update window coefficients for current FFT size
    void update_window();

    uint32_t fft_size_ = 2048;
    uint32_t log2_fft_ = 11;
    WindowType window_type_ = WindowType::Hanning;

    // Window coefficients
    float window_[kMaxFFTSize]{};

    // Input ring buffer for overlapping
    float ring_buf_[kMaxFFTSize]{};
    uint32_t ring_pos_ = 0;
    uint32_t ring_filled_ = 0;

    // FFT work buffers
    float real_[kMaxFFTSize]{};
    float imag_[kMaxFFTSize]{};

    // Current spectrum
    Spectrum current_{};
};

} // namespace aria::metering

#endif // ARIA_SPECTRUM_ANALYZER_H
