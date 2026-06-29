#include "offline_renderer.h"

#include "../audio_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace aria {

OfflineRenderer::OfflineRenderer() = default;

// ═══════════════════════════════════════════════════════════════════
// Main render loop
// ═══════════════════════════════════════════════════════════════════

bool OfflineRenderer::render(AudioEngine& engine, const ExportConfig& config) {
    if (rendering_.exchange(true)) {
        return false; // already rendering
    }

    cancelled_.store(false, std::memory_order_release);
    progress_.store(0.0, std::memory_order_release);

    // Determine total frames to render
    // For now, we use a fixed duration (10 seconds) based on sample rate.
    // In a full implementation, range_start/range_end in PPQN would be
    // converted to sample positions via the tempo map.
    const uint32_t out_sr = config.sample_rate;
    const uint64_t total_frames = out_sr * 10; // default 10s
    const uint32_t block_size = engine.buffer_size() > 0 ? engine.buffer_size() : 1024;

    // Allocate output buffer (interleaved float)
    const uint32_t channels = 2; // stereo output
    std::vector<float> output(total_frames * channels, 0.0f);

    // Process in blocks, reusing the engine's graph at max speed
    uint64_t frames_written = 0;
    bool success = true;

    // We'll do multiple passes through the graph
    // In a real implementation, the clock drives the position.
    // For offline, we process the graph directly without real-time clock.
    const uint32_t out_channels = 2;

    while (frames_written < total_frames) {
        if (cancelled_.load(std::memory_order_acquire)) {
            success = false;
            break;
        }

        uint32_t block = static_cast<uint32_t>(
            std::min(static_cast<uint64_t>(block_size),
                     total_frames - frames_written));

        // Process the engine's graph
        // For offline rendering, we call process() which advances the
        // transport and graph. In a full implementation, the audio graph's
        // output nodes would be connected to capture the rendered audio.
        engine.process(block, frames_written);

        // Capture output from the engine's output nodes
        // For now, we read the master peak as a proxy — in a real
        // implementation, we'd read the output node buffers.
        // Placeholder: clear the output block (engine doesn't expose
        // output buffer capture yet — this would need an output node
        // connection in the full implementation).
        for (uint32_t ch = 0; ch < out_channels; ++ch) {
            float* dst = output.data() + frames_written * out_channels + ch;
            for (uint32_t f = 0; f < block; ++f) {
                dst[f * out_channels] = 0.0f;
            }
        }

        frames_written += block;

        // Update progress
        double p = static_cast<double>(frames_written) /
                   static_cast<double>(total_frames);
        progress_.store(p, std::memory_order_release);
    }

    if (success) {
        // Apply normalization if requested
        if (config.normalize != NormalizeMode::None) {
            apply_normalization(output.data(),
                                static_cast<uint32_t>(frames_written),
                                out_channels, config);
        }

        // Write output file
        switch (config.format) {
        case ExportFormat::WAV:
            success = FileWriter::write_wav(
                config.file_path, output.data(), out_channels,
                static_cast<uint32_t>(frames_written), out_sr,
                config.bit_depth, config.dither);
            break;
        case ExportFormat::AIFF:
            success = FileWriter::write_aiff(
                config.file_path, output.data(), out_channels,
                static_cast<uint32_t>(frames_written), out_sr,
                config.bit_depth, config.dither);
            break;
        case ExportFormat::FLAC:
            success = FileWriter::write_flac(
                config.file_path, output.data(), out_channels,
                static_cast<uint32_t>(frames_written), out_sr,
                config.bit_depth, config.dither);
            break;
        case ExportFormat::MP3:
            success = FileWriter::write_mp3(
                config.file_path, output.data(), out_channels,
                static_cast<uint32_t>(frames_written), out_sr,
                config.bit_depth, config.dither);
            break;
        case ExportFormat::OGG:
            success = FileWriter::write_ogg(
                config.file_path, output.data(), out_channels,
                static_cast<uint32_t>(frames_written), out_sr,
                config.bit_depth, config.dither);
            break;
        }
    }

    rendering_.store(false, std::memory_order_release);
    progress_.store(success ? 1.0 : 0.0, std::memory_order_release);
    return success;
}

// ═══════════════════════════════════════════════════════════════════
// Per-track render (Freeze/Bounce)
// ═══════════════════════════════════════════════════════════════════

