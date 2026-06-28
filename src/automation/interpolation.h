#ifndef ARIA_AUTOMATION_INTERPOLATION_H
#define ARIA_AUTOMATION_INTERPOLATION_H

#include "automation_types.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace aria::automation {

// ─── Interpolation Functions ──────────────────────────────
/// @{
/// All functions take `t` in [0, 1] and return a normalized value in [0, 1].

/// Hold — output is 0.0 for all `t < 1.0`, then 1.0 at endpoint.
inline double eval_hold(double t) {
    return t >= 1.0 ? 1.0 : 0.0;
}

/// Linear — direct proportional mapping.
inline double eval_linear(double t) {
    return t;
}

/// Cubic Bezier — evaluated using the De Casteljau approach with
/// endpoints fixed at (0,0) and (1,1) and control points from BezierControl.
inline double eval_bezier(double t, double cp1_y, double cp2_y) {
    const double omt = 1.0 - t;
    const double omt2 = omt * omt;
    const double omt3 = omt2 * omt;
    const double t2 = t * t;
    const double t3 = t2 * t;
    // P0 = (0,0), P3 = (1,1) — only Y matters here; X used for t-remap in evaluate_segment
    return omt3 * 0.0 + 3.0 * omt2 * t * cp1_y + 3.0 * omt * t2 * cp2_y + t3 * 1.0;
}

/// Smooth — Catmull-Rom / smoothstep (3t² − 2t³).
inline double eval_smooth(double t) {
    return t * t * (3.0 - 2.0 * t);
}

/// EaseIn — quadratic (t²).
inline double eval_ease_in(double t) {
    return t * t;
}

/// EaseOut — quadratic (1 − (1−t)²).
inline double eval_ease_out(double t) {
    return t * (2.0 - t);
}

/// EaseInOut — Hermite (2t² for t<0.5, else 1 − 2(1−t)²).
inline double eval_ease_in_out(double t) {
    return t < 0.5
        ? 2.0 * t * t
        : -1.0 + (4.0 - 2.0 * t) * t;
}

/// Exponential — (e^{6t} − 1) / (e^6 − 1). Fast rise near end.
inline double eval_exponential(double t) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return (std::exp(6.0 * t) - 1.0) / (std::exp(6.0) - 1.0);
}

/// Logarithmic — log10(1 + 9t). Fast rise near start.
inline double eval_logarithmic(double t) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return std::log(1.0 + 9.0 * t) / std::log(10.0);
}

/// S-Curve — sigmoid 1 / (1 + e^{−12(t−0.5)}). Slow-fast-slow.
inline double eval_s_curve(double t) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return 1.0 / (1.0 + std::exp(-12.0 * (t - 0.5)));
}
/// @}

// ─── Interpolation Function Table ─────────────────────────

using InterpolationFunc = double(*)(double);

struct InterpolationEntry {
    InterpolationType type;
    InterpolationFunc func;          ///< nullptr for Bezier (handled specially)
};

inline constexpr std::array<InterpolationEntry, 10> kInterpolationTable = {{
    { InterpolationType::Hold,        eval_hold        },
    { InterpolationType::Linear,      eval_linear      },
    { InterpolationType::Bezier,      nullptr           },  // custom eval
    { InterpolationType::Smooth,      eval_smooth      },
    { InterpolationType::EaseIn,      eval_ease_in     },
    { InterpolationType::EaseOut,     eval_ease_out    },
    { InterpolationType::EaseInOut,   eval_ease_in_out },
    { InterpolationType::Exponential, eval_exponential },
    { InterpolationType::Logarithmic, eval_logarithmic },
    { InterpolationType::SCurve,      eval_s_curve     },
}};

// ─── evaluate_segment ─────────────────────────────────────

/// Evaluate one curve segment between two automation points.
///
/// @param t             Normalised position within the segment [0, 1]
/// @param start_value   Value at the left point
/// @param end_value     Value at the right point
/// @param type          Interpolation type to use
/// @param bezier        Control points (only used for Bezier)
/// @return Interpolated value mapped into [start_value, end_value]
inline double evaluate_segment(double t,
                                double start_value,
                                double end_value,
                                InterpolationType type,
                                const BezierControl& bezier = {})
{
    t = std::clamp(t, 0.0, 1.0);

    double normalized;
    if (type == InterpolationType::Bezier) {
        // Bezier control points define both X and Y curves.
        // First find the X-position corresponding to input t, then evaluate Y at that point.
        // Solve for t_bezier where bezier_x(t_bezier) == t (using Newton or binary search).
        // For simplicity: use the Y curve directly with t mapped through the X control points.
        auto bezier_x = [&](double bt) -> double {
            const double ombt = 1.0 - bt;
            return ombt * ombt * ombt * 0.0
                 + 3.0 * ombt * ombt * bt * bezier.x1
                 + 3.0 * ombt * bt * bt * bezier.x2
                 + bt * bt * bt * 1.0;
        };

        // Newton-Raphson to find bt such that bezier_x(bt) == t
        double bt = t;  // initial guess
        for (int i = 0; i < 8; ++i) {
            double bx = bezier_x(bt);
            double dx = 3.0 * (1.0 - bt) * (1.0 - bt) * bezier.x1
                      + 6.0 * (1.0 - bt) * bt * (bezier.x2 - bezier.x1)
                      + 3.0 * bt * bt * (1.0 - bezier.x2);
            if (std::abs(dx) < 1e-10) break;
            bt = bt - (bx - t) / dx;
            bt = std::clamp(bt, 0.0, 1.0);
        }

        // Evaluate Y at the found bt
        const double ombt = 1.0 - bt;
        normalized = ombt * ombt * ombt * 0.0
                   + 3.0 * ombt * ombt * bt * bezier.y1
                   + 3.0 * ombt * bt * bt * bezier.y2
                   + bt * bt * bt * 1.0;
    } else {
        auto it = std::find_if(kInterpolationTable.begin(), kInterpolationTable.end(),
                               [type](const InterpolationEntry& e) { return e.type == type; });
        normalized = (it != kInterpolationTable.end() && it->func)
                     ? it->func(t)
                     : t;  // fallback linear
    }

    return start_value + (end_value - start_value) * normalized;
}

/// Convenience overload for segments that don't need Bezier control points.
inline double evaluate_segment(double t, double start_value, double end_value, InterpolationType type) {
    return evaluate_segment(t, start_value, end_value, type, {});
}

} // namespace aria::automation

#endif // ARIA_AUTOMATION_INTERPOLATION_H
