#include "audio/wsola.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>

namespace aria {

// ─── Constants ─────────────────────────────────────────────────

/// Search range for cross-correlation (proportion of hop).
static constexpr double kSearchFraction = 0.125;

/// Ring buffer scale factor (relative to window_size).
static constexpr uint32_t kInputScale = 4;
static constexpr uint32_t kOutputScale = 8;

// ─── Construction ──────────────────────────────────────────────

WSOLAStretch::WSOLAStretch(double sample_rate, uint32_t channels,
                             double window_ms, double overlap)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , window_ms_(window_ms)
    , overlap_(overlap) {

    window_size_ = static_cast<uint32_t>(sample_rate_ * window_ms_ / 1000.0);
    if (window_size_ < 64) window_size_ = 64;
    if (window_size_ > 16384) window_size_ = 16384;

    // Synthesis hop = window * (1 - overlap)
    synthesis_hop_ = static_cast<uint32_t>(
        static_cast<double>(window_size_) * (1.0 - overlap_));
    if (synthesis_hop_ < 1) synthesis_hop_ = 1;
    if (synthesis_hop_ > window_size_) synthesis_hop_ = window_size_;

    analysis_hop_ = static_cast<double>(synthesis_hop_);

    // Latency = one synthesis hop
    latency_samples_ = synthesis_hop_;

    // Input ring: large enough for analysis_hop * frames
    input_capacity_ = window_size_ * kInputScale;
    input_buf_.resize(input_capacity_ * channels_, 0.0f);

    // Output ring: large enough to hold several window overlaps
    output_capacity_ = window_size_ * kOutputScale;
    output_buf_.resize(output_capacity_ * channels_, 0.0f);
    weight_buf_.resize(output_capacity_, 0.0f);

    build_window();
}

void WSOLAStretch::build_window() {
    window_.resize(window_size_);
    for (uint32_t i = 0; i < window_size_; ++i) {
        double phase = 2.0 * std::numbers::pi_v<double> *
                       static_cast<double>(i) /
                       static_cast<double>(window_size_ - 1);
        window_[i] = static_cast<float>(0.5 * (1.0 - std::cos(phase)));
    }
}

// ─── Ratio ─────────────────────────────────────────────────────

void WSOLAStretch::set_ratio(double ratio) {
    if (ratio < 0.5) ratio = 0.5;
    if (ratio > 2.0) ratio = 2.0;
    ratio_ = ratio;
    // Larger ratio = faster playback = larger analysis hop
    analysis_hop_ = static_cast<double>(synthesis_hop_) * ratio_;
}

// ─── Reset ─────────────────────────────────────────────────────

void WSOLAStretch::reset() {
    std::fill(input_buf_.begin(), input_buf_.end(), 0.0f);
    std::fill(output_buf_.begin(), output_buf_.end(), 0.0f);
    std::fill(weight_buf_.begin(), weight_buf_.end(), 0.0f);
    input_frames_written_ = 0;
    input_analysis_pos_ = 0.0;
    output_frames_written_ = 0;
    output_frames_read_ = 0;
    first_frame_ = true;
}

uint32_t WSOLAStretch::latency() const {
    return latency_samples_;
}

// ─── Core WSOLA processing ─────────────────────────────────────

uint32_t WSOLAStretch::process(const float* input, float* output,
                                uint32_t input_frames,
                                uint32_t max_output_frames) {
    if (input_frames == 0 || channels_ == 0) return 0;

    // ── Step 1: Copy input to ring buffer ────────────────────
    for (uint32_t c = 0; c < channels_; ++c) {
        const float* src = input + c * input_frames;
        float* ring = &input_buf_[c * input_capacity_];
        for (uint32_t i = 0; i < input_frames; ++i) {
            uint32_t pos = static_cast<uint32_t>(
                (input_frames_written_ + i) % input_capacity_);
            ring[pos] = src[i];
        }
    }
    input_frames_written_ += input_frames;

    // ── Step 2: WSOLA synthesis loop ─────────────────────────
    while (output_frames_written_ - output_frames_read_ + window_size_ <
           output_capacity_) {

        double next_analysis_end = input_analysis_pos_ +
                                   static_cast<double>(window_size_);
        if (next_analysis_end > static_cast<double>(input_frames_written_)) {
            break;
        }

        // ── Step 2a: Read and window analysis frame ──────────
        uint32_t analysis_int = static_cast<uint32_t>(input_analysis_pos_);
        std::vector<float> windowed(window_size_ * channels_);

        for (uint32_t c = 0; c < channels_; ++c) {
            const float* ring = &input_buf_[c * input_capacity_];
            for (uint32_t i = 0; i < window_size_; ++i) {
                uint32_t pos = static_cast<uint32_t>(
                    (analysis_int + i) % input_capacity_);
                windowed[c * window_size_ + i] = ring[pos] * window_[i];
            }
        }

        // ── Step 2b: Similarity search ──────────────────────
        uint32_t overlap_offset = 0;
        if (!first_frame_) {
            uint64_t overlap_start = output_frames_written_ > synthesis_hop_
                ? output_frames_written_ - synthesis_hop_
                : 0;
            uint32_t search_len = synthesis_hop_;

            const float* ch0_frame = &windowed[0];
            std::vector<float> overlap_buf(search_len);
            for (uint32_t i = 0; i < search_len; ++i) {
                uint32_t pos = static_cast<uint32_t>(
                    (overlap_start + i) % output_capacity_);
                overlap_buf[i] = output_buf_[pos] / (weight_buf_[pos] + 1e-10f);
            }

            overlap_offset = find_best_overlap(
                ch0_frame, overlap_buf.data(), search_len);
        }

        // ── Step 2c: Overlap-add ────────────────────────────
        uint32_t write_offset = overlap_offset;
        for (uint32_t c = 0; c < channels_; ++c) {
            float* ring = &output_buf_[c * output_capacity_];
            const float* frame = &windowed[c * window_size_];

            for (uint32_t i = 0; i < window_size_; ++i) {
                uint64_t wp = output_frames_written_ +
                              static_cast<uint64_t>(i) + write_offset;
                uint32_t pos = static_cast<uint32_t>(wp % output_capacity_);
                ring[pos] += frame[i];
            }
        }

        for (uint32_t i = 0; i < window_size_; ++i) {
            uint64_t wp = output_frames_written_ +
                          static_cast<uint64_t>(i) + write_offset;
            uint32_t pos = static_cast<uint32_t>(wp % output_capacity_);
            weight_buf_[pos] += window_[i];
        }

        // ── Step 2d: Advance ─────────────────────────────────
        input_analysis_pos_ += analysis_hop_;
        output_frames_written_ += synthesis_hop_;
        first_frame_ = false;
    }

    // ── Step 3: Read output with weight normalization ────────
    uint64_t avail = output_frames_written_ > output_frames_read_
        ? output_frames_written_ - output_frames_read_
        : 0;
    uint32_t to_read = static_cast<uint32_t>(
        std::min<uint64_t>(avail, max_output_frames));

    for (uint32_t c = 0; c < channels_; ++c) {
        float* dst = output + c * max_output_frames;
        const float* ring = &output_buf_[c * output_capacity_];

        for (uint32_t i = 0; i < to_read; ++i) {
            uint64_t rp = output_frames_read_ + i;
            uint32_t ridx = static_cast<uint32_t>(rp % output_capacity_);
            float w = weight_buf_[ridx];
            if (w > 1e-10f) {
                dst[i] = ring[ridx] / w;
            } else {
                dst[i] = 0.0f;
            }
        }
        // Zero remaining output buffer (up to max_output_frames)
        for (uint32_t i = to_read; i < max_output_frames; ++i) {
            dst[i] = 0.0f;
        }
    }

    // ── Step 4: Zero consumed output region ─────────────────
    for (uint64_t i = 0; i < to_read; ++i) {
        uint64_t cp = output_frames_read_ + i;
        uint32_t ridx = static_cast<uint32_t>(cp % output_capacity_);
        weight_buf_[ridx] = 0.0f;
        for (uint32_t c = 0; c < channels_; ++c) {
            output_buf_[c * output_capacity_ + ridx] = 0.0f;
        }
    }
    output_frames_read_ += to_read;

    return to_read;
}

// ─── Similarity search ─────────────────────────────────────────

uint32_t WSOLAStretch::find_best_overlap(const float* analysis,
                                          const float* overlap,
                                          uint32_t len) const {
    uint32_t search_radius = static_cast<uint32_t>(
        static_cast<double>(len) * kSearchFraction);
    if (search_radius < 1) search_radius = 1;

    uint32_t search_len = 2 * search_radius;
    if (search_len > len) search_len = len;

    uint32_t best_offset = 0;
    double best_corr = -std::numeric_limits<double>::max();

    for (uint32_t offset = 0; offset <= search_len; ++offset) {
        uint32_t corr_len = len - offset;
        if (corr_len < 4) continue;

        double corr = 0.0;
        double norm_a = 0.0;
        double norm_o = 0.0;

        for (uint32_t i = 0; i < corr_len; ++i) {
            double a = analysis[i + offset];
            double o = overlap[i];
            corr += a * o;
            norm_a += a * a;
            norm_o += o * o;
        }

        double denom = std::sqrt(norm_a * norm_o);
        if (denom > 1e-10) {
            corr /= denom;
        }

        if (corr > best_corr) {
            best_corr = corr;
            best_offset = offset;
        }
    }

    return best_offset;
}

} // namespace aria
