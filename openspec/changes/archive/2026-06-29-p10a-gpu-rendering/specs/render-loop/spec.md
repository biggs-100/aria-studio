# Render Loop Specification

## Purpose

Drive frame timing at a target of 144 FPS in `Application::run()`, synchronize with vsync, present frames, and expose profiling counters for frame budget monitoring.

## Requirements

### Requirement: Frame Timing

The render loop SHALL target 144 FPS (6.94 ms per frame). It SHALL use `std::chrono::steady_clock` for high-resolution timing. If the GPU completes faster than the budget, the loop SHALL sleep to maintain precise frame pacing.

#### Scenario: Frame completes within budget

- GIVEN a 144 FPS target (6.94 ms budget)
- WHEN frame work completes in 4.0 ms
- THEN the loop sleeps ~2.94 ms before starting the next frame
- AND `present()` is called at the vsync boundary

#### Scenario: Frame exceeds budget

- GIVEN a 144 FPS target
- WHEN frame work takes 9 ms (over budget)
- THEN the loop presents immediately without sleep
- AND a `kFrameBudgetExceeded` profiling event is emitted

### Requirement: Vsync and Present Cycle

The loop MUST call `wgpu::SwapChain::Present()` once per frame. Presentation SHALL be synchronized with vsync when the display supports it. The loop sequence SHALL be: process input → update state → layout → paint → present.

#### Scenario: Normal present cycle

- GIVEN an active render loop
- WHEN one frame cycle runs
- THEN input events are dispatched
- AND layout is recomputed
- AND the scene is rendered to the swap chain
- AND `Present()` is called once

### Requirement: Profiling Counters

The loop SHALL expose per-frame counters: total frame time (ms), GPU draw call count, Skia flush time (ms), and present time (ms). Counters SHALL be available via `RenderLoop::profiling_counters()`.

#### Scenario: Profiling data collected per frame

- GIVEN a running render loop
- WHEN frame N completes
- THEN `profiling_counters()` returns a struct with frame_time_ms, draw_call_count, flush_time_ms, present_time_ms
- AND counters are valid for the just-completed frame

### Requirement: Frame Budget Monitoring

The loop SHOULD emit a warning when cumulative frame time exceeds 80% of the budget for 10 consecutive frames. The loop MAY drop to 60 FPS target if over-budget for more than 30 consecutive frames.

#### Scenario: Frame rate drops to 60 FPS under sustained load

- GIVEN 35 consecutive frames over budget
- WHEN the 31st frame exceeds budget
- THEN the target frame rate drops to 60 FPS (16.67 ms budget)
- AND a `kFrameRateReduced` event is emitted
