# Delta for track-types

## MODIFIED Requirements

### Requirement: AudioTrack SHALL own audio clips and crossfades

`AudioTrack` MUST store a vector of shared `AudioClip` pointers and support `clip_at(ppqn)` position queries, `clips_in_range()` range queries, crossfade pairs between overlapping clips, and clip slot management for session view integration.
(Previously: no session clip slot management; crossfade pairs were specified at conceptual level only)

#### Scenario: AudioTrack returns clip at given position
- GIVEN an AudioTrack with one AudioClip at PPQN 0, length 960
- WHEN `clip_at(480)` is called
- THEN the clip is returned (position 480 falls within [0, 960))

#### Scenario: Crossfade renders overlapping clip transition
- GIVEN two AudioClips with a 96-sample overlap and crossfade defined
- WHEN the playback crosses the overlap boundary
- THEN the gain envelope blends the two clips using equal-power curve

#### Scenario: Session clip slot associates track clip with scene
- GIVEN an AudioTrack with a clip placed in session slot (Scene S1)
- WHEN the session view queries the slot
- THEN the track returns the clip reference for that scene

### Requirement: MidiTrack SHALL own MIDI clips and instrument assignment

`MidiTrack` MUST store `MidiClip` objects, an optional instrument `PluginID`, transpose offset, velocity curve, drum map, and session clip slot management.
(Previously: no session clip slot management; instrument and transpose were specified at conceptual level only)

#### Scenario: MidiTrack applies transpose on playback
- GIVEN a MidiTrack with transpose=+5 semitones and a MidiClip containing C3
- WHEN the clip is played back
- THEN all notes output at F3 (C3 + 5 semitones)

#### Scenario: MidiTrack routes MIDI to instrument plugin
- GIVEN a MidiTrack with an instrument plugin assigned
- WHEN a clip plays
- THEN MIDI events are routed to the instrument for audio generation

#### Scenario: Session slot returns MidiClip for scene
- GIVEN a MidiTrack with a clip in session slot (Scene S1)
- WHEN `session().clip_at(track, S1)` is called
- THEN the MidiClip reference is returned

## ADDED Requirements

### Requirement: AudioTrack and MidiTrack SHALL support freeze state

Both subclasses MUST manage a freeze state flag, a pointer to the frozen audio clip (if frozen), and a stale/dirty flag triggered by clip or FX edits.

#### Scenario: AudioTrack freeze flag prevents clip edits
- GIVEN a frozen AudioTrack
- WHEN `add_clip()` is called
- THEN the operation returns false (track is frozen)
