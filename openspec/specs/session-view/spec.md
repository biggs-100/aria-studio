# Session View Specification

## Purpose

Defines the session view model — a grid of clip slots organized by tracks (columns) and scenes (rows). Supports clip launching, scene triggering, follow actions, crossfader assignment, and quantization. The C++ model (Scene, ClipSlot, LaunchState, Crossfader, FollowAction) drives a TypeScript Canvas UI.

## Requirements

### Requirement: Session SHALL manage a grid of clip slots organized by track and scene

The `Session` model MUST store a 2D grid indexed by `TrackID` × `SceneID`. Each slot MAY hold a `Clip` or be empty. Scenes MUST be ordered and named.

#### Scenario: Clip placed in slot is retrievable
- GIVEN a Session with Track T1 and Scene S1
- WHEN `set_clip_slot(T1, S1, clip)` is called
- THEN `clip_at(T1, S1)` returns the clip

#### Scenario: Empty slot returns nullptr
- GIVEN a Session with an unset slot (T1, S2)
- WHEN `clip_at(T1, S2)` is called
- THEN it returns nullptr

### Requirement: Session SHALL support scene and clip launch with quantization

`launch_scene()` MUST start all clips in that scene simultaneously. `launch_clip()` MUST start a single clip on its track, stopping any previously playing clip on that track. Launch MUST respect `LaunchQuantization` — the action queues until the next quantization boundary.

#### Scenario: Scene launch triggers all clips in scene
- GIVEN Scene S1 with clips at (T1,S1), (T2,S1)
- WHEN `launch_scene(S1)` is called
- THEN both clips begin playback at the next quantization boundary

#### Scenario: Clip launch stops previous clip on same track
- GIVEN Track T1 playing clip from scene S1
- WHEN `launch_clip(T1, S2)` is called
- THEN the S1 clip stops and the S2 clip starts

### Requirement: Session SHALL support follow actions

Follow actions (`Stop`, `PlayNext`, `PlayRandom`, `ContinueAsLoop`) MUST execute when a clip finishes playback. Per-clip and per-scene follow actions SHALL be configurable.

#### Scenario: Follow action advances to next scene
- GIVEN Scene S1 with follow action `PlayNext` and Scene S2 exists
- WHEN all clips in S1 finish playback
- THEN S2 is launched automatically

### Requirement: Session SHALL support crossfader assignment

Tracks MUST be assignable to crossfader side A, side B, both, or none. The crossfader position (−1.0 to 1.0) MUST scale the gain of tracks on each side: A scales left, B scales right.

#### Scenario: Crossfader at +1.0 silences A-assigned tracks
- GIVEN Track T1 assigned to crossfader side A, crossfader at 0.0
- WHEN crossfader is moved to +1.0
- THEN T1's effective gain is 0 (fully B side)

### Requirement: Session TS UI SHALL render the clip grid with launch state indicators

The Canvas-based UI MUST display a scrollable grid: columns = tracks, rows = scenes. Filled slots MUST show clip name and color. Playing clips MUST show a pulsing indicator. Scene launch buttons and a crossfader slider MUST be interactive.

#### Scenario: Playing clip shows visual indicator
- GIVEN a clip slot with a playing clip
- WHEN the UI renders
- THEN the slot has a green/playing border

#### Scenario: Empty slot renders as empty cell
- GIVEN a slot with no clip
- WHEN the UI renders
- THEN the slot appears as a dim empty rectangle
