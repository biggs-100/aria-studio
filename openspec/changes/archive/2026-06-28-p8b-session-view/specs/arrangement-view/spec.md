# Delta for arrangement-view

## ADDED Requirements

### Requirement: Arrangement SHALL synchronize transport with session view

The arrangement transport position MUST be shared with the session view. When the transport plays, both views advance synchronously via a shared PPQN position source. The `Transport` SHALL be the single source of truth for playback position.

#### Scenario: Session view clip launches sync with arrangement transport
- GIVEN the arrangement transport is playing at bar 2
- WHEN a session clip is launched
- THEN the clip starts from its beginning, synchronized to the next quantization boundary in transport time

#### Scenario: Arrangement seek updates session position indicator
- GIVEN the arrangement playhead at PPQN 0
- WHEN the user seeks to PPQN 960 in the arrangement
- THEN the session view's position indicator also shows PPQN 960

### Requirement: Arrangement SHALL support session capture recording

A "capture to arrangement" operation MUST read all currently playing session clips and write them as new clip placements in the arrangement timeline at their current PPQN positions. Capture MUST be non-destructive — existing arrangement clips are not removed.

#### Scenario: Scene capture writes clips to arrangement timeline
- GIVEN a session scene S1 with clips on tracks T1 (start 0) and T2 (start 960) currently playing
- WHEN `capture_scene_to_arrangement(S1)` is called
- THEN T1 has a new clip placement at PPQN 0 and T2 at PPQN 960 in the arrangement

#### Scenario: Capture preserves existing arrangement clips
- GIVEN T1 already has an arrangement clip at PPQN 0–960
- WHEN session clips are captured
- THEN the existing clip remains and the captured clip is placed alongside it

### Requirement: Arrangement SHALL show session-clip visual indicator

Clips placed in the arrangement via session capture MUST render with a distinct visual style (e.g., dashed border, "S" badge) distinguishing them from manually arranged clips.

#### Scenario: Captured clip shows session badge
- GIVEN a clip placed via session capture
- WHEN the arrangement renders
- THEN the clip has a visual "S" indicator
