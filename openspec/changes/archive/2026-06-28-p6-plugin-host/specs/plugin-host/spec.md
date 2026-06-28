# Plugin Host Specification

## Purpose

Define the unified `AudioPlugin` abstraction, CLAP (v1.2+) and VST3 host implementations, native CLAP plugin factory, and thread-safe parameter system with automation. All built-in plugins register as CLAP — no internal fast path.

## Requirements

### Requirement: AudioPlugin Lifecycle

Every instance MUST implement: `init()` → `activate()` → `process()` cycles → `deactivate()` → `destroy()`. `init()` loads the binary. The host MUST NOT call `process()` before `activate()` returns true.

#### Scenario: Standard lifecycle completes
- GIVEN a valid CLAP `.clap` file
- WHEN `init()` is called
- THEN it returns true AND `activate()` at 48 kHz / 256 frames returns true
- AND processing runs until `deactivate()` and `destroy()`

#### Scenario: Unsupported buffer size fails
- GIVEN a plugin supporting only 32–512 frames
- WHEN `activate()` is called with 1024 frames
- THEN it returns false AND the plugin accepts a subsequent valid call

### Requirement: Audio Processing and MIDI

`process()` MUST accept `ProcessContext` with float I/O buffers, sample position, transport, and parameter changes with sample offsets. MIDI events MUST arrive via `set_midi_events()` before each `process()` call.

#### Scenario: Bypassed plugin passes audio through
- GIVEN an active plugin with `set_bypass(true)`
- WHEN `process()` receives a sine wave
- THEN output matches input within FP tolerance

#### Scenario: Sample-offset parameter changes apply in order
- GIVEN parameter changes at sample offsets 32 and 128
- WHEN processing a 256-frame buffer
- THEN the first change applies before sample 32 AND the second at sample 128

### Requirement: Parameter System

`ParameterManager` MUST support thread-safe access, begin/perform/end edit for automation, modulation stacking, and text conversion. Each parameter has a stable `ParamID`, name, unit, range, default, and step count.

#### Scenario: Automation edit creates undo boundary
- GIVEN a parameter at default 0.5
- WHEN `begin_edit()` and `perform_edit()` set it to 0.8, then `end_edit()`
- THEN `get_value()` returns 0.8 AND the host records an undo action

#### Scenario: Concurrent reads do not block audio thread
- GIVEN a parameter being updated by the UI thread
- WHEN the audio thread calls `get_value()` simultaneously
- THEN it returns the previous or current value without blocking

### Requirement: State Serialization

`save_state()` / `load_state()` MUST preserve all parameter values, bypass state, and plugin-specific data as a portable byte vector.

#### Scenario: State round-trip restores exactly
- GIVEN a plugin with non-default parameters and bypass enabled
- WHEN state is saved, a new instance created, and state loaded
- THEN all parameter values and bypass state match exactly

### Requirement: Native CLAP Plugin Factory

`NativeCLAPFactory` MUST register built-in DSP (EQ, compressor, reverb, delay, synth) through `clap_plugin_entry`. Built-in plugins MUST be indistinguishable from third-party CLAP plugins.

#### Scenario: Built-in loads same as third-party
- GIVEN NativeCLAPFactory registered "aria.eq"
- WHEN `PluginFactory::create_plugin()` is called with its PluginID
- THEN it returns an `AudioPlugin*` responsive to all CLAP host extensions

### Requirement: CLAP Host Extensions

The host MUST implement required extensions: `clap_audio_ports`, `clap_note_ports`, `clap_params`, `clap_latency`, `clap_tail`, `clap_state`, and `clap_mpe`. Optional extensions (GUI, timers, thread_pool, render, track_info, voice_info) MAY be implemented.

#### Scenario: Plugin queries latency on activation
- GIVEN a reverb with 2048 samples latency
- WHEN the host queries `clap_latency` during activation
- THEN `AudioPlugin::latency()` returns 2048

### Requirement: VST3 Component/Controller

The VST3 host MUST separate `IComponent` (audio) and `IEditController` (params/state). `IAudioProcessor::process()` MUST handle real-time and offline modes. Bus config MUST enumerate audio inputs, outputs, and event busses.

#### Scenario: Multi-bus VST3 enumerates correctly
- GIVEN a VST3 plugin with 2 stereo output busses
- WHEN `audio_io_config()` is called after activation
- THEN it reports 4 output channels across 2 busses

### Requirement: PluginFactory Registry

`PluginFactory` MUST be a singleton managing discovered descriptors, creating instances by ID, and emitting `on_plugin_discovered`. It MUST support query by category and text search.

#### Scenario: Search finds plugin by vendor
- GIVEN a registered plugin with vendor "ValhallaDSP"
- WHEN `search_plugins("Valhalla")` is called
- THEN the descriptor is in results
