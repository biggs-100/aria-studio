# Freeze & Bounce Specification

## Purpose

Defines the freeze/bounce subsystem — `FreezeManager` orchestrates per-track offline rendering to replace FX-heavy tracks with rendered audio clips, and multi-track bounce (individual, stem, master) to WAV files.

## Requirements

### Requirement: FreezeManager SHALL freeze a track by rendering FX to audio

`freeze_track()` MUST render the track's output through its entire FX chain for the project length, store the result as an `AudioClip`, bypass all track FX, and replace clips with the frozen render. `unfreeze_track()` MUST restore the original clips and re-enable FX.

#### Scenario: Frozen track bypasses FX during playback
- GIVEN a track with 3 compressor plugins, `freeze_track(track)` called
- WHEN the track processes audio
- THEN all FX are bypassed and the frozen audio clip plays instead

#### Scenario: Unfreeze restores original clips and FX
- GIVEN a frozen track
- WHEN `unfreeze_track(track)` is called
- THEN original clips reappear and all FX are re-enabled

### Requirement: FreezeManager SHALL detect stale freeze state

A dirty-flag per track MUST be set when clips, FX, or routing change on a frozen track. The freeze render SHALL be considered stale until re-freeze.

#### Scenario: Clip edit marks freeze as dirty
- GIVEN a frozen track
- WHEN a clip is moved on the track
- THEN `is_frozen(track)` returns true but a stale indicator is set

### Requirement: FreezeManager SHALL support bounce in three modes

`bounce()` MUST accept `BounceFormat`: `Individual` (one file per track), `Stem` (selected tracks summed per group), or `Master` (full mix). Each mode MUST render via the `OfflineRenderer`.

#### Scenario: Master bounce renders all tracks to stereo WAV
- GIVEN a project with 4 tracks and master FX
- WHEN `bounce({tracks: all, format: Master, range: [0, 3840]})` is called
- THEN a single stereo WAV of the full mix is written to disk

#### Scenario: Individual bounce produces per-track files
- GIVEN tracks T1, T2
- WHEN `bounce({tracks: [T1,T2], format: Individual, range: [0, 1920]})` is called
- THEN two WAV files are created (T1.wav, T2.wav)

### Requirement: OfflineRenderer SHALL support per-track rendering

The renderer MUST accept a `TrackID` filter and render only that track's audio through its FX chain to a buffer, without master FX or other tracks.

#### Scenario: Per-track render excludes other tracks
- GIVEN a project with tracks T1 (drums) and T2 (bass)
- WHEN rendering only T2
- THEN the output buffer contains only bass audio, zero drums
