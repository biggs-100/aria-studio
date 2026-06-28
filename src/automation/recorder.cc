#include "recorder.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>

namespace aria::automation {

// ─── Construction ──────────────────────────────────────────

AutomationRecorder::AutomationRecorder(size_t ring_buffer_capacity)
    : capacity_(ring_buffer_capacity) {
    buffer_.reserve(capacity_);
}

// ─── Lifecycle ─────────────────────────────────────────────

void AutomationRecorder::start_recording(uint64_t start_ppqn,
                                          double sample_rate) {
    buffer_.clear();
    start_ppqn_ = start_ppqn;
    sample_rate_ = sample_rate;
    recording_ = true;
    last_value_ = 0.0;
    last_ppqn_ = start_ppqn;
    has_smoothed_ = false;
    smoothed_value_ = 0.0;
}

void AutomationRecorder::stop_recording() {
    recording_ = false;
}

bool AutomationRecorder::is_recording() const {
    return recording_;
}

// ─── Data Capture ──────────────────────────────────────────

void AutomationRecorder::set_smoothing(double ms) {
    smoothing_ms_ = std::max(0.0, ms);
}

void AutomationRecorder::record_value(uint64_t ppqn, double value) {
    if (!recording_) return;

    // Apply exponential smoothing
    double sample = value;
    if (smoothing_ms_ > 0.0 && sample_rate_ > 0.0) {
        double smoothing_samples = (smoothing_ms_ / 1000.0) * sample_rate_;
        double alpha = 1.0 / std::max(1.0, smoothing_samples);
        if (!has_smoothed_) {
            smoothed_value_ = value;
            has_smoothed_ = true;
        } else {
            smoothed_value_ += alpha * (value - smoothed_value_);
        }
        sample = smoothed_value_;
    }

    // Ring-buffer wrap: remove oldest samples when at capacity
    if (buffer_.size() >= capacity_) {
        // Erase the first ~25% to make room
        size_t erase_count = capacity_ / 4;
        buffer_.erase(buffer_.begin(),
                       buffer_.begin() + erase_count);
    }

    buffer_.push_back({ppqn, sample, InterpolationType::Linear, {}});
    last_value_ = sample;
    last_ppqn_ = ppqn;
}

// ─── Point Reduction ───────────────────────────────────────

void AutomationRecorder::reduce_points(ReductionMode /*mode*/,
                                        double tolerance) {
    if (buffer_.size() <= 2) return;

    // All three modes apply D-P reduction to the current buffer.
    // ReduceOnStop: called by stop_recording().
    // Adaptive: would throttle during recording in a real-time context;
    //           here we reduce the accumulated buffer in one pass.
    // Manual: explicit user invocation.
    auto reduced = douglas_peucker(buffer_, tolerance);
    buffer_ = std::move(reduced);
}

// ─── Results ───────────────────────────────────────────────

std::vector<AutomationPoint> AutomationRecorder::take_result() {
    return std::move(buffer_);
}

size_t AutomationRecorder::point_count() const {
    return buffer_.size();
}

// ─── Douglas-Peucker Implementation ────────────────────────

double AutomationRecorder::perpendicular_distance(
    const AutomationPoint& pt,
    const AutomationPoint& a,
    const AutomationPoint& b)
{
    double ax = static_cast<double>(a.ppqn);
    double ay = a.value;
    double bx = static_cast<double>(b.ppqn);
    double by = b.value;
    double px = static_cast<double>(pt.ppqn);
    double py = pt.value;

    double dx = bx - ax;
    double dy = by - ay;
    double length_sq = dx * dx + dy * dy;

    if (length_sq == 0.0) {
        // a and b are coincident — return Euclidean distance from pt to a
        double ex = px - ax;
        double ey = py - ay;
        return std::sqrt(ex * ex + ey * ey);
    }

    // Project pt onto the infinite line, clamp to segment
    double t = ((px - ax) * dx + (py - ay) * dy) / length_sq;
    t = std::clamp(t, 0.0, 1.0);

    double proj_x = ax + t * dx;
    double proj_y = ay + t * dy;

    double ex = px - proj_x;
    double ey = py - proj_y;
    return std::sqrt(ex * ex + ey * ey);
}

std::vector<AutomationPoint> AutomationRecorder::douglas_peucker(
    const std::vector<AutomationPoint>& points,
    double tolerance) const
{
    if (points.size() <= 2) return points;

    // Find the point with the maximum perpendicular distance from
    // the line between the first and last points.
    double dmax = 0.0;
    size_t index = 0;

    for (size_t i = 1; i + 1 < points.size(); ++i) {
        double d = perpendicular_distance(points[i],
                                          points.front(),
                                          points.back());
        if (d > dmax) {
            dmax = d;
            index = i;
        }
    }

    if (dmax > tolerance) {
        // Recurse on both halves
        auto left = douglas_peucker(
            std::vector<AutomationPoint>(points.begin(),
                                          points.begin() + index + 1),
            tolerance);
        auto right = douglas_peucker(
            std::vector<AutomationPoint>(points.begin() + index,
                                          points.end()),
            tolerance);

        // Merge: left + right[1..] to avoid duplicating the split point
        left.reserve(left.size() + right.size() - 1);
        std::copy(right.begin() + 1, right.end(), std::back_inserter(left));
        return left;
    } else {
        return {points.front(), points.back()};
    }
}

} // namespace aria::automation
