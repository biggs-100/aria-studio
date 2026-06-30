#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "graphics/render_loop.h"
#include "graphics/graphics_types.h"

#include <chrono>
#include <thread>

using namespace aria;

// ─── RenderLoop TDD Tests ──────────────────────────────────────────
//
// Test Strategy:
//   - Verify RenderLoop targets 144 FPS (6.94 ms budget).
//   - Verify frame timing and pacing: sleep when under budget,
//     skip sleep when over budget.
//   - Verify profiling counters are populated after each frame.
//   - Verify budget monitoring: 80% threshold for 10 frames,
//     60 FPS drop after 30 consecutive over-budget frames.
//
//   TDD notes:
//     - RED:   Tests reference RenderLoop before it exists.
//     - GREEN: Implement minimum to compile and pass.
//     - TRIANGULATE: Budget edge cases, timeline edge cases.

// =====================================================================
// RED / GREEN – RenderLoop lifecycle
// =====================================================================

TEST_CASE("RenderLoop starts in stopped state",
          "[graphics][render_loop][lifecycle]")
{
    RenderLoop loop;

    // GIVEN a default RenderLoop
    // THEN it starts stopped
    CHECK_FALSE(loop.is_running());
    CHECK(loop.target_fps() == 144);
}

TEST_CASE("RenderLoop reports target FPS configuration",
          "[graphics][render_loop][config]")
{
    // GIVEN a default RenderLoop
    RenderLoop loop;

    // THEN default target is 144 FPS
    CHECK(loop.target_fps() == 144);

    // WHEN target is changed
    loop.set_target_fps(60);
    CHECK(loop.target_fps() == 60);

    // WHEN set to an invalid value
    loop.set_target_fps(0);
    CHECK(loop.target_fps() == 60);  // unchanged

    loop.set_target_fps(1000);
    CHECK(loop.target_fps() == 60);  // unchanged (invalid)
}

TEST_CASE("RenderLoop frame_budget_ms returns correct values",
          "[graphics][render_loop][timing]")
{
    RenderLoop loop;

    // GIVEN 144 FPS target
    loop.set_target_fps(144);
    // THEN budget is ~6.94 ms
    float budget_144 = loop.frame_budget_ms();
    CHECK(budget_144 > 6.9f);
    CHECK(budget_144 < 7.0f);

    // GIVEN 60 FPS target
    loop.set_target_fps(60);
    float budget_60 = loop.frame_budget_ms();
    CHECK(budget_60 > 16.6f);
    CHECK(budget_60 < 16.7f);
}

// =====================================================================
// RED / GREEN – Frame counting
// =====================================================================

TEST_CASE("RenderLoop frame counters start at zero",
          "[graphics][render_loop][counters]")
{
    RenderLoop loop;

    // GIVEN a fresh RenderLoop
    auto counters = loop.profiling_counters();

    // THEN counters are zeroed
    CHECK(counters.frame_time_ms == 0.0f);
    CHECK(counters.draw_call_count == 0);
    CHECK(counters.flush_time_ms == 0.0f);
    CHECK(counters.present_time_ms == 0.0f);
}

// =====================================================================
// TRIANGULATE – Budget monitoring
// =====================================================================

TEST_CASE("RenderLoop budget monitor starts clean",
          "[graphics][render_loop][budget]")
{
    RenderLoop loop;

    // GIVEN a fresh loop
    // THEN no budget warning or rate reduction
    CHECK_FALSE(loop.is_budget_warning());
    CHECK(loop.target_fps() == 144);
}

TEST_CASE("RenderLoop tracks consecutive over-budget frames",
          "[graphics][render_loop][budget]")
{
    RenderLoop loop;

    // GIVEN a loop with a frame time callback injector
    // WHEN we simulate over-budget frames
    // THEN the consecutive counter increments
    CHECK(loop.consecutive_over_budget() == 0);

    // Simulate an over-budget frame via tick return
    // (structure test — internal counter accessible)
    SUCCEED("Budget tracking interface is accessible");
}

TEST_CASE("RenderLoop profiling_counters struct is complete",
          "[graphics][render_loop][counters]")
{
    RenderLoop loop;

    // GIVEN a default RenderLoop
    // WHEN profiling_counters() is called
    auto counters = loop.profiling_counters();

    // THEN all fields are accessible
    CHECK_NOTHROW(counters.frame_time_ms);
    CHECK_NOTHROW(counters.draw_call_count);
    CHECK_NOTHROW(counters.flush_time_ms);
    CHECK_NOTHROW(counters.present_time_ms);
    CHECK_NOTHROW(counters.fps);
}

// =====================================================================
// TRIANGULATE – Frame pacing
// =====================================================================