bool OfflineRenderer::render_track(AudioEngine& engine, uint32_t track_index,
                                    uint32_t sample_rate,
                                    uint32_t duration_frames,
                                    std::vector<float>& output) {
    if (rendering_.exchange(true)) {
        return false; // already rendering
    }

    cancelled_.store(false, std::memory_order_release);
    progress_.store(0.0, std::memory_order_release);

    const uint32_t out_channels = 2; // stereo output
    const uint32_t block_size = engine.buffer_size() > 0
                                    ? engine.buffer_size()
                                    : 1024;

    // Validate track index
    if (track_index >= engine.track_count()) {
        output.clear();
        rendering_.store(false, std::memory_order_release);
        progress_.store(0.0, std::memory_order_release);
        return false;
    }

    // Allocate output buffer
    output.assign(duration_frames * out_channels, 0.0f);

    // sample_rate is reserved for future use (e.g., SRC conversion)
    (void)sample_rate;

    if (duration_frames == 0) {
        rendering_.store(false, std::memory_order_release);
        progress_.store(1.0, std::memory_order_release);
        return true;
    }

    // Save current mute states of all tracks
    const uint32_t num_tracks = engine.track_count();
    std::vector<bool> saved_mute(num_tracks);
    for (uint32_t i = 0; i < num_tracks; ++i) {
        auto* tp = engine.track(i);
        if (tp) {
            saved_mute[i] = tp->is_muted();
            // Mute all tracks except the target
            tp->set_muted(i != track_index);
        }
    }

    // Process engine blocks and capture output
    // Since the engine processes all tracks to the master output,
    // muting other tracks isolates the target track's contribution.
    // The master output represents only the target track's signal.
    // This is a practical approximation — a full per-track render
    // would use dedicated track output nodes in the audio graph.
    bool success = true;
    uint64_t frames_written = 0;

    while (frames_written < duration_frames) {
        if (cancelled_.load(std::memory_order_acquire)) {
            success = false;
            break;
        }

        const uint32_t block = static_cast<uint32_t>(
            std::min(static_cast<uint64_t>(block_size),
                     static_cast<uint64_t>(duration_frames) - frames_written));

        // Process the engine (advances transport and graph)
        engine.process(block, frames_written);

        // For now, capture is a placeholder — the engine's process()
        // writes to internal buffers. In a full implementation, we
        // would connect the track's output node to a capture buffer.
        // For the freeze use case, the track isolation via mute
        // ensures only the target track reaches the master bus.
        // We zero the output block here as a placeholder until the
        // engine exposes per-track output capture.
        // This ensures the buffer API contract is met (correct size,
        // non-garbage data) for the FreezeManager to use.
        for (uint32_t f = 0; f < block; ++f) {
            for (uint32_t ch = 0; ch < out_channels; ++ch) {
                output[(frames_written + f) * out_channels + ch] = 0.0f;
            }
        }

        frames_written += block;

        const double p = static_cast<double>(frames_written) /
                         static_cast<double>(duration_frames);
        progress_.store(p, std::memory_order_release);
    }

    // Restore mute states
    for (uint32_t i = 0; i < num_tracks; ++i) {
        auto* tp = engine.track(i);
        if (tp && i < saved_mute.size()) {
            tp->set_muted(saved_mute[i]);
        }
    }

    if (!success && frames_written < duration_frames) {
        output.clear();
    }

    rendering_.store(false, std::memory_order_release);
    progress_.store(success ? 1.0 : 0.0, std::memory_order_release);
    return success;
}

// ═══════════════════════════════════════════════════════════════════
// Normalization
// ═══════════════════════════════════════════════════════════════════

void OfflineRenderer::apply_normalization(float* data, uint32_t frames,
                                           uint32_t channels,
                                           const ExportConfig& config)
{
    if (frames == 0 || channels == 0) return;

    if (config.normalize == NormalizeMode::Peak) {
        double peak = calculate_peak(data, frames, channels);
        if (peak > 0.0) {
            double target_linear = std::pow(10.0, config.normalize_target / 20.0);
            double gain = target_linear / peak;
            for (uint32_t i = 0; i < frames * channels; ++i) {
                data[i] = static_cast<float>(data[i] * gain);
            }
        }
    } else if (config.normalize == NormalizeMode::LUFS) {
        double lufs = calculate_lufs(data, frames, channels, config.sample_rate);
        if (lufs < 0.0) {
            double gain_linear = std::pow(10.0, (config.normalize_target - lufs) / 20.0);
            for (uint32_t i = 0; i < frames * channels; ++i) {
                data[i] = static_cast<float>(data[i] * gain_linear);
            }
        }
    }
}

double OfflineRenderer::calculate_peak(const float* data, uint32_t frames,
                                        uint32_t channels)
{
    double peak = 0.0;
    const uint32_t total = frames * channels;
    for (uint32_t i = 0; i < total; ++i) {
        double abs_val = std::abs(static_cast<double>(data[i]));
        if (abs_val > peak) peak = abs_val;
    }
    return peak;
}

double OfflineRenderer::calculate_lufs(const float* data, uint32_t frames,
                                        uint32_t channels,
                                        uint32_t /*sample_rate*/)
{
    // Simplified LUFS calculation — full EBU R128 would use K-weighting
    // filter and gated loudness measurement.
    // This is a placeholder that computes mean squared energy.
    if (frames == 0 || channels == 0) return -70.0;

    double sum_sq = 0.0;
    const uint32_t total = frames * channels;
    for (uint32_t i = 0; i < total; ++i) {
        sum_sq += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    double mean_sq = sum_sq / static_cast<double>(total);
    if (mean_sq <= 0.0) return -70.0;
    return 10.0 * std::log10(mean_sq);
}

} // namespace aria
