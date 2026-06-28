# Modulation Sources Specification

## Purpose

Defines all modulation source types — LFO, envelope follower, step sequencer, macro, random/S&H, and note property — that drive real-time parameter modulation.

## Requirements

### Requirement: LFO sources SHALL support 10 waveforms with sync options

The `LFOSource` MUST provide Sine, Triangle, Saw, SawInv, Square, SampleAndHold, Noise, RampUp, RampDown, and Pulse waveforms. Rate MUST be configurable in Hz or note-synced. The source MUST support key-sync (retrigger on note-on) and tempo-sync.

#### Scenario: Note-synced LFO retriggers on key-sync

- GIVEN an LFO with `rate_synced_ = true`, `rate_note_ = 1.0/4`, and `key_sync_ = true`
- WHEN a new note-on occurs at PPQN 240
- THEN the LFO phase MUST reset to the configured `phase_` value

#### Scenario: Free-running LFO advances independently of transport

- GIVEN an LFO with `rate_synced_ = false` and `rate_hz_ = 1.0`
- WHEN the transport is stopped for 2 seconds
- THEN the LFO MUST have advanced by approximately 2 full cycles

### Requirement: Envelope follower SHALL extract amplitude from audio

The `EnvelopeFollowerSource` MUST support configurable attack (0–100ms) and release (0–1000ms) times, multiple output modes (Amplitude, Envelope, Peak, RMS), and sidechain source selection.

#### Scenario: Envelope follower tracks amplitude peaks

- GIVEN an envelope follower with `attack_ms_ = 1.0`, `release_ms_ = 100.0`, `mode = Peak`
- WHEN a transient audio signal is fed to the sidechain input
- THEN the follower output MUST rise within 1ms and decay over ~100ms

#### Scenario: RMS mode produces smoothed output

- GIVEN an envelope follower in `RMS` mode with `attack_ms_ = 50.0`
- WHEN fed with rapidly fluctuating amplitude
- THEN the output MUST be smoother than `Amplitude` mode at equivalent settings

### Requirement: Step sequencer SHALL produce stepped patterns

The `StepSequencerSource` MUST support 1–64 steps, configurable steps per beat, smoothing, swing, randomize amount, and trigger modes (Free, NoteOn, NoteGate, Transport).

#### Scenario: Step sequencer advances on each note-on

- GIVEN a 4-step sequencer with `trigger = NoteOn` and values [0.0, 0.3, 0.7, 1.0]
- WHEN four note-on events occur
- THEN the output MUST sequence through 0.0, 0.3, 0.7, 1.0 and wrap back to 0.0

#### Scenario: Transport-triggered sequencer resets on play

- GIVEN a 16-step sequencer with `trigger = Transport`
- WHEN transport starts from stop
- THEN the sequencer MUST reset to step 0

### Requirement: Macro knobs SHALL support user-assigned modulation

MacroSource MUST provide a named knob (value 0.0–1.0) settable via UI or MIDI learn, lock-free via `std::atomic<double>`, with multiple parameter mapping targets each having min/max range.

#### Scenario: Macro maps value to parameter range

- GIVEN a macro with `value_ = 0.5` mapping to parameter filter_cutoff with min=200, max=20000
- WHEN the macro is evaluated
- THEN the mapped output MUST be 10100

#### Scenario: MIDI learn updates macro value atomically

- GIVEN a macro configured for MIDI learn on CC 1
- WHEN a MIDI CC 1 value of 64 is received
- THEN the macro's `value_` MUST be updated to approximately 0.5

### Requirement: Random/S&H source SHALL generate stepped random values

The system MUST support a sample-and-hold modulation source with configurable rate, smoothing time, and random walk mode.

#### Scenario: S&H holds value between trigger events

- GIVEN an S&H source with rate = 1/4 note
- WHEN evaluated between rate intervals
- THEN the output MUST remain constant until the next trigger

### Requirement: Note property source SHALL read note attributes

The system MUST support modulation sources that read current note velocity, pitch, pressure, and timbre from `ModulationContext`.

#### Scenario: Note velocity modulates target parameter

- GIVEN a note property source configured for `current_note_velocity`
- WHEN a note-on with velocity 0.75 is triggered
- THEN the source output MUST be 0.75
