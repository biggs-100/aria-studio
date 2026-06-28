#ifndef ARIA_AUDIO_DIAGNOSTICS_H
#define ARIA_AUDIO_DIAGNOSTICS_H

#include <atomic>
#include <chrono>
#include <cstdint>

namespace aria {

/// Real-time audio diagnostics — x-run detection, callback timing.
///
/// Tracks performance metrics for the audio callback:
///   - x-run count (when callback exceeds buffer duration)
///   - max and average callback time
///   - per-callback timing
///
/// Thresholds:
///   - Single x-run: increments counter
///   - 5+ x-runs within 1 second: triggers auto buffer increment suggestion
///
/// All methods are real-time safe (no allocations, no locks).
class AudioDiagnostics {
public:
    AudioDiagnostics() = default;

    // ─── Callback lifecycle ────────────────────────────────────

    /// Begin timing an audio callback.
    /// Must be called at the START of the audio callback.
    void begin_callback();

    /// End timing an audio callback.
    /// Must be called at the END of the audio callback.
    /// Automatically detects x-runs and updates statistics.
    void end_callback();

    // ─── Configuration ─────────────────────────────────────────

    /// Set the buffer duration in microseconds.
    /// Used to detect x-runs (callback_time > buffer_duration).
    /// @param us  Buffer duration in microseconds.
    void set_buffer_duration_us(uint64_t us) {
        buffer_duration_us_.store(us, std::memory_order_release);
    }

    /// Get the buffer duration in microseconds.
    uint64_t buffer_duration_us() const {
        return buffer_duration_us_.load(std::memory_order_relaxed);
    }

    // ─── Statistics ────────────────────────────────────────────

    /// Number of x-runs detected since the last reset.
    uint64_t x_run_count() const {
        return x_run_count_.load(std::memory_order_relaxed);
    }

    /// Maximum callback time in milliseconds.
    double max_callback_time_ms() const {
        return max_callback_time_ms_.load(std::memory_order_relaxed);
    }

    /// Average callback time in milliseconds.
    double avg_callback_time_ms() const;

    /// Whether the audio callback is currently performing.
    bool is_performing() const {
        return performing_.load(std::memory_order_relaxed);
    }

    /// Whether an x-run burst has been detected (5+ x-runs in 1s window).
    /// This is a suggestion to auto-increase the buffer size.
    bool x_run_burst_detected() const {
        return burst_detected_.load(std::memory_order_relaxed);
    }

    /// Total number of callbacks processed.
    uint64_t callback_count() const {
        return callback_count_.load(std::memory_order_relaxed);
    }

    /// Reset all statistics.
    void reset();

private:
    /// Detect x-run: if callback time exceeds buffer duration,
    /// increment x-run counter and check for burst.
    void detect_x_run(uint64_t callback_us);

    // ─── State ─────────────────────────────────────────────────

    // Timing
    std::chrono::high_resolution_clock::time_point callback_start_;
    std::atomic<uint64_t> buffer_duration_us_{0};

    // Callback time tracking
    std::atomic<uint64_t> total_callback_time_us_{0};
    std::atomic<uint64_t> callback_count_{0};
    std::atomic<double>   max_callback_time_ms_{0.0};

    // X-run tracking
    std::atomic<uint64_t> x_run_count_{0};
    std::atomic<bool>     performing_{false};
    std::atomic<bool>     burst_detected_{false};

    // Burst detection: track x-run timestamps within a 1-second window
    static constexpr uint32_t kBurstThreshold = 5;
    static constexpr uint64_t kBurstWindowUs  = 1'000'000;  // 1 second
    uint64_t x_run_timestamps_[kBurstThreshold] = {};
    uint32_t x_run_index_ = 0;
};

} // namespace aria

#endif // ARIA_AUDIO_DIAGNOSTICS_H
