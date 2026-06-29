# Arrangement View Specification

## Purpose

Defines the arrangement timeline — the primary horizontal composition view showing tracks as rows and clips as positioned rectangles along a time axis with bar/beat grid, clip selection, drag-and-drop, and fade/crossfade rendering.

## Requirements

### Requirement: Arrangement SHALL render a time-axis grid with bar/beat markers

The arrangement view MUST display a horizontal time axis with bar numbers, beat markers, and PPQN-aligned grid lines. Grid resolution MUST adapt to zoom level: show bars at low zoom, beats at medium, sixteenths at high. The playhead position MUST be rendered as a vertical cursor.

#### Scenario: Playhead advances during playback
- GIVEN the transport is playing at 120 BPM
- WHEN 1 second elapses
- THEN the playhead moves 960 PPQN (1 quarter note at 120 BPM)

#### Scenario: Grid lines snap to beat boundaries
- GIVEN zoom level showing beats
- WHEN the grid is rendered
- THEN each beat position has a visually distinct grid line

### Requirement: Arrangement SHALL display clips as positioned rectangles per track row

Each clip MUST render as a rectangle at its `start` PPQN position with width proportional to its `length` PPQN. Audio clips MUST show a waveform thumbnail (peaks from LOD cache). MIDI clips SHOULD show a mini piano roll or note density. Automation clips MUST show their curve as a filled envelope.

#### Scenario: Audio clip renders waveform thumbnail
- GIVEN an AudioClip at PPQN 0, length 960, with waveform cache at visible resolution
- WHEN the clip rectangle is drawn
- THEN the waveform peaks are rendered inside the rectangle bounds

#### Scenario: Clip rectangle width matches length
- GIVEN two clips with lengths 960 and 1920 at the same zoom
- WHEN rendered
- THEN the second clip's rectangle is exactly twice as wide as the first

### Requirement: Arrangement SHALL support clip selection and drag-drop repositioning

The user MUST be able to click a clip to select it, SHIFT-click to extend selection, and drag to reposition. Drag MUST snap to grid when grid snap is enabled. Dropping at an invalid position (e.g., wrong track type) MUST be rejected.

#### Scenario: Selected clip moves with drag
- GIVEN a clip at PPQN 0 with grid snap off
- WHEN the user drags it 480 PPQN to the right
- THEN `clip.start()` is 480

#### Scenario: Drag snaps to grid
- GIVEN grid snap enabled at 16th notes (240 PPQN)
- WHEN the user drags to PPQN 310
- THEN the clip snaps to PPQN 240 (nearest 16th)

### Requirement: Arrangement SHALL render clip fades and crossfades

Fade-in/out SHALL be rendered as triangular/curved overlays at clip edges. Crossfades between overlapping clips on the same track SHALL render as a shared fade region. The user MUST be able to drag fade handles.

#### Scenario: Fade handle drag changes fade length
- GIVEN a clip with fade_in=96 PPQN
- WHEN the user drags the left fade handle to increase to 192 PPQN
- THEN `clip.fade_in()` returns 192

#### Scenario: Crossfade renders between overlapping clips
- GIVEN two clips on the same track overlapping by 96 PPQN with a crossfade defined
- WHEN rendered
- THEN a shared fade curve spans the overlap region

### Requirement: Track folder hierarchy SHALL affect arrangement row display

When a `GroupTrack` has `folded=true`, its child tracks MUST be hidden from the arrangement view's track list. An expand/collapse arrow MUST be rendered on the group track header.

#### Scenario: Folded group hides children from view
- GIVEN a folded GroupTrack with 2 children
- WHEN the arrangement queries visible tracks
- THEN only the GroupTrack appears in the list

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
