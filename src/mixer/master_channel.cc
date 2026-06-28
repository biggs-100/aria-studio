#include "mixer/master_channel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace aria {

// ═══════════════════════════════════════════════════════════════════
//  MasterChannel
// ═══════════════════════════════════════════════════════════════════

MasterChannel::MasterChannel() = default;

void MasterChannel::set_volume(double db) {
    volume_db_ = std::clamp(db, -120.0, 24.0);
}

void MasterChannel::set_limiter(const LimiterConfig& config) {
    LimiterConfig c;
    c.enabled      = config.enabled;
    c.threshold_db = std::clamp(config.threshold_db, -6.0, -0.1);
    c.ceiling_db   = std::clamp(config.ceiling_db,   -6.0, -0.1);
    c.release_ms   = std::max(config.release_ms, 1.0);
    limiter_config_ = c;

    // Sync internal limiter config
    limiter_.threshold_db = c.threshold_db;
    limiter_.release_ms   = c.release_ms;
}

void MasterChannel::set_dither(DitherType type, uint16_t bit_depth) {
    dither_    = type;
    bit_depth_ = std::clamp(bit_depth, uint16_t{8}, uint16_t{32});
}

void MasterChannel::add_fx(PluginID plugin, uint32_t position) {
    if (position >= fx_chain_.size()) {
        fx_chain_.push_back(plugin);
    } else {
        fx_chain_.insert(
            fx_chain_.begin() + static_cast<ptrdiff_t>(position),
            plugin);
    }
}

// ── Processing ───────────────────────────────────────────────────

void MasterChannel::process(const AudioBuffer* input,
                            AudioBuffer* output, uint32_t frames) {
    if (!input || !output || frames == 0) return;

    uint32_t nc = std::min(input->channels, output->channels);
    if (nc == 0) return;

    // Copy input to output (planar copy)
    for (uint32_t ch = 0; ch < nc; ++ch) {
        if (input->data[ch] && output->data[ch]) {
            std::memcpy(output->data[ch], input->data[ch],
                        frames * sizeof(float));
        }
    }
    output->frames  = frames;
    output->channels = nc;

    // 1. Apply master volume (linear gain)
    if (volume_db_ != 0.0) {
        float gain = static_cast<float>(Channel::db_to_linear(volume_db_));
        for (uint32_t ch = 0; ch < nc; ++ch) {
            float* samples = output->data[ch];
            if (!samples) continue;
            for (uint32_t i = 0; i < frames; ++i) {
                samples[i] *= gain;
            }
        }
    }

    // 2. Master FX chain (delegated — actual plugin processing
    //    is handled externally by the plugin host).
    //    Here the chain order is tracked and plugins are applied
    //    by the audio engine when processing the master bus.
    //    (No-op in the standalone processor.)

    // 3. Brickwall limiter
    if (limiter_config_.enabled) {
        // Process interleaved for limiter (simplified: process all channels)
        // The limiter works on the first two channels (stereo) or mono.
        float* interleaved = nullptr;
        std::vector<float> scratch;

        uint32_t process_ch = std::min(nc, uint32_t{2});

        if (process_ch == 1) {
            limiter_.process(output->data[0], frames, 1, sample_rate_);
        } else {
            // Interleave for stereo limiter processing
            scratch.resize(frames * 2);
            for (uint32_t i = 0; i < frames; ++i) {
                scratch[i * 2 + 0] = output->data[0] ? output->data[0][i] : 0.0f;
                scratch[i * 2 + 1] = output->data[1] ? output->data[1][i] : 0.0f;
            }

            limiter_.process(scratch.data(), frames, 2, sample_rate_);

            // De-interleave back
            for (uint32_t i = 0; i < frames; ++i) {
                if (output->data[0]) output->data[0][i] = scratch[i * 2 + 0];
                if (output->data[1]) output->data[1][i] = scratch[i * 2 + 1];
            }
        }

        last_gain_reduction_db_ = Channel::linear_to_db(
            static_cast<double>(limiter_.gain_reduction));
    } else {
        last_gain_reduction_db_ = 0.0;
    }

    // 4. Dither
    if (dither_ == Triangular) {
        apply_triangular_dither(output->data[0], frames, nc);
    } else if (dither_ == Shaped) {
        apply_shaped_dither(output->data[0], frames, nc);
    }

    // Ensure ceiling is respected after limiter + dither
    if (limiter_config_.enabled) {
        float ceiling = std::pow(10.0f, static_cast<float>(limiter_config_.ceiling_db) / 20.0f);
        for (uint32_t ch = 0; ch < nc; ++ch) {
            float* samples = output->data[ch];
            if (!samples) continue;
            for (uint32_t i = 0; i < frames; ++i) {
                samples[i] = std::clamp(samples[i], -ceiling, ceiling);
            }
        }
    }
}

