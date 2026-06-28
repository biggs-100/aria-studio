# Automation Lanes Specification

## Purpose

Defines automation lanes — one per parameter — with arm/disarm, visibility control, height customization, and clip binding for recording and playback.

## Requirements

### Requirement: A lane SHALL bind one automation clip to one parameter target

Each `AutomationLane` MUST reference a `ParameterID` target and a single `AutomationClip`. The lane SHALL expose `set_clip()`, `has_clip()`, `clip()`, and `set_target()`.

#### Scenario: Lane evaluates its bound clip

- GIVEN a lane bound to parameter `mixer.volume` with a clip containing a point at PPQN 0 (value 0.8)
- WHEN `evaluate(PPQN 0)` is called
- THEN the result MUST be 0.8

#### Scenario: Empty lane returns 0.0

- GIVEN a lane with no clip assigned
- WHEN `evaluate()` is called
- THEN the result MUST be 0.0

### Requirement: A lane SHALL support arm/disarm for recording

The lane's `armed_` property MUST be toggled via `set_armed()`. Only armed lanes SHALL accept recorded automation data. Recording SHALL always target the armed lane's clip.

#### Scenario: Only armed lanes receive recording data

- GIVEN two lanes on the same track, only Lane A is armed
- WHEN recording starts and parameter values are captured
- THEN Lane A MUST contain the recorded data and Lane B MUST remain unchanged

#### Scenario: Arming a lane disarms other lanes on the same track

- GIVEN lanes for Volume (armed) and Pan (armed)
- WHEN Volume lane is re-armed
- THEN Pan lane MUST become disarmed

### Requirement: A lane SHALL support bypass per lane

When bypassed, the lane's evaluation MUST return the base parameter value regardless of clip content. The lane SHALL expose `is_active()` returning true only when not bypassed, has a clip, and the clip has data.

#### Scenario: Bypassed lane has no effect

- GIVEN a lane with a clip at value 0.75
- WHEN `set_bypass(true)` is called and `evaluate()` is invoked
- THEN the result MUST indicate bypass (e.g., return 0.0 or a sentinel)

### Requirement: Lanes SHALL support visibility toggling and height customization

The system MUST support `set_visible()`, `is_visible()`, `set_lane_height(height_px)`, and `lane_height()`. Height SHALL be clamped between configurable min (30px) and max (300px).

#### Scenario: Hidden lane suppresses UI rendering

- GIVEN a lane with `visible_ = false`
- WHEN the UI queries lanes for rendering
- THEN the lane MUST be excluded from the visible lane list

#### Scenario: Lane height is clamped to min/max bounds

- GIVEN a lane with default height 80px
- WHEN `set_lane_height(10)` is called
- THEN `lane_height()` MUST return 30px (the configured minimum)
- WHEN `set_lane_height(500)` is called
- THEN `lane_height()` MUST return 300px (the configured maximum)
