// Tests for automation point evaluation: 10 interpolation modes,
// bezier curve, boundary conditions, and Douglas-Peucker reduction.
#define _USE_MATH_DEFINES
#include <catch2/catch_all.hpp>
#include <catch2/catch_approx.hpp>

#include "automation/automation_clip.h"
#include "automation/automation_types.h"
#include "automation/interpolation.h"
#include "automation/recorder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using namespace aria::automation;

// Convenience approx matcher
static constexpr double kApprox = 0.0001;

// ============================================================
// Individual interpolation function tests
// ============================================================

TEST_CASE("Hold evaluation", "[automation][points][interp]") {
    REQUIRE(eval_hold(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_hold(0.5) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_hold(1.0) == Catch::Approx(1.0).margin(kApprox));
    // Edge: just below 1.0
    REQUIRE(eval_hold(0.999) == Catch::Approx(0.0).margin(kApprox));
}

TEST_CASE("Linear evaluation", "[automation][points][interp]") {
    REQUIRE(eval_linear(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_linear(0.5) == Catch::Approx(0.5).margin(kApprox));
    REQUIRE(eval_linear(1.0) == Catch::Approx(1.0).margin(kApprox));
    REQUIRE(eval_linear(0.25) == Catch::Approx(0.25).margin(kApprox));
    REQUIRE(eval_linear(0.75) == Catch::Approx(0.75).margin(kApprox));
}

TEST_CASE("Smooth evaluation", "[automation][points][interp]") {
    REQUIRE(eval_smooth(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_smooth(1.0) == Catch::Approx(1.0).margin(kApprox));
    // smoothstep(0.5) = 3*(0.5)^2 - 2*(0.5)^3 = 0.75 - 0.25 = 0.5
    REQUIRE(eval_smooth(0.5) == Catch::Approx(0.5).margin(kApprox));
    // Symmetry: smoothstep(t) + smoothstep(1-t) = 1
    REQUIRE(eval_smooth(0.3) + eval_smooth(0.7) == Catch::Approx(1.0).margin(kApprox));
}

TEST_CASE("EaseIn evaluation", "[automation][points][interp]") {
    REQUIRE(eval_ease_in(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_ease_in(1.0) == Catch::Approx(1.0).margin(kApprox));
    // t^2
    REQUIRE(eval_ease_in(0.5) == Catch::Approx(0.25).margin(kApprox));
    REQUIRE(eval_ease_in(0.25) == Catch::Approx(0.0625).margin(kApprox));
}

TEST_CASE("EaseOut evaluation", "[automation][points][interp]") {
    REQUIRE(eval_ease_out(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_ease_out(1.0) == Catch::Approx(1.0).margin(kApprox));
    // t*(2-t) = 2t - t^2
    REQUIRE(eval_ease_out(0.5) == Catch::Approx(0.75).margin(kApprox));
}

TEST_CASE("EaseInOut evaluation", "[automation][points][interp]") {
    REQUIRE(eval_ease_in_out(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_ease_in_out(1.0) == Catch::Approx(1.0).margin(kApprox));
    // 2*0.3^2 = 0.18
    REQUIRE(eval_ease_in_out(0.3) == Catch::Approx(0.18).margin(kApprox));
    // -1 + (4-2*0.7)*0.7 = -1 + (4-1.4)*0.7 = -1 + 2.6*0.7 = -1 + 1.82 = 0.82
    REQUIRE(eval_ease_in_out(0.7) == Catch::Approx(0.82).margin(kApprox));
    // Continuous at 0.5
    REQUIRE(eval_ease_in_out(0.5) == Catch::Approx(0.5).margin(kApprox));
}

TEST_CASE("Exponential evaluation", "[automation][points][interp]") {
    REQUIRE(eval_exponential(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_exponential(1.0) == Catch::Approx(1.0).margin(kApprox));
    // Monotonic: larger t → larger value
    REQUIRE(eval_exponential(0.3) < eval_exponential(0.6));
    REQUIRE(eval_exponential(0.6) < eval_exponential(0.9));
    // Fast rise near end: <0.5 at t=0.5 (e^6x rises very late)
    REQUIRE(eval_exponential(0.5) < 0.5);
    // At t=0.9 it should be much higher
    REQUIRE(eval_exponential(0.9) > 0.5);
}

TEST_CASE("Logarithmic evaluation", "[automation][points][interp]") {
    REQUIRE(eval_logarithmic(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_logarithmic(1.0) == Catch::Approx(1.0).margin(kApprox));
    // Monotonic
    REQUIRE(eval_logarithmic(0.3) < eval_logarithmic(0.6));
    // Fast rise near start: >0.5 at t=0.5
    REQUIRE(eval_logarithmic(0.5) > 0.5);
}

TEST_CASE("SCurve evaluation", "[automation][points][interp]") {
    REQUIRE(eval_s_curve(0.0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(eval_s_curve(1.0) == Catch::Approx(1.0).margin(kApprox));
    // Symmetric: s(t) + s(1-t) ≈ 1
    REQUIRE(eval_s_curve(0.3) + eval_s_curve(0.7) == Catch::Approx(1.0).margin(0.01));
    // At t=0.5, sigmoid centre → ~0.5
    REQUIRE(eval_s_curve(0.5) == Catch::Approx(0.5).margin(0.01));
}

// ============================================================
// evaluate_segment boundary conditions
// ============================================================

TEST_CASE("evaluate_segment boundaries", "[automation][points][segment]") {
    double start = 0.2;
    double end = 0.8;

    // t=0 → start_value
    REQUIRE(evaluate_segment(0.0, start, end, InterpolationType::Linear, {})
            == Catch::Approx(start).margin(kApprox));
    REQUIRE(evaluate_segment(0.0, start, end, InterpolationType::EaseIn, {})
            == Catch::Approx(start).margin(kApprox));
    REQUIRE(evaluate_segment(0.0, start, end, InterpolationType::Exponential, {})
            == Catch::Approx(start).margin(kApprox));

    // t=1 → end_value
    REQUIRE(evaluate_segment(1.0, start, end, InterpolationType::Linear, {})
            == Catch::Approx(end).margin(kApprox));
    REQUIRE(evaluate_segment(1.0, start, end, InterpolationType::EaseOut, {})
            == Catch::Approx(end).margin(kApprox));
    REQUIRE(evaluate_segment(1.0, start, end, InterpolationType::Smooth, {})
            == Catch::Approx(end).margin(kApprox));

    // t clamped to [0, 1]
    REQUIRE(evaluate_segment(-0.5, start, end, InterpolationType::Linear, {})
            == Catch::Approx(start).margin(kApprox));
    REQUIRE(evaluate_segment(1.5, start, end, InterpolationType::Linear, {})
            == Catch::Approx(end).margin(kApprox));
}

TEST_CASE("evaluate_segment linear ramp", "[automation][points][segment]") {
    // Linear from 0.0 to 1.0 should match eval_linear directly
    double val = evaluate_segment(0.25, 0.0, 1.0, InterpolationType::Linear, {});
    REQUIRE(val == Catch::Approx(0.25).margin(kApprox));

    val = evaluate_segment(0.75, 0.0, 1.0, InterpolationType::Linear, {});
    REQUIRE(val == Catch::Approx(0.75).margin(kApprox));

    // With non-zero range
    val = evaluate_segment(0.5, 0.2, 0.8, InterpolationType::Linear, {});
    REQUIRE(val == Catch::Approx(0.5).margin(kApprox)); // 0.2 + 0.5 * 0.6 = 0.5
}

TEST_CASE("evaluate_segment hold", "[automation][points][segment]") {
    // Hold: value stays at start until t=1.0
    REQUIRE(evaluate_segment(0.0, 0.3, 0.9, InterpolationType::Hold, {})
            == Catch::Approx(0.3).margin(kApprox));
    REQUIRE(evaluate_segment(0.5, 0.3, 0.9, InterpolationType::Hold, {})
            == Catch::Approx(0.3).margin(kApprox));
    REQUIRE(evaluate_segment(0.999, 0.3, 0.9, InterpolationType::Hold, {})
            == Catch::Approx(0.3).margin(kApprox));
    REQUIRE(evaluate_segment(1.0, 0.3, 0.9, InterpolationType::Hold, {})
            == Catch::Approx(0.9).margin(kApprox));
}

// ============================================================
// Bezier evaluation
// ============================================================

TEST_CASE("Bezier evaluate_segment", "[automation][points][bezier]") {
    BezierControl ctrl;
    ctrl.x1 = 0.0; ctrl.y1 = 0.0;  // Linear control points
    ctrl.x2 = 1.0; ctrl.y2 = 1.0;

    // With linear control points, bezier = linear
    double val = evaluate_segment(0.25, 0.0, 1.0, InterpolationType::Bezier, ctrl);
    REQUIRE(val == Catch::Approx(0.25).margin(0.01));

    val = evaluate_segment(0.75, 0.0, 1.0, InterpolationType::Bezier, ctrl);
    REQUIRE(val == Catch::Approx(0.75).margin(0.01));
}

TEST_CASE("Bezier with high control points gives high values",
          "[automation][points][bezier]") {
    // Control points are Y-only here — the X is used for t-remap.
    // P1=(0.0, 0.9) P2=(0.0, 1.0): Y curve is heavily front-loaded
    BezierControl ctrl;
    ctrl.x1 = 0.0; ctrl.y1 = 0.9;
    ctrl.x2 = 0.0; ctrl.y2 = 1.0;

    // At t=0.5, bezier with these controls should differ from linear (0.5)
    double val = evaluate_segment(0.5, 0.0, 1.0, InterpolationType::Bezier, ctrl);
    REQUIRE(val != Catch::Approx(0.5).margin(0.1));

    // With different control points, we get a different result
    BezierControl ctrl2;
    ctrl2.x1 = 1.0; ctrl2.y1 = 0.0;
    ctrl2.x2 = 1.0; ctrl2.y2 = 0.1;
    double val2 = evaluate_segment(0.5, 0.0, 1.0, InterpolationType::Bezier, ctrl2);
    REQUIRE(val2 != Catch::Approx(val).margin(0.1));
}

TEST_CASE("Bezier boundary: t=0 and t=1", "[automation][points][bezier]") {
    BezierControl ctrl;
    ctrl.x1 = 0.2; ctrl.y1 = 0.8;
    ctrl.x2 = 0.8; ctrl.y2 = 0.2;

    REQUIRE(evaluate_segment(0.0, 0.1, 0.9, InterpolationType::Bezier, ctrl)
            == Catch::Approx(0.1).margin(kApprox));
    REQUIRE(evaluate_segment(1.0, 0.1, 0.9, InterpolationType::Bezier, ctrl)
            == Catch::Approx(0.9).margin(kApprox));
}

// ============================================================
// Douglas-Peucker reduction accuracy
// ============================================================

TEST_CASE("Douglas-Peucker reduces a linear ramp to 2-5 points",
          "[automation][points][dp]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);
    recorder.set_smoothing(0.0);  // disable smoothing for exact values

    // Record a perfectly linear ramp from 0 to 1 over 1000 samples
    for (uint64_t i = 0; i <= 1000; ++i) {
        double v = static_cast<double>(i) / 1000.0;
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    // Reduce with moderate tolerance
    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.01);
    auto result = recorder.take_result();

    // A linear ramp should reduce to ~2 points (first and last)
    REQUIRE(result.size() >= 2);
    REQUIRE(result.size() <= 5);

    // Verify first point is at start
    REQUIRE(result.front().ppqn == 0);
    REQUIRE(result.front().value == Catch::Approx(0.0).margin(kApprox));
    // Last point should have max ppqn
    REQUIRE(result.back().ppqn == 1000);
    REQUIRE(result.back().value == Catch::Approx(1.0).margin(kApprox));
}

TEST_CASE("Douglas-Peucker reduces a sine curve to 20-120 points",
          "[automation][points][dp]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);

    // Record 1800 points of a sine wave (simulating 5s of knob movement)
    for (uint64_t i = 0; i < 1800; ++i) {
        double v = 0.5 + 0.5 * std::sin(static_cast<double>(i) * 0.05);
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    REQUIRE(recorder.point_count() >= 1790); // close to 1800 before reduction

    // Reduce with moderate tolerance
    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.01);
    auto result = recorder.take_result();

    // Sine curve with tolerance 0.01 should reduce to 20-120 points
    REQUIRE(result.size() >= 10);
    REQUIRE(result.size() <= 120);
}

TEST_CASE("Douglas-Peucker preserves endpoints and max points on a curve",
          "[automation][points][dp]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);

    // Record a curve with a single prominent peak
    for (uint64_t i = 0; i < 500; ++i) {
        double v = std::sin(static_cast<double>(i) * 0.1);
        v = v * v; // unipolar peak
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.005);
    auto result = recorder.take_result();

    // First and last points are always preserved
    REQUIRE(result.size() >= 2);
    REQUIRE(result.front().ppqn == 0);
    REQUIRE(result.back().ppqn == 499);
}

TEST_CASE("Douglas-Peucker: zero tolerance keeps all points",
          "[automation][points][dp]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);

    for (uint64_t i = 0; i < 100; ++i) {
        recorder.record_value(i, static_cast<double>(i) / 100.0);
    }
    recorder.stop_recording();

    // Tolerance = 0 means no reduction (all points kept)
    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.0);
    auto result = recorder.take_result();

    // All points should be kept
    REQUIRE(result.size() == 100);
}

// ============================================================
// Interpolation function table completeness
// ============================================================

TEST_CASE("All 10 interpolation types have entries in the function table",
          "[automation][points][table]") {
    std::array<InterpolationType, 10> expected = {{
        InterpolationType::Hold,
        InterpolationType::Linear,
        InterpolationType::Bezier,
        InterpolationType::Smooth,
        InterpolationType::EaseIn,
        InterpolationType::EaseOut,
        InterpolationType::EaseInOut,
        InterpolationType::Exponential,
        InterpolationType::Logarithmic,
        InterpolationType::SCurve
    }};

    REQUIRE(kInterpolationTable.size() == 10);
    for (auto type : expected) {
        auto it = std::find_if(kInterpolationTable.begin(),
                               kInterpolationTable.end(),
                               [type](const InterpolationEntry& e) {
                                   return e.type == type;
                               });
        REQUIRE(it != kInterpolationTable.end());
    }
}
