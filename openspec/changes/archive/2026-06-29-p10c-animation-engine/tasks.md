# Tasks: P10c Animation Engine

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 180–260 |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | single-pr |
| Chain strategy | pending |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

## Phase 1: Easing Math Library

- [x] 1.1 Create `src/graphics/easing.h` with 30+ constexpr easing functions — linear, quad, cubic, quart, quint, sine, expo, circ, elastic, back, bounce (× in/out/inOut)
- [x] 1.2 Create `src/graphics/animation_types.h` with `AnimatableProperty` enum, `EasingCurve` enum, and movable `Animation` struct

## Phase 2: Animator Core

- [x] 2.1 Create `src/graphics/animator.h`/`.cc` with `Animator` class — `start()`, `tick(dt)`, `cancel()`, `is_animating()`, and `completed()` accessor

## Phase 3: RenderLoop Integration

- [x] 3.1 Add `set_animator()` setter to `src/graphics/render_loop.h`
- [x] 3.2 Modify `src/graphics/render_loop.cc` — call `animator_->tick(dt)` before paint pass, mark completed widgets dirty via `DirtyTracker`
- [x] 3.3 Add `set_animated_property()` and `animated_value()` to `src/graphics/widget.h` for interpolation target storage

## Phase 4: Testing

- [x] 4.1 Unit test: verify each easing function returns 0.0 at t=0 and 1.0 at t=1
- [x] 4.2 Unit test: verify monotonicity for in-variants across [0, 1]
- [x] 4.3 Unit test: `Animator::tick()` advances animation, fires on_complete, removes from active list
- [x] 4.4 Unit test: `Animator::cancel()` removes all animations for a given WidgetID
- [x] 4.5 Integration test: `RenderLoop::tick()` calls `Animator::tick()` before paint pass
