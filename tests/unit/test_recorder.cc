// Tests for automation recorder: ring buffer wrap, smoothing,
// D-P reduction modes (1800 → 20-50 points), ReduceOnStop vs Adaptive.
#define _USE_MATH_DEFINES
#include <catch2/catch_all.hpp>
#include <catch2/catch_approx.hpp>

#include "automation/automation_types.h"
#include "automation/recorder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace aria::automation;

static constexpr double kApprox = 0.0001;

// ============================================================
// Recorder lifecycle
// ============================================================

TEST_CASE("AutomationRecorder: starts not recording",
          "[automation][recorder]") {
    AutomationRecorder recorder;
    REQUIRE_FALSE(recorder.is_recording());
    REQUIRE(recorder.point_count() == 0);
}

TEST_CASE("AutomationRecorder: start/stop recording cycle",
          "[automation][recorder]") {
    AutomationRecorder recorder;

    recorder.start_recording(0, 48000.0);
    REQUIRE(recorder.is_recording());

    recorder.record_value(0, 0.5);
    REQUIRE(recorder.point_count() == 1);

    recorder.stop_recording();
    REQUIRE_FALSE(recorder.is_recording());
}

TEST_CASE("AutomationRecorder: recorded values are retrievable via take_result",
          "[automation][recorder]") {
    AutomationRecorder recorder;

    recorder.start_recording(0, 48000.0);
    recorder.set_smoothing(0.0);  // disable smoothing
    recorder.record_value(0, 0.25);
    recorder.record_value(480, 0.5);
    recorder.record_value(960, 0.75);
    recorder.stop_recording();

    auto result = recorder.take_result();
    REQUIRE(result.size() == 3);
    REQUIRE(result[0].value == Catch::Approx(0.25).margin(kApprox));
    REQUIRE(result[1].value == Catch::Approx(0.5).margin(kApprox));
    REQUIRE(result[2].value == Catch::Approx(0.75).margin(kApprox));

    REQUIRE(recorder.point_count() == 0);
}

// ============================================================
// Ring buffer wrap
// ============================================================

TEST_CASE("AutomationRecorder: ring buffer wraps when capacity exceeded",
          "[automation][recorder][wrap]") {
    AutomationRecorder recorder(100);

    recorder.start_recording(0, 48000.0);

    for (uint64_t i = 0; i < 150; ++i) {
        recorder.record_value(i, 0.5);
    }
    recorder.stop_recording();

    REQUIRE(recorder.point_count() > 0);
    REQUIRE(recorder.point_count() <= 100);

    auto result = recorder.take_result();
    REQUIRE(result.size() > 0);
    REQUIRE(result.size() <= 100);
}

TEST_CASE("AutomationRecorder: wrap preserves most recent data",
          "[automation][recorder][wrap]") {
    AutomationRecorder recorder(50);

    recorder.start_recording(0, 48000.0);
    recorder.set_smoothing(0.0);  // disable smoothing

    for (uint64_t i = 0; i < 100; ++i) {
        recorder.record_value(i, static_cast<double>(i) / 100.0);
    }
    recorder.stop_recording();

    auto result = recorder.take_result();

    REQUIRE(result.size() > 0);
    REQUIRE(result.size() <= 50);

    // The buffer wraps at 50 (capacity), erasing oldest 12, then continues
    // The last few values should include near-max PPQN entries
    REQUIRE(result.back().ppqn > 80);
}

// ============================================================
// Smoothing
// ============================================================

TEST_CASE("AutomationRecorder: smoothing enabled produces different output",
          "[automation][recorder][smoothing]") {
    AutomationRecorder recorder(1000);

    recorder.start_recording(0, 48000.0);
    recorder.set_smoothing(0.0);

    recorder.record_value(0, 0.0);
    recorder.record_value(1, 1.0);
    recorder.record_value(2, 1.0);
    recorder.stop_recording();

    auto unsmoothed = recorder.take_result();
    REQUIRE(unsmoothed.size() == 3);
    REQUIRE(unsmoothed[1].value == Catch::Approx(1.0).margin(kApprox));

    recorder.start_recording(0, 48000.0);
    recorder.set_smoothing(10.0);

    recorder.record_value(0, 0.0);
    recorder.record_value(1, 1.0);
    recorder.record_value(2, 1.0);
    recorder.stop_recording();

    auto smoothed = recorder.take_result();
    REQUIRE(smoothed.size() >= 3);
    REQUIRE(smoothed[1].value < 1.0);
}

TEST_CASE("AutomationRecorder: zero smoothing is passthrough",
          "[automation][recorder][smoothing]") {
    AutomationRecorder recorder(1000);

    recorder.start_recording(0, 48000.0);
    recorder.set_smoothing(0.0);

    recorder.record_value(0, 0.42);
    recorder.stop_recording();

    auto result = recorder.take_result();
    REQUIRE(result[0].value == Catch::Approx(0.42).margin(kApprox));
}

// ============================================================
// Douglas-Peucker reduction
// ============================================================

