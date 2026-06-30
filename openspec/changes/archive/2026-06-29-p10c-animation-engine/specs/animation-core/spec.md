# Animation Core Specification

## Purpose

Provide the foundational animation primitives: a comprehensive easing math library, a portable `Animation` descriptor, and an `Animator` that manages the lifecycle of active animations.

## Requirements

### Requirement: Easing Functions

The system SHALL provide a header-only `easing.h` implementing 30+ easing curves as constexpr pure functions taking `float t` in [0, 1] and returning the eased value. Curves SHALL cover the standard CSS set (linear, ease, ease-in, ease-out, ease-in-out, sine, quad, cubic, quart, quint, expo, circ, back, elastic, bounce, each with in/out/in-out variants where applicable).

#### Scenario: Easing maps 0.0 to 0.0 and 1.0 to 1.0

- GIVEN any easing function from the library
- WHEN called with `t = 0.0`
- THEN the result SHALL be 0.0
- WHEN called with `t = 1.0`
- THEN the result SHALL be 1.0

#### Scenario: Easing produces monotonic output for in variants

- GIVEN `ease_in_cubic(t)`
- WHEN called with `t1 = 0.25` and `t2 = 0.5`
- THEN `ease_in_cubic(t1) <= ease_in_cubic(t2)` (monotonic non-decreasing)

#### Scenario: Out-back overshoots then returns

- GIVEN `ease_out_back(t)`
- WHEN called with `t` values across [0, 1]
- THEN `ease_out_back(1.0)` returns 1.0
- AND `ease_out_back(t)` exceeds 1.0 for some `t < 1.0`

### Requirement: Animation Descriptor

The system SHALL define an `Animation` struct with fields: `target_id` (WidgetID), `property` (enum: kOpacity, kX, kY, kWidth, kHeight, kRotation, kScaleX, kScaleY), `from`/`to` (float), `duration` (float seconds), `easing` (EasingCurve enum), `start_time` (steady_clock::time_point), and `on_complete` (std::function<void()>). The struct SHALL be movable but not copyable.

#### Scenario: Animation created with valid parameters

- GIVEN a WidgetID and a target property
- WHEN an `Animation` is constructed with duration 0.3s, easing `kEaseOutCubic`, and an onComplete lambda
- THEN `animation.target_id` matches the WidgetID
- AND `animation.duration` is 0.3f
- AND `animation.easing` is `kEaseOutCubic`
- AND `animation.on_complete` is callable

#### Scenario: Animation move transfers ownership

- GIVEN an Animation `a` with an onComplete lambda
- WHEN `Animation b = std::move(a)`
- THEN `b.on_complete` is callable
- AND `a.on_complete` is empty

### Requirement: Animator Lifecycle

The `Animator` class SHALL maintain a list of active animations. `Animator::start(Animation)` SHALL add an animation to the active list. `Animator::tick(float dt)` SHALL advance all active animations by `dt` seconds, interpolate the animated value using the easing curve, invoke `on_complete` when an animation reaches t >= 1.0, and remove completed animations. `Animator::cancel(WidgetID)` SHALL remove all animations for a given widget. `Animator::is_animating(WidgetID)` SHALL return whether the widget has active animations.

#### Scenario: Tick advances animation and fires completion

- GIVEN an Animator with one animation of duration 0.5s
- WHEN `tick(0.5)` is called
- THEN the animation SHALL be removed from the active list
- AND `on_complete` SHALL have been invoked

#### Scenario: Tick partial step produces interpolated value

- GIVEN an Animator with one animation: from=0, to=100, duration=1.0s, easing=linear
- WHEN `tick(0.5)` is called
- THEN the animation SHALL remain active
- AND the animated value SHALL be 50.0 (linear interpolation at t=0.5)

#### Scenario: Cancel removes all animations for a widget

- GIVEN an Animator with two animations for widget ID 42
- WHEN `cancel(42)` is called
- THEN `is_animating(42)` returns false
- AND the active animation count decreases by 2

#### Scenario: Tick with zero dt is a no-op

- GIVEN an Animator with one active animation
- WHEN `tick(0.0)` is called
- THEN the animation SHALL remain at t=0
- AND `on_complete` SHALL NOT be invoked
