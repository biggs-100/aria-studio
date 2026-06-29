# Delta for Automation Clips

## ADDED Requirements

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

## RENAMED Requirements

### Requirement: Clips SHALL support point-based automation with interpolation → Clips SHALL support point-based automation with interpolation (base AutomationClip type)

(Reason: The base `AutomationClip` class is now referenced by `AutomationClipWrapper` from the arrangement timeline; the core automation data model is unchanged, but its role is now explicitly the data backend rather than a standalone arrangement element.)
(Migration: All existing references to `AutomationClip` as an arrangement element should route through `AutomationClipWrapper` instead. The underlying `AutomationClip` evaluation API remains identical.)