// ── Brickwall Limiter ────────────────────────────────────────────

void MasterChannel::BrickwallLimiter::reset() {
    envelope = 0.0;
    gain_reduction = 1.0;
}

void MasterChannel::BrickwallLimiter::process(
    float* samples, uint32_t frames, uint32_t channels, double sample_rate) {

    if (!samples || frames == 0 || channels == 0) return;

    double threshold_linear = std::pow(10.0, threshold_db / 20.0);
    double release_coeff = std::exp(-1.0 / (release_ms * 0.001 * sample_rate));

    for (uint32_t i = 0; i < frames; ++i) {
        // Find peak across channels at this sample
        double peak = 0.0;
        for (uint32_t ch = 0; ch < channels; ++ch) {
            double abs_val = std::abs(static_cast<double>(samples[i * channels + ch]));
            if (abs_val > peak) peak = abs_val;
        }

        // Envelope follower (peak detector with release)
        if (peak > envelope) {
            envelope = peak;  // Instant attack
        } else {
            envelope += (peak - envelope) * release_coeff;
        }

        // Compute desired gain reduction
        if (envelope > threshold_linear) {
            gain_reduction = threshold_linear / envelope;
        } else {
            gain_reduction = 1.0;
        }

        // Apply gain reduction to all channels
        for (uint32_t ch = 0; ch < channels; ++ch) {
            samples[i * channels + ch] *= static_cast<float>(gain_reduction);
        }
    }
}

// ── Dither ───────────────────────────────────────────────────────

void MasterChannel::apply_triangular_dither(
    float* samples, uint32_t frames, uint32_t channels) {

    if (!samples || frames == 0 || channels == 0) return;

    double lsb = 1.0 / (1 << (bit_depth_ - 1));
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (uint32_t ch = 0; ch < channels; ++ch) {
        float* ch_samples = samples + ch * frames;
        if (!ch_samples) continue;
        for (uint32_t i = 0; i < frames; ++i) {
            // Triangular: sum of two uniform random values
            double noise = (dist(rng_) + dist(rng_)) * lsb * 0.5;
            ch_samples[i] += static_cast<float>(noise);
        }
    }
}

void MasterChannel::apply_shaped_dither(
    float* samples, uint32_t frames, uint32_t channels) {

    if (!samples || frames == 0 || channels == 0) return;

    double lsb = 1.0 / (1 << (bit_depth_ - 1));
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (uint32_t ch = 0; ch < channels; ++ch) {
        float* ch_samples = samples + ch * frames;
        if (!ch_samples) continue;
        double error = 0.0;

        for (uint32_t i = 0; i < frames; ++i) {
            // Triangular noise
            double noise = (dist(rng_) + dist(rng_)) * lsb * 0.5;

            // First-order highpass noise shaping: subtract previous error
            double shaped_noise = noise - error;

            double original = static_cast<double>(ch_samples[i]);
            double dithered = original + shaped_noise;

            // Quantize
            double quantized = std::round(dithered / lsb) * lsb;

            // Update error for next sample (highpass shaping)
            error = (dithered - quantized) * 0.5;

            ch_samples[i] = static_cast<float>(quantized);
        }
    }
}

} // namespace aria
