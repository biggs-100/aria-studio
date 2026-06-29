# Clip Model Specification

## Purpose

Defines the abstract `Clip` base class, concrete `AudioClip` subclass, and `ClipID` identity system used across all track types and views in ARIA DAW. All clip types (MIDI, Audio, Automation) inherit from this base.

## Requirements

### Requirement: Clip SHALL provide base position, length, and looping metadata

The system MUST define an abstract `Clip` base class with PPQN-based position (`start`, `length`), looping range `loop_start/loop_end`, and `looping` toggle. A `ClipID` struct with unique `uint64_t` value MUST identify every clip.

#### Scenario: Clip stores position and reports end
- GIVEN a Clip at start=0 PPQN with length=960 PPQN
- WHEN `end()` is called
- THEN it MUST return 960 PPQN

#### Scenario: Looping clip repeats within loop range
- GIVEN a Clip with looping enabled, loop_start=0, loop_end=480
- WHEN `evaluate()` at PPQN 960 is requested
- THEN the effective position wraps to PPQN 480 (loop_end)

### Requirement: Clip SHALL support fade in/out with configurable shapes

The Clip MUST store `fade_in_ppqn`, `fade_out_ppqn`, and `FadeShape` (Linear, EqualPower, Exponential, Logarithmic, SCurve) for each edge. Zero-length fades MUST produce no gain modification.

#### Scenario: Fade-in applies gain envelope at clip start
- GIVEN a Clip with fade_in=96 PPQN, shape=Linear
- WHEN the playback position is at 48 PPQN (50% through fade)
- THEN the fade gain is 0.5 (linear midpoint)

#### Scenario: Zero-length fade is no-op
- GIVEN a Clip with fade_in=0
- WHEN gain is evaluated at the clip start
- THEN the fade factor is 1.0 (no attenuation)

### Requirement: AudioClip SHALL reference an audio file and cache waveform data

`AudioClip` inherits from `Clip` and MUST store `file_path`, `file_hash`, `sample_offset`, and `sample_length`. It MUST provide an LOD-based `WaveformCache` with peak/minima arrays at configurable resolution.

#### Scenario: Waveform cache returns downsampled peaks
- GIVEN an AudioClip referencing a 44100-sample file
- WHEN `get_waveform(441)` is called
- THEN the cache returns 100 peak values (1 per 441 samples)

#### Scenario: Sample offset trims playback start
- GIVEN an AudioClip with sample_offset=44100, total_samples=88200
- WHEN the clip plays for its full length
- THEN only the last 44100 samples of the source file are heard

### Requirement: AudioClip SHALL support a clip-specific gain envelope

The AudioClip MUST store a `gain_envelope` — a vector of `GainPoint` (PPQN position, normalized gain) — independent of track or automation gain. An empty envelope MUST apply unity gain.

#### Scenario: Gain envelope applies per-position gain
- GIVEN an AudioClip with GainPoints at PPQN 0 → 0.0 and PPQN 960 → 1.0
- WHEN evaluated at PPQN 480
- THEN the clip gain is 0.5
