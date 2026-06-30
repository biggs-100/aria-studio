# Script API Bindings Specification

## Purpose

Expose core DAW transport, track, clip, and timing operations to Lua scripts via sol2 `set_function()` and `new_usertype()` bindings under the `aria.*` namespace.

## Requirements

### Requirement: Transport Bindings

The system MUST expose transport operations as `aria.transport.*` Lua bindings.

#### Scenario: Start playback

- GIVEN Transport is stopped
- WHEN `aria.transport.play()` is called
- THEN Playback starts from the current position

#### Scenario: Stop playback

- GIVEN Playback is active
- WHEN `aria.transport.stop()` is called
- THEN Playback stops; position remains at the stop point

#### Scenario: Toggle record

- GIVEN Transport is stopped
- WHEN `aria.transport.record()` is called
- THEN Recording toggles on; armed tracks begin capturing input

#### Scenario: Query transport state

- GIVEN Playback is active
- WHEN `aria.transport.is_playing()` is called
- THEN Returns `true`

### Requirement: Track Bindings

The system MUST expose track operations as `aria.tracks.*` Lua bindings.

#### Scenario: List all tracks

- GIVEN The project has 5 tracks
- WHEN `aria.tracks.list()` is called
- THEN An array of track objects with name, id, and type fields is returned

#### Scenario: Get track by name

- GIVEN A track named "Guitar" exists
- WHEN `aria.tracks.get("Guitar")` is called
- THEN A track object for "Guitar" is returned

#### Scenario: Set track mute

- GIVEN Track "Guitar" is unmuted
- WHEN `aria.tracks.get("Guitar").muted = true`
- THEN The track is muted; audio output stops

#### Scenario: Get nonexistent track

- GIVEN No track named "Bass" exists
- WHEN `aria.tracks.get("Bass")` is called
- THEN `nil` is returned; no error is thrown

### Requirement: Clip Bindings

The system MUST expose clip operations via clip objects returned from `aria.clips.*`.

#### Scenario: Move clip to different track and beat

- GIVEN A clip at track 1, beat 4
- WHEN `clip:move(track=2, beat=8)` is called
- THEN The clip is relocated to track 2, beat 8

#### Scenario: Trim clip duration

- GIVEN A clip is 16 beats long
- WHEN `clip:trim(start=0, end=8)` is called
- THEN The clip duration becomes 8 beats

#### Scenario: Move clip to invalid position

- GIVEN A clip exists
- WHEN `clip:move(track=-1, beat=0)` is called
- THEN An error is returned; the clip position is unchanged

### Requirement: Timing Bindings

The system MUST expose timing queries as `aria.timing.*` Lua bindings.

#### Scenario: Get current playback position

- GIVEN Playback is at bar 4, beat 2
- WHEN `aria.timing.position()` is called
- THEN A position object with `bar=4` and `beat=2` is returned

#### Scenario: Get project tempo

- GIVEN Project tempo is 120 BPM
- WHEN `aria.timing.tempo()` is called
- THEN `120.0` is returned

#### Scenario: Convert beats to seconds

- GIVEN Tempo is 120 BPM (0.5 s/beat)
- WHEN `aria.timing.beats_to_seconds(4)` is called
- THEN `2.0` is returned
