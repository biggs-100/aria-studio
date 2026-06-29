#ifndef ARIA_OFFLINE_RENDERER_H
#define ARIA_OFFLINE_RENDERER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "file_writer.h"

namespace aria {

class AudioEngine;

/// Offline renderer for exporting audio projects to disk.
///
/// Processes the same audio graph used in real-time playback, but runs
/// at maximum speed (no real-time constraints). Supports progress
/// reporting, cancellation, normalization, and range selection.
///
/// Thread safety:
///   - render() is blocking and should be called from a worker thread.
///   - progress() and is_rendering() are safe to call from any thread.
///   - cancel() is safe to call from any thread.
class OfflineRenderer {
public:
    OfflineRenderer();

    /// Export configuration.
    struct ExportConfig {
        std::string file_path;                  ///< Output file path.
        ExportFormat format = ExportFormat::WAV; ///< Output format.
        uint32_t    sample_rate = 48000;         ///< Output sample rate.
        uint16_t    bit_depth = 24;              ///< Output bit depth (16/24/32/0=float).
        DitherType  dither = DitherType::None;   ///< Dither type.
        NormalizeMode normalize = NormalizeMode::None;  ///< Normalization mode.
        double      normalize_target = -1.0;     ///< Target: dB for peak, LUFS for loudness.
        uint64_t    range_start = 0;             ///< Start position (PPQN).
        uint64_t    range_end = 0;               ///< End position (0 = entire project).
        bool        include_master_fx = true;    ///< Include master bus effects.
    };

    /// Render the project to a file.
    /// @param engine  The AudioEngine to render from.
    /// @param config  Export configuration.
    /// @return true on success.
    bool render(AudioEngine& engine, const ExportConfig& config);

    /// Render a single track to an interleaved float buffer.
    ///
    /// Processes the engine at maximum speed for the given duration,
    /// isolating the specified track by muting other tracks. The
    /// output buffer is resized to duration_frames * 2 (stereo).
    ///
    /// Thread safety:
    ///   - render_track() is blocking and should be called from a
    ///     worker thread.
    ///   - Like render(), only one render_track() can run at a time
    ///     on a given OfflineRenderer instance.
    ///
    /// @param engine           Audio engine to render from.
    /// @param track_index      Index of the track processor to render.
    /// @param sample_rate      Output sample rate in Hz.
    /// @param duration_frames  Number of output frames to render.
    /// @param output           [out] Interleaved float samples.
    ///                        Resized to duration_frames * channels.
    /// @return true on success, false if track_index is invalid or
    ///         another render is already in progress.
    bool render_track(AudioEngine& engine, uint32_t track_index,
                      uint32_t sample_rate, uint32_t duration_frames,
                      std::vector<float>& output);

    /// Current progress (0.0 – 1.0).
    double progress() const { return progress_.load(std::memory_order_acquire); }

    /// Whether rendering is in progress.
    bool is_rendering() const { return rendering_.load(std::memory_order_acquire); }

    /// Cancel the current render operation.
    void cancel() { cancelled_.store(true, std::memory_order_release); }

private:
    /// Apply normalization to the rendered data.
    void apply_normalization(float* data, uint32_t frames, uint32_t channels,
                             const ExportConfig& config);

    /// Calculate peak value across all channels.
    double calculate_peak(const float* data, uint32_t frames, uint32_t channels);

    /// Calculate integrated LUFS (simplified).
    double calculate_lufs(const float* data, uint32_t frames, uint32_t channels,
                          uint32_t sample_rate);

    std::atomic<double>  progress_{0.0};
    std::atomic<bool>    rendering_{false};
    std::atomic<bool>    cancelled_{false};
};

} // namespace aria

#endif // ARIA_OFFLINE_RENDERER_H
