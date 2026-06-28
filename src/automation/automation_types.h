#ifndef ARIA_AUTOMATION_TYPES_H
#define ARIA_AUTOMATION_TYPES_H

#include <cstdint>

namespace aria::automation {

// ─── Identifiers ──────────────────────────────────────────

/// Unique parameter identifier. 0 = invalid.
using ParameterID = uint64_t;

/// Unique automation clip identifier. 0 = invalid.
using AutomationClipID = uint64_t;

// ─── InterpolationType ────────────────────────────────────

/// Ten interpolation modes for curved automation.
enum class InterpolationType {
    Hold,           ///< No interpolation — step at next point
    Linear,         ///< Linear ramp between points
    Bezier,         ///< Cubic Bezier curve (requires control points)
    Smooth,         ///< Catmull-Rom / smoothstep
    EaseIn,         ///< Quadratic ease in (slow start)
    EaseOut,        ///< Quadratic ease out (fast start, slow end)
    EaseInOut,      ///< Smooth ease (slow-fast-slow)
    Exponential,    ///< Exponential curve (fast approach asymptote)
    Logarithmic,    ///< Logarithmic curve (slow start, fast end)
    SCurve          ///< Sigmoid S-curve
};

// ─── BezierControl ────────────────────────────────────────

/// Control points for cubic Bezier interpolation.
///
/// All coordinates are normalized 0..1 within the segment.
struct BezierControl {
    double x1 = 0.0, y1 = 0.0;  ///< Control point 1
    double x2 = 0.0, y2 = 0.0;  ///< Control point 2

    bool is_valid() const {
        return x1 >= 0.0 && x1 <= 1.0 &&
               y1 >= 0.0 && y1 <= 1.0 &&
               x2 >= 0.0 && x2 <= 1.0 &&
               y2 >= 0.0 && y2 <= 1.0;
    }
};

// ─── AutomationPoint ──────────────────────────────────────

/// A single automation point in a clip's curve.
///
/// Points are stored sorted by `ppqn`. The `interpolation` field
/// determines how the curve behaves when moving from this point to
/// the next. `bezier` is only meaningful when `interpolation == Bezier`.
struct AutomationPoint {
    uint64_t ppqn = 0;                          ///< Position in PPQN ticks
    double value = 0.0;                         ///< Normalized 0.0–1.0
    InterpolationType interpolation = InterpolationType::Linear;
    BezierControl bezier = {};                  ///< Only used when type == Bezier

    bool has_bezier() const {
        return interpolation == InterpolationType::Bezier;
    }
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_TYPES_H
