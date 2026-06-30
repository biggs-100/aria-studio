# Animation Render Integration Specification

## Purpose

Integrate the Animator into the existing RenderLoop and DirtyTracker so that animated widget properties are updated each frame and the rendering system correctly re-paints only affected widgets.

## Requirements

### Requirement: RenderLoop Ticks Animator Each Frame

The RenderLoop SHALL call `Animator::tick(dt)` once per frame using the elapsed frame time, immediately before the paint pass and after layout has been computed. The frame sequence SHALL be: process input → update state → **tick animations** → layout → paint → present.

#### Scenario: Active animation advances each frame

- GIVEN a running RenderLoop and an Animator with one active 1.0s animation
- WHEN the loop runs for 10 frames at ~16 ms each
- THEN `Animator::is_animating(target_id)` returns true after 5 frames
- AND `Animator::is_animating(target_id)` returns false after 63+ frames (t >= 1.0)

#### Scenario: Animation tick runs regardless of frame budget

- GIVEN a frame that exceeds its 144 FPS budget
- WHEN the RenderLoop processes the frame
- THEN `Animator::tick()` SHALL still be called before paint
- AND the animation advances by the actual elapsed time, not the target budget

#### Scenario: No active animations is a fast no-op

- GIVEN a RenderLoop with no active animations
- WHEN a frame is processed
- THEN `Animator::tick()` SHALL complete in negligible time (< 1 µs overhead)
- AND no widgets SHALL be marked dirty

### Requirement: Completed Animations Mark Widgets Dirty

After `Animator::tick()` completes, the system SHALL collect the set of widgets whose animations finished (t >= 1.0) and mark them as dirty via the existing `DirtyTracker`. This ensures the paint pass re-renders those widgets at their final interpolated values.

#### Scenario: Animation completion triggers dirty mark

- GIVEN a widget W with an active opacity animation
- WHEN the animation reaches t >= 1.0 in `Animator::tick()`
- THEN W SHALL be marked dirty in DirtyTracker
- AND W SHALL be re-painted on the current frame's paint pass

#### Scenario: Mid-animation does NOT mark dirty

- GIVEN a widget W with an active animation at t = 0.5
- WHEN `Animator::tick()` runs
- THEN W SHALL NOT be marked dirty by the completion logic
- (W may still be dirty if the animation implementation updates the property via `set_animated_value()` which independently marks dirty)

#### Scenario: Multiple completions mark all affected widgets

- GIVEN three widgets A, B, C with animations that all complete in the same tick
- WHEN `Animator::tick()` runs
- THEN all three SHALL be marked dirty for the next paint pass