TEST_CASE("AutomationRecorder: DP reduces 1800 sine points significantly",
          "[automation][recorder][dp]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);

    for (uint64_t i = 0; i < 1800; ++i) {
        double t = static_cast<double>(i) / 1800.0;
        double v = 0.5
                 + 0.3 * std::sin(t * 12.0 * M_PI)
                 + 0.2 * std::sin(t * 5.0 * M_PI + 1.0)
                 + 0.1 * std::cos(t * 20.0 * M_PI);
        v = std::clamp(v, 0.0, 1.0);
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    REQUIRE(recorder.point_count() >= 1700);

    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.005);
    auto result = recorder.take_result();

    REQUIRE(result.size() >= 10);
    REQUIRE(result.size() <= 150);
    REQUIRE(result.front().ppqn == 0);
}

TEST_CASE("AutomationRecorder: tighter tolerance preserves more points",
          "[automation][recorder][dp]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);

    for (uint64_t i = 0; i < 500; ++i) {
        double v = 0.5 + 0.4 * std::sin(static_cast<double>(i) * 0.05);
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.000001);
    auto result = recorder.take_result();
    REQUIRE(result.size() > 400);
}

TEST_CASE("AutomationRecorder: loose tolerance reduces aggressively",
          "[automation][recorder][dp]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);

    for (uint64_t i = 0; i < 500; ++i) {
        double v = 0.5 + 0.4 * std::sin(static_cast<double>(i) * 0.05);
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 1.0);
    auto result = recorder.take_result();
    REQUIRE(result.size() <= 10);
}

// ============================================================
// Reduction modes: ReduceOnStop vs Adaptive
// ============================================================

TEST_CASE("AutomationRecorder: ReduceOnStop mode reduces after stop",
          "[automation][recorder][mode]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);
    for (uint64_t i = 0; i < 200; ++i) {
        double v = static_cast<double>(i) / 200.0;
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    size_t before = recorder.point_count();
    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.01);
    size_t after = recorder.take_result().size();

    REQUIRE(after <= before);
    REQUIRE(after >= 2);
}

TEST_CASE("AutomationRecorder: Adaptive mode also reduces effectively",
          "[automation][recorder][mode]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);
    for (uint64_t i = 0; i < 500; ++i) {
        double v = 0.5 + 0.4 * std::sin(static_cast<double>(i) * 0.03);
        recorder.record_value(i, v);
    }

    recorder.reduce_points(AutomationRecorder::ReductionMode::Adaptive, 0.01);
    recorder.stop_recording();

    auto result = recorder.take_result();
    REQUIRE(result.size() >= 2);
    REQUIRE(result.size() < 200);
}

TEST_CASE("AutomationRecorder: Manual mode requires explicit reduce",
          "[automation][recorder][mode]") {
    AutomationRecorder recorder(10000);

    recorder.start_recording(0, 48000.0);
    for (uint64_t i = 0; i < 100; ++i) {
        double v = static_cast<double>(i) / 100.0;
        recorder.record_value(i, v);
    }
    recorder.stop_recording();

    REQUIRE(recorder.point_count() == 100);

    recorder.reduce_points(AutomationRecorder::ReductionMode::Manual, 0.01);
    auto result = recorder.take_result();
    REQUIRE(result.size() <= 10);
}

// ============================================================
// Edge cases
// ============================================================

TEST_CASE("AutomationRecorder: record before start is ignored",
          "[automation][recorder][edge]") {
    AutomationRecorder recorder;

    recorder.record_value(0, 0.5);
    REQUIRE(recorder.point_count() == 0);
}

TEST_CASE("AutomationRecorder: consecutive start clears buffer",
          "[automation][recorder][edge]") {
    AutomationRecorder recorder;

    recorder.start_recording(0, 48000.0);
    recorder.record_value(0, 0.5);
    recorder.record_value(480, 0.8);
    recorder.stop_recording();
    REQUIRE(recorder.point_count() == 2);

    recorder.start_recording(0, 48000.0);
    REQUIRE(recorder.point_count() == 0);
}

TEST_CASE("AutomationRecorder: take_result after empty recording returns empty",
          "[automation][recorder][edge]") {
    AutomationRecorder recorder;

    recorder.start_recording(0, 48000.0);
    recorder.stop_recording();

    auto result = recorder.take_result();
    REQUIRE(result.empty());
}

TEST_CASE("AutomationRecorder: single point recording reduces to 1 point",
          "[automation][recorder][edge]") {
    AutomationRecorder recorder(100);

    recorder.start_recording(0, 48000.0);
    recorder.record_value(480, 0.75);
    recorder.stop_recording();

    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.01);
    auto result = recorder.take_result();
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].value == Catch::Approx(0.75).margin(kApprox));
}

TEST_CASE("AutomationRecorder: two-point recording stays at 2 points",
          "[automation][recorder][edge]") {
    AutomationRecorder recorder(100);

    recorder.start_recording(0, 48000.0);
    recorder.record_value(0, 0.0);
    recorder.record_value(960, 1.0);
    recorder.stop_recording();

    recorder.reduce_points(AutomationRecorder::ReductionMode::ReduceOnStop, 0.01);
    auto result = recorder.take_result();
    REQUIRE(result.size() == 2);
}
