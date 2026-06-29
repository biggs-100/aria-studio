# Track Types Specification

## Purpose

Defines specialized track subclasses — `AudioTrack`, `MidiTrack`, `GroupTrack`, `VCATrack`, `ReturnTrack`, and `MasterTrack` — inheriting from the shared `Track` base. These enable type-specific behavior: child summing, VCA fader control, FX-only return processing.

## Requirements

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

### Requirement: AudioTrack and MidiTrack SHALL support freeze state

Both subclasses MUST manage a freeze state flag, a pointer to the frozen audio clip (if frozen), and a stale/dirty flag triggered by clip or FX edits.

#### Scenario: AudioTrack freeze flag prevents clip edits
- GIVEN a frozen AudioTrack
- WHEN `add_clip()` is called
- THEN the operation returns false (track is frozen)

### Requirement: GroupTrack SHALL sum child tracks with configurable mode

`GroupTrack` MUST store child `TrackID`s, support `add_child()`/`remove_child()`, and provide three `GroupMode` values: `Summing` (sum audio), `Folder` (visual only, no summing), and `Routing` (children route individually). Fold state (`folded`/`unfolded`) MUST collapse/expand children in the UI.

#### Scenario: GroupTrack sums child audio
- GIVEN a GroupTrack with two child AudioTracks at −6 dB each
- WHEN the mixer processes
- THEN the GroupTrack output is the sum (approximately 0 dB)

#### Scenario: Folded group hides children visually
- GIVEN a GroupTrack with `folded=true` and 3 children
- WHEN the arrangement view queries visible tracks
- THEN the children MUST NOT appear in the visible track list

### Requirement: VCATrack SHALL control slave faders proportionally

`VCATrack` MUST store slave `TrackID`s and move all slave faders proportionally when the VCA fader changes. A `VCAAffects` enum MUST configure which parameters the VCA controls: `Volume`, `VolumePan`, or `All` (volume + pan + sends).

#### Scenario: VCA fader moves slaves proportionally
- GIVEN a VCA track at 0 dB with a slave AudioTrack at −6 dB
- WHEN the VCA fader moves to −12 dB
- THEN the slave's effective volume is −18 dB (−6 + −12)

#### Scenario: VCA slave at −∞ stays at −∞
- GIVEN a VCA track with a slave track at −∞ dB
- WHEN the VCA fader moves to −6 dB
- THEN the slave remains at −∞ (muted stays muted)

### Requirement: ReturnTrack SHALL process FX with solo-safe behavior

`ReturnTrack` MUST own an FX chain (vector of `PluginID`), have no clips or inputs, always route to Master (or parent group), and default to `solo_safe=true` (never affected by track solo).

#### Scenario: Solo-safe return remains audible during solo
- GIVEN a ReturnTrack with `solo_safe=true` and a soloed AudioTrack
- WHEN the mixer processes with solo active
- THEN the ReturnTrack output is still audible

### Requirement: MasterTrack SHALL provide final output with limiter and dither

`MasterTrack` MUST own a master FX chain, hardware output configuration, dither type, and separate monitor output. It MUST always exist, cannot be deleted, and sits at the end of the routing chain.

#### Scenario: MasterTrack cannot be deleted
- GIVEN a project with a MasterTrack
- WHEN `delete_track(MasterTrack.id)` is called
- THEN the operation fails (returns false or throws)