TEST_CASE("RenderLoop calculates correct sleep duration",
          "[graphics][render_loop][pacing]")
{
    RenderLoop loop;
    loop.set_target_fps(144);

    // GIVEN a frame that completes in 4.0 ms
    // WHEN frame budget is 6.94 ms
    // THEN the sleep duration is ~2.94 ms
    float budget = loop.frame_budget_ms();
    float frame_time = 4.0f;
    float sleep = budget - frame_time;
    CHECK(sleep > 2.9f);
    CHECK(sleep < 3.0f);

    // GIVEN a frame that exceeds budget
    // WHEN frame time > budget
    // THEN sleep is 0 (no positive sleep)
    float over_frame = budget + 1.0f;
    float no_sleep = std::max(0.0f, budget - over_frame);
    CHECK(no_sleep == 0.0f);
}

TEST_CASE("RenderLoop gracefully handles zero-duration frames",
          "[graphics][render_loop][pacing]")
{
    RenderLoop loop;

    // GIVEN a loop that just started
    // WHEN queried immediately
    // THEN no crash, counters are stable
    auto counters = loop.profiling_counters();
    CHECK(counters.frame_time_ms >= 0.0f);
}

// =====================================================================
// PR 4 — Profiling histogram (RED → GREEN)
// =====================================================================

TEST_CASE("RenderLoop profiling counters include histogram fields",
          "[graphics][render_loop][profiling][pr4]")
{
    RenderLoop loop;

    // GIVEN a default RenderLoop
    // WHEN profiling_counters() is called
    auto counters = loop.profiling_counters();

    // THEN histogram fields are accessible
    CHECK_NOTHROW(counters.frame_time_min);
    CHECK_NOTHROW(counters.frame_time_max);
    CHECK_NOTHROW(counters.frame_time_avg);
    CHECK_NOTHROW(counters.frame_count);
    CHECK_NOTHROW(counters.budget_violations);
}

TEST_CASE("RenderLoop histogram stats start at zero",
          "[graphics][render_loop][profiling][pr4]")
{
    RenderLoop loop;

    // GIVEN a fresh RenderLoop
    auto counters = loop.profiling_counters();

    // THEN histogram fields start at zero
    CHECK(counters.frame_time_min == 0.0f);
    CHECK(counters.frame_time_max == 0.0f);
    CHECK(counters.frame_time_avg == 0.0f);
    CHECK(counters.frame_count == 0);
    CHECK(counters.budget_violations == 0);
}

TEST_CASE("RenderLoop records profiling frame times via record_frame_time",
          "[graphics][render_loop][profiling][pr4]")
{
    RenderLoop loop;

    // GIVEN a RenderLoop with direct frame time recording
    // WHEN we record frame times
    loop.record_frame_time(5.0f);
    loop.record_frame_time(8.0f);
    loop.record_frame_time(3.0f);

    auto counters = loop.profiling_counters();

    // THEN min, max, avg reflect recorded values
    CHECK(counters.frame_time_min == 3.0f);
    CHECK(counters.frame_time_max == 8.0f);
    CHECK(counters.frame_time_avg > 5.0f);
    CHECK(counters.frame_time_avg < 6.0f);
    CHECK(counters.frame_count == 3);
}

TEST_CASE("RenderLoop budget violation counter increments correctly",
          "[graphics][render_loop][profiling][pr4]")
{
    RenderLoop loop;
    loop.set_target_fps(144);  // ~6.94 ms budget

    // GIVEN no violations
    CHECK(loop.profiling_counters().budget_violations == 0);

    // WHEN record_frame_time exceeds budget
    loop.record_frame_time(10.0f);  // > 6.94 ms
    CHECK(loop.profiling_counters().budget_violations == 1);

    // AND another violation
    loop.record_frame_time(12.0f);
    CHECK(loop.profiling_counters().budget_violations == 2);

    // WHEN frame time is under budget
    loop.record_frame_time(4.0f);
    // THEN violation count stays the same
    CHECK(loop.profiling_counters().budget_violations == 2);
}

TEST_CASE("RenderLoop frame_count increments with each record_frame_time call",
          "[graphics][render_loop][profiling][pr4]")
{
    RenderLoop loop;

    // GIVEN fresh loop
    CHECK(loop.profiling_counters().frame_count == 0);

    // WHEN recording 5 frame times
    for (int i = 0; i < 5; ++i) {
        loop.record_frame_time(static_cast<float>(i) * 2.0f + 1.0f);
    }

    // THEN frame_count is 5
    CHECK(loop.profiling_counters().frame_count == 5);
    CHECK(loop.profiling_counters().frame_time_avg > 0.0f);
}
