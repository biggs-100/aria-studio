# Automation Clips Specification

## Purpose

Defines automation clip types — point-based, step, and LFO — with 10 interpolation modes for parameter automation curves in ARIA DAW.

## Requirements

### Requirement: Clips SHALL support point-based automation with interpolation (base AutomationClip type)

The system MUST store automation points sorted by PPQN position. Each point SHALL hold a normalized `double` value (0.0–1.0), an `InterpolationType`, and optional `BezierControl` points. The system MUST evaluate curves between points using the declared interpolation type.

#### Scenario: Evaluate linear interpolation between two points

- GIVEN a clip with points at PPQN 0 (value 0.0) and PPQN 960 (value 1.0)
- WHEN evaluated at PPQN 480
- THEN the result MUST be approximately 0.5

#### Scenario: Hold interpolation produces stepped output

- GIVEN a clip with `InterpolationType::Hold` between two points
- WHEN evaluated at any position between them
- THEN the value MUST equal the first point's value

### Requirement: The system SHALL support 10 interpolation modes

The interpolation types MUST include: Hold, Linear, Bezier, Smooth (Catmull-Rom), EaseIn, EaseOut, EaseInOut, Exponential, Logarithmic, SCurve.

#### Scenario: Bezier curve evaluation with control points

- GIVEN a clip with two points connected by a `Bezier` curve where control point 1 = (0.2, 0.8) and control point 2 = (0.8, 0.2)
- WHEN evaluated at t = 0.5
- THEN the result MUST follow the cubic Bezier formula, not linear interpolation

#### Scenario: Exponential curve approaches asymptote

- GIVEN a clip using `InterpolationType::Exponential` from 0.0 to 1.0
- WHEN evaluated near the start of the segment
- THEN the result MUST be closer to 0.0 than a linear interpolation would produce

### Requirement: Clips SHALL support step automation

The system MUST provide a `StepAutomationClip` type with configurable step count (1–64), per-step values, optional smoothing between steps, and swing amount.

#### Scenario: Step clip evaluates to the correct step value

- GIVEN a 4-step clip with values [0.0, 0.5, 1.0, 0.25]
- WHEN evaluated at the third step position
- THEN the result MUST be 1.0

#### Scenario: Smooth step clip interpolates between steps

- GIVEN a 4-step clip with `smooth_ = true` and values [0.0, 1.0, 0.0, 1.0]
- WHEN evaluated at the midpoint between step 1 and step 2
- THEN the result MUST be 0.5

### Requirement: Clips SHALL support LFO-based automation

The system MUST provide an `LFOAutomationClip` type with waveform selection (Sine, Triangle, Saw, SawInv, Square, SampleAndHold, Noise, RampUp, RampDown), rate (Hz or note-synced), phase offset, pulse width, DC offset, and bipolar toggle.

#### Scenario: LFO clip produces correct waveform at known phase

- GIVEN an LFO clip with `Sine` waveform, `phase_ = 0.0`, `bipolar_ = false`
- WHEN evaluated at the start of the cycle (t = 0)
- THEN the result MUST equal the DC offset (default 0.5)

#### Scenario: Bipolar LFO produces negative values

- GIVEN an LFO clip with `Sine` waveform, `bipolar_ = true`, `offset_ = 0.0`
- WHEN evaluated at the cycle midpoint (t = 0.5)
- THEN the result MUST be 0.0 (crossing the center)

### Requirement: Points SHALL support bulk operations

The system MUST support quantizing points to a grid, offsetting all values by a constant, scaling all values by a factor, and clearing all points.

#### Scenario: Quantize snaps points to nearest grid position

- GIVEN a clip with a point at PPQN 47 and grid = 16
- WHEN `quantize_points(16)` is called
- THEN the point MUST be moved to PPQN 48

### Requirement: AutomationClips SHALL be owned by tracks and positioned on the arrangement timeline

Each track MUST own its automation clips (stored as `AutomationClipWrapper` objects). The `AutomationClipWrapper` — a subclass of `Clip` — wraps an `AutomationClipID` to reference the actual automation data in the `AutomationEngine` and adds timeline positioning, display range, and a target parameter binding.

#### Scenario: Track returns automation clips at a timeline position

- GIVEN a track with an AutomationClipWrapper at PPQN 0, length 960, wrapping AutomationClipID=42
- WHEN `clip_at(480)` is called on the track
- THEN the wrapper is returned

#### Scenario: AutomationClipWrapper displays with configured range

- GIVEN an AutomationClipWrapper with display_min=0.0, display_max=1.0
- WHEN the arrangement view renders the automation curve
- THEN the Y-axis maps 0.0 to bottom and 1.0 to top of the clip rectangle

### Requirement: AutomationClips SHALL appear as filled curve envelopes in arrangement view

When an AutomationClipWrapper exists on a track, the arrangement view MUST render its automation curve as a filled envelope shape inside the clip rectangle, using the underlying `AutomationClip::evaluate()` for the visible segment.

#### Scenario: Automation curve renders in clip rectangle

- GIVEN an AutomationClipWrapper at PPQN 0, length 960, wrapping a sine LFO
- WHEN rendered in arrangement view
- THEN the sine wave fills the clip rectangle area

#### Scenario: Empty automation clip renders flat line

- GIVEN an AutomationClipWrapper with no automation points (default value)
- WHEN rendered
- THEN a flat line at the default value fills the clip rectangle
