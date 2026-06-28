#ifndef ARIA_AUTOMATION_RECORDER_H
#define ARIA_AUTOMATION_RECORDER_H

#include "automation_types.h"

#include <cstdint>
#include <vector>

namespace aria::automation {

/// Real-time automation recorder with smoothing and point reduction.
///
/// Captures parameter values from the audio thread into a ring buffer,
/// applies configurable exponential smoothing, and provides
/// Douglas-Peucker point reduction for converting dense recordings
/// into compact automation curves.
class AutomationRecorder {
public:
    explicit AutomationRecorder(size_t ring_buffer_capacity = 44100);

    // ─── Lifecycle ────────────────────────────────────────────

    /// Start recording. Called from the main thread.
    /// @param start_ppqn  The PPQN position at recording start.
    /// @param sample_rate The audio sample rate (for smoothing).
    void start_recording(uint64_t start_ppqn, double sample_rate);

    /// Stop recording. Called from the main thread.
    void stop_recording();

    /// Whether recording is currently active.
    bool is_recording() const;

    // ─── Data Capture ─────────────────────────────────────────

    /// Record a sample from the audio thread.
    /// Applies exponential smoothing if configured.
    void record_value(uint64_t ppqn, double value);

    /// Configure smoothing time in milliseconds.
    /// Default is 10ms. Set to 0 to disable.
    void set_smoothing(double ms);

    // ─── Point Reduction ──────────────────────────────────────

    /// Reduction mode for Douglas-Peucker simplification.
    enum class ReductionMode {
        ReduceOnStop,  ///< Simplify once when recording stops.
        Adaptive,      ///< Simplify periodically during recording.
        Manual         ///< User calls reduce_points() explicitly.
    };

    /// Run Douglas-Peucker reduction on the recorded buffer.
    /// @param mode      Reduction mode (default: ReduceOnStop).
    /// @param tolerance Perpendicular-distance tolerance (default: 0.005).
    void reduce_points(ReductionMode mode = ReductionMode::ReduceOnStop,
                       double tolerance = 0.005);

    // ─── Results ──────────────────────────────────────────────

    /// Take the current buffer of points (moves out the internal buffer).
    std::vector<AutomationPoint> take_result();

    /// Number of points currently in the buffer.
    size_t point_count() const;

private:
    // ─── Douglas-Peucker helpers ──────────────────────────────

    /// Perpendicular distance from point pt to the line (a, b).
    static double perpendicular_distance(const AutomationPoint& pt,
                                          const AutomationPoint& a,
                                          const AutomationPoint& b);

    /// Recursive Douglas-Peucker simplification.
    std::vector<AutomationPoint> douglas_peucker(
        const std::vector<AutomationPoint>& points,
        double tolerance) const;

    // ─── Internal state ───────────────────────────────────────

    std::vector<AutomationPoint> buffer_;
    uint64_t start_ppqn_ = 0;
    double sample_rate_ = 44100.0;
    bool recording_ = false;
    double last_value_ = 0.0;
    uint64_t last_ppqn_ = 0;

    // Smoothing state
    double smoothing_ms_ = 10.0;
    double smoothed_value_ = 0.0;
    bool has_smoothed_ = false;

    // Capacity (ring-buffer wrap limit)
    size_t capacity_ = 44100;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_RECORDER_H
