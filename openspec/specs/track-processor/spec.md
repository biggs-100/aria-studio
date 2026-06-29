# Track Processor Specification

## Purpose

Defines the per-track audio processing pipeline — `TrackProcessor` handles clip mixing, WSOLA time stretch, gain/pan, and the plugin chain in a real-time safe path. This is the first specification for this capability, covering the WSOLA time stretch algorithm addition.

## Requirements

### Requirement: TrackProcessor SHALL apply WSOLA time stretch per-clip

A WSOLA (Waveform Similarity Overlap-Add) algorithm MUST be implemented in `apply_time_stretch()`. It MUST accept a stretch ratio (0.5–2.0) and produce output with minimal artifacts by locating the most similar waveform overlap between analysis frames. The algorithm MUST be real-time safe (no allocations, no locks).

#### Scenario: WSOLA at 2.0x plays audio at double speed
- GIVEN a 10-second stereo audio buffer and stretch ratio 2.0
- WHEN `apply_time_stretch()` processes the buffer
- THEN the output is 5 seconds with pitch preserved

#### Scenario: WSOLA at 1.0x is pass-through
- GIVEN a buffer with ratio 1.0
- WHEN `apply_time_stretch()` processes it
- THEN the output is identical to the input (within float precision)

#### Scenario: Extreme ratio clamps to [0.5, 2.0]
- GIVEN ratio 3.0
- WHEN `apply_time_stretch()` is called
- THEN the effective ratio is clamped to 2.0

### Requirement: TrackProcessor SHALL support a polymorphic time stretch interface

A `TimeStretchAlgorithm` abstract base class MUST define `process(buffer, frames, ratio)` — enabling future swap-in of Rubber Band or other libraries without changing the pipeline. `WSOLAStretch` SHALL be the default implementation.

#### Scenario: Rubber Band stub can be swapped in
- GIVEN a `RubberBandStretch` subclass of `TimeStretchAlgorithm`
- WHEN set on a TrackProcessor
- THEN the pipeline delegates to Rubber Band instead of WSOLA

### Requirement: TrackProcessor SHALL support per-clip audio processing pipeline

The process pipeline MUST sequence: (1) read clip slots, (2) apply clip gain envelope, (3) apply time stretch per active clip, (4) sum clips into track buffer, (5) apply track gain/pan, (6) process plugin chain.

#### Scenario: Pipeline processes clips with gain then FX
- GIVEN a track with one clip at gain −6 dB and one EQ plugin
- WHEN `process()` is called
- THEN the clip gain is applied BEFORE the EQ plugin processes

#### Scenario: Empty clip slots produce silence
- GIVEN a track with no active clip slots
- WHEN `process()` is called
- THEN the output buffer is zeroed
