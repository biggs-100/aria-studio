# TS View Arrangement Specification

## Purpose

Port of the Canvas 2D ArrangementView to the GPU Component tree. Track headers, clip rendering, playhead, grid lines, zoom, and pan SHALL render via the FFI bridge. Zero Canvas 2D calls SHALL remain after port.

## Requirements

### Requirement: Track-Header Component

Each track SHALL render as a component showing name, type badge, mute/solo buttons, and fold arrow (for group tracks). Layout SHALL match the existing Canvas 2D TrackList rendering.

#### Scenario: Track header renders all controls

- GIVEN a track with name "Kick", type Audio, muted=false, soloed=false
- WHEN mounted in the ArrangementView component tree
- THEN the VNode subtree contains a text label "Kick", an Audio type badge, a grey "M" circle, and a grey "S" circle

#### Scenario: Fold arrow toggles children visibility

- GIVEN a GroupTrack with 2 children and folded=false
- WHEN the fold arrow is clicked
- THEN setState fires, updating the expanded/collapsed state
- AND the dirty-tree scheduler re-renders the track header branch

### Requirement: Clip Rendering on Timeline

Clips SHALL render as rectangles with type-specific content (waveform for audio, density bars for MIDI, curve for automation). Selection borders, fade handles, and muted overlays SHALL match the Canvas 2D output.

#### Scenario: Audio clip with waveform

- GIVEN an AudioClip with peak/valley waveform data
- WHEN it is in the visible viewport
- THEN the component renders a filled rect with a waveform polygon overlay

#### Scenario: Clip outside viewport is culled

- GIVEN a clip whose pixel bounds are entirely outside the visible scroll region
- WHEN the frame renders
- THEN the clip component produces no draw commands (culled)

### Requirement: Playhead and Grid Lines

The playhead SHALL render as a vertical line at the current playback PPQN position. Grid lines SHALL render at beat and bar intervals, respecting zoom level. Both SHALL update reactively via the data source's render loop.

#### Scenario: Playhead advances during playback

- GIVEN playback at 120 BPM
- WHEN the data source reports playhead advancing by 1 beat
- THEN the playhead component re-renders at the new x-position via dirty marking

#### Scenario: Zoom changes grid density

- GIVEN a zoom level change from bars-level to beats-level
- WHEN pixelsPerPPQN crosses the threshold
- THEN the grid component re-renders with finer tick spacing

### Requirement: Zoom and Pan

Scroll position (scrollX, scrollY) and zoom (pixelsPerPPQN) SHALL be owned by the ArrangementView root component. Changes SHALL dirty the entire viewport subtree for re-render.

#### Scenario: Pan updates all children

- GIVEN scrollY offset at 0
- WHEN a wheel event increases scrollY by 100
- THEN setState marks the viewport dirty, re-rendering clips and tracks at the new scroll position
