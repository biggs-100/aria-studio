#include "spectrum_analyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aria::metering {

// ═══════════════════════════════════════════════════════════════════

SpectrumAnalyzer::SpectrumAnalyzer() {
    update_window();
}

void SpectrumAnalyzer::reset() {
    std::memset(ring_buf_, 0, sizeof(ring_buf_));
    ring_pos_ = 0;
    ring_filled_ = 0;
    std::memset(&current_, 0, sizeof(current_));
}

void SpectrumAnalyzer::set_fft_size(uint32_t size) {
    if (size < 512) size = 512;
    if (size > kMaxFFTSize) size = kMaxFFTSize;

    // Round to nearest power of two
    uint32_t p2 = 1;
    while (p2 < size) p2 <<= 1;
    if (p2 > kMaxFFTSize) p2 = kMaxFFTSize;

    fft_size_ = p2;
    log2_fft_ = 0;
    uint32_t t = p2;
    while (t > 1) { t >>= 1; ++log2_fft_; }

    update_window();
    reset();
}

void SpectrumAnalyzer::set_window_type(WindowType type) {
    window_type_ = type;
    update_window();
}

// ─── Window function computation ────────────────────────────────

void SpectrumAnalyzer::update_window() {
    const uint32_t n = fft_size_;
    const double two_pi = 2.0 * 3.14159265358979323846;

    for (uint32_t i = 0; i < n; ++i) {
        double w = 0.0;
        double a0, a1, a2, a3, a4;

        switch (window_type_) {
        case WindowType::Rectangular:
            w = 1.0;
            break;

        case WindowType::Hanning:
            w = 0.5 * (1.0 - std::cos(two_pi * i / (n - 1)));
            break;

        case WindowType::Hamming:
            w = 0.53836 - 0.46164 * std::cos(two_pi * i / (n - 1));
            break;

        case WindowType::Blackman:
            a0 = 0.42659071;
            a1 = 0.49656062;
            a2 = 0.07684867;
            w = a0 - a1 * std::cos(two_pi * i / (n - 1))
                  + a2 * std::cos(4.0 * two_pi * i / (n - 1));
            break;

        case WindowType::BlackmanHarris:
            a0 = 0.35875;
            a1 = 0.48829;
            a2 = 0.14128;
            a3 = 0.01168;
            w = a0 - a1 * std::cos(two_pi * i / (n - 1))
                  + a2 * std::cos(4.0 * two_pi * i / (n - 1))
                  - a3 * std::cos(6.0 * two_pi * i / (n - 1));
            break;
        }

        window_[i] = static_cast<float>(w);
    }
}

// ─── Bit reversal (for FFT) ─────────────────────────────────────

uint32_t SpectrumAnalyzer::bit_reverse(uint32_t x, uint32_t log2_n) const {
    uint32_t result = 0;
    for (uint32_t i = 0; i < log2_n; ++i) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

// ─── Radix-2 Cooley-Tukey FFT (in-place) ────────────────────────

void SpectrumAnalyzer::fft(float* real, float* imag, uint32_t n) {
    // Bit-reversal permutation
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t j = bit_reverse(i, log2_fft_);
        if (j > i) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }

    // Cooley-Tukey radix-2 DIT
    for (uint32_t len = 2; len <= n; len <<= 1) {
        uint32_t half_len = len >> 1;
        double angle = -2.0 * 3.14159265358979323846 / static_cast<double>(len);
        float w_real = static_cast<float>(std::cos(angle));
        float w_imag = static_cast<float>(std::sin(angle));

        for (uint32_t i = 0; i < n; i += len) {
            float wr = 1.0f, wi = 0.0f;
            for (uint32_t j = 0; j < half_len; ++j) {
                // Butterfly
                float t_real = wr * real[i + j + half_len] - wi * imag[i + j + half_len];
                float t_imag = wr * imag[i + j + half_len] + wi * real[i + j + half_len];

                real[i + j + half_len] = real[i + j] - t_real;
                imag[i + j + half_len] = imag[i + j] - t_imag;
                real[i + j] += t_real;
                imag[i + j] += t_imag;

                // Rotate twiddle
                float tmp = wr * w_real - wi * w_imag;
                wi = wr * w_imag + wi * w_real;
                wr = tmp;
            }
        }
    }
}

// ─── Process ────────────────────────────────────────────────────

void SpectrumAnalyzer::process(const float* samples, uint32_t frames) {
    if (!samples || frames == 0) return;

    // Accumulate samples into ring buffer
    for (uint32_t i = 0; i < frames; ++i) {
        ring_buf_[ring_pos_] = samples[i];
        ring_pos_ = (ring_pos_ + 1) % fft_size_;
        if (ring_filled_ < fft_size_) ++ring_filled_;
    }

    // Only run FFT when ring buffer is full
    if (ring_filled_ < fft_size_) return;

    // Copy ring buffer to FFT buffers (in correct order for contiguous FFT)
    // We need the most recent fft_size_ samples in chronological order
    uint32_t start = (ring_pos_ >= fft_size_) ? ring_pos_ - fft_size_ : 0;
    for (uint32_t i = 0; i < fft_size_; ++i) {
        uint32_t idx = (start + i) % fft_size_;
        real_[i] = ring_buf_[idx] * window_[i];
        imag_[i] = 0.0f;
    }

    // Compute FFT
    fft(real_, imag_, fft_size_);

    // Compute magnitude spectrum
    uint32_t bin_count = fft_size_ / 2;
    current_.bin_count = bin_count;
    current_.max_magnitude = 0.0f;
    current_.dominant_freq = 0.0f;

    // Normalize by FFT size and window correction factor
    // Window correction: Hanning = ~0.5 mean amplitude → multiply by 2
    float norm = 2.0f / static_cast<float>(fft_size_);

    for (uint32_t i = 0; i < bin_count; ++i) {
        float mag = std::sqrt(real_[i] * real_[i] + imag_[i] * imag_[i]) * norm;
        current_.bins[i] = mag;

        if (mag > current_.max_magnitude) {
            current_.max_magnitude = mag;
            current_.dominant_freq =
                static_cast<float>(i) *
                static_cast<float>(48000.0) / static_cast<float>(fft_size_);
        }
    }
}

// ─── Read ───────────────────────────────────────────────────────

SpectrumAnalyzer::Spectrum SpectrumAnalyzer::read() const {
    return current_;
}

} // namespace aria::metering
