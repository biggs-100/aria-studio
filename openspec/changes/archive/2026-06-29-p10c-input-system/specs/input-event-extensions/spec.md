# Input Event Extensions Specification

## Purpose

Extend the existing EventDispatcher with touch and pen event types, and add accessibility metadata fields to the Widget base class — enabling non-mouse input and assistive technology integration.

## Requirements

### Requirement: Touch and Pen Event Dispatch

The `EventDispatcher` SHALL support `kTouchDown`, `kTouchMove`, `kTouchUp`, `kPenDown`, `kPenMove`, and `kPenUp` event types. Touch events SHALL carry `touch_id` (multi-touch tracking), `x`, `y` in logical pixels. Pen events SHALL carry `x`, `y`, `pressure` (0.0–1.0), `tilt_x`/`tilt_y` (degrees), and `barrel_button` (bool). Both SHALL be dispatched through the same hit-test pipeline as mouse events. A convenience flag `is_touch_or_pen` SHALL be available on the base `Event` struct.

#### Scenario: Touch down dispatches to hit widget

- GIVEN a touch at logical pixel (300, 200) over widget A
- WHEN `dispatch_touch_down(300, 200, touch_id=0)` is called
- THEN widget A receives `kTouchDown`
- AND `event.touch_id` is `0`
- AND `event.x` is `300`, `event.y` is `200`

#### Scenario: Multi-touch tracks separate IDs

- GIVEN active touches with IDs 0 and 1
- WHEN touch 0 moves to (310, 210) and touch 1 moves to (100, 50)
- THEN each touch dispatches `kTouchMove` with its own touch_id and coordinates
- AND events are independent (no mixing of coordinates)

#### Scenario: Pen event carries pressure and tilt

- GIVEN a pen at (400, 300) with pressure 0.75, tilt_x=15°, tilt_y=0°, barrel=false
- WHEN `dispatch_pen_down(400, 300, 0.75, 15.0, 0.0, false)` is called
- THEN the hit widget receives `kPenDown`
- AND `event.pressure` is `0.75`
- AND `event.tilt_x` is `15.0`
- AND `event.barrel_button` is `false`

#### Scenario: Touch/pen events hit-test like mouse events

- GIVEN widget A at (0, 0, 200, 200) and widget B at (250, 250, 100, 100)
- WHEN a pen down occurs at (50, 50)
- THEN A receives the event (same hit-test logic as mouse)
- AND the event dispatches through the existing depth-first reverse traversal

### Requirement: Accessibility Metadata on Widget

The `Widget` base class SHALL gain optional fields: `accessible_name` (string), `accessible_role` (enum: kButton, kSlider, kLabel, kPanel, kList, kTextInput, kTabStop, kNone), and `accessible_description` (string). These fields SHALL default to empty/no-role. The `FocusManager` SHALL use `accessible_name` and `accessible_role` when rendering the focus ring and when announcing focus changes. Widgets SHALL inherit their parent's `accessible_role` when none is explicitly set, unless the parent's role is `kNone`.

#### Scenario: Default accessibility state

- GIVEN a newly created RectWidget
- WHEN no accessibility fields are set
- THEN `accessible_name` is empty
- AND `accessible_role` is `kNone`
- AND `accessible_description` is empty

#### Scenario: Focus manager reads accessible name

- GIVEN a focused ButtonWidget with `accessible_name = "Play"` and `accessible_role = kButton`
- WHEN the focus ring renders
- THEN the focus ring SHALL use the role-appropriate styling (button style)
- AND `accessible_name` SHALL be available for a11y announcement

#### Scenario: Widget inherits parent role

- GIVEN a container PanelWidget with `accessible_role = kPanel`
- WHEN a child widget has no explicit role
- THEN `child.effective_role()` returns `kPanel`
- AND if the child sets `accessible_role = kSlider`, `effective_role()` returns `kSlider`

#### Scenario: Accessible description is optional metadata

- GIVEN a SliderWidget with `accessible_name = "Volume"` and `accessible_description = "Controls master output level"`
- WHEN the slider is focused
- THEN `accessible_description` SHALL be accessible via `widget.accessible_description()`
- AND an empty description SHALL NOT affect focus rendering
