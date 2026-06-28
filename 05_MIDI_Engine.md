# ARIA DAW — MIDI Engine Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23
> **Real-Time Safety**: Audio thread path is lock-free

---

## Table of Contents

1. [Overview](#1-overview)
2. [MIDI Device Abstraction](#2-midi-device-abstraction)
3. [MIDI Data Model](#3-midi-data-model)
4. [MIDI Graph](#4-midi-graph)
5. [MIDI Clock & Sync](#5-midi-clock--sync)
6. [MIDI Recording & Playback](#6-midi-recording--playback)
7. [MPE (MIDI Polyphonic Expression)](#7-mpe-midi-polyphonic-expression)
8. [MIDI Processing & Transformation](#8-midi-processing--transformation)
9. [MIDI Learn & Mapping](#9-midi-learn--mapping)
10. [Timing & Jitter Management](#10-timing--jitter-management)
11. [API Reference](#11-api-reference)
12. [RFC Index](#12-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The MIDI Engine handles all Musical Instrument Digital Interface operations in ARIA DAW. It is responsible for:

- MIDI device input/output (classic MIDI, USB MIDI, virtual ports)
- MIDI clock generation and synchronization
- MIDI recording with sample-accurate timestamps
- MIDI playback through the audio graph
- MPE (MIDI Polyphonic Expression) encoding/decoding
- MIDI learn and hardware controller mapping
- MIDI CC, NRPN, and SysEx handling

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Input jitter | < 1ms deviation from ideal timing |
| Clock accuracy | < 0.1 BPM deviation |
| Max simultaneous notes | Unlimited (limited by system memory) |
| MIDI resolution | 960 PPQN (pulses per quarter note) |
| MPE support | Full spec (MIDI 1.0 MPE) |
| Port count | 128+ virtual MIDI ports |

### 1.3. MIDI Engine Architecture

```
┌────────────────────────────────────────────────────────────┐
│                      MIDI Engine                            │
│                                                             │
│  ┌────────────────┐  ┌────────────────┐  ┌──────────────┐  │
│  │  MIDI Input    │  │  MIDI Output   │  │  MIDI Clock  │  │
│  │  Manager       │  │  Manager       │  │  & Sync      │  │
│  └───────┬────────┘  └────────┬───────┘  └──────┬───────┘  │
│          │                    │                  │          │
│  ┌───────▼────────────────────▼──────────────────▼───────┐  │
│  │                   MIDI Graph                          │  │
│  │  (Processing nodes: router, monitor, transform, FX)   │  │
│  └───────┬────────────────────┬──────────────────┬───────┘  │
│          │                    │                  │          │
│  ┌───────▼────┐     ┌────────▼───────┐  ┌───────▼───────┐  │
│  │  Recording │     │   Playback     │  │  MPE Engine   │  │
│  │  Buffer    │     │   Engine       │  │               │  │
│  └────────────┘     └────────────────┘  └───────────────┘  │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              MIDI Learn / Mapping                     │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────┘
```

---

## 2. MIDI Device Abstraction

### 2.1. Driver Architecture

```
┌──────────────────────────────────────────────┐
│              MidiDriver (Abstract)             │
│  +open() → Result                             │
│  +close() → Result                            │
│  +send(event) → void                          │
│  +on_received(callback) → void                │
│                                               │
│  Callback (from driver to MIDI Engine):       │
│  on_received(MidiEvent, timestamp_us)         │
└──────────────┬────────────────────────────────┘
               │
      ┌────────┼────────┐
      ▼        ▼        ▼
┌──────────┐ ┌──────┐ ┌──────────┐
│ WinMM    │ │Core  │ │ ALSA     │
│ MIDI     │ │MIDI  │ │ Sequencer│
└──────────┘ └──────┘ └──────────┘
┌──────────┐ ┌──────────────┐
│USB MIDI  │ │ Virtual MIDI │
│(class)   │ │ Ports        │
└──────────┘ └──────────────┘
```

### 2.2. Virtual MIDI Ports

ARIA creates virtual MIDI ports for internal routing:

```
ARIA MIDI Out  →  ARIA Virtual Port 1  →  Any application
ARIA MIDI In   ←  ARIA Virtual Port 2  ←  Any application
```

This allows ARIA to send/receive MIDI to/from other applications without physical MIDI cables.

### 2.3. Device Discovery

```cpp
struct MidiDeviceInfo {
    std::string id;
    std::string name;
    MidiDeviceDirection direction;  // Input, Output, Duplex
    MidiDeviceType type;            // Physical, Virtual, USB
    bool is_default;
};

class MidiDriver {
    virtual std::vector<MidiDeviceInfo> enumerate_devices() = 0;
    virtual Result open(const MidiDeviceInfo& device) = 0;
    virtual void close() = 0;
    virtual void send(const MidiEvent& event) = 0;
    virtual void set_receive_callback(
        std::function<void(const MidiEvent&, uint64_t timestamp_us)>) = 0;
};
```

---

## 3. MIDI Data Model

### 3.1. Core Event Types

```cpp
enum class MidiMessageType : uint8_t {
    NoteOff          = 0x80,
    NoteOn           = 0x90,
    PolyphonicKey    = 0xA0,  // Polyphonic Aftertouch
    ControlChange    = 0xB0,
    ProgramChange    = 0xC0,
    ChannelAftertouch= 0xD0,
    PitchBend        = 0xE0,
    SysEx            = 0xF0,
    MidiClock        = 0xF8,
    MidiStart        = 0xFA,
    MidiContinue     = 0xFB,
    MidiStop         = 0xFC,
    ActiveSensing    = 0xFE,
    Meta             = 0xFF   // Internal meta-events
};

struct MidiEvent {
    MidiMessageType type;
    uint8_t channel;        // 0-15
    uint8_t data1;          // Note number, CC number, etc.
    uint8_t data2;          // Velocity, CC value, etc.
    std::vector<uint8_t> sysex_data;  // For SysEx messages
    uint64_t timestamp_us;  // Microsecond timestamp (for recording)
    uint32_t ppqn_position; // PPQN position (for sequencing)

    // Utility
    bool is_note() const;
    bool is_cc() const;
    bool is_channel_message() const;
};
```

### 3.2. Note State

```cpp
struct NoteState {
    uint8_t note;           // 0-127
    uint8_t channel;        // 0-15
    uint8_t velocity;       // 0-127
    uint8_t release_velocity;
    uint64_t start_sample;  // Sample-accurate start
    uint64_t duration;      // In samples
    bool active;
    MPEState mpe;           // Per-note MPE data
};

class NoteTracker {
public:
    void note_on(const MidiEvent& event, uint64_t sample_position);
    void note_off(const MidiEvent& event, uint64_t sample_position);
    void all_notes_off();
    void sustain(bool pedal_down);

    const NoteState* active_note(uint8_t note, uint8_t channel) const;
    const std::vector<NoteState>& active_notes() const;

private:
    std::unordered_map<uint16_t, NoteState> active_notes_;
    // Key: (note << 8) | channel
};
```

### 3.3. MIDI Clip Data

```cpp
struct MidiNote {
    uint32_t start_ppqn;    // Start position in PPQN
    uint32_t duration_ppqn; // Duration in PPQN
    uint8_t note;           // MIDI note number (0-127)
    uint8_t velocity;       // 0-127
    uint8_t channel;        // 0-15
    uint8_t release_velocity;
    MPEExpression mpe;      // Per-note MPE data
    std::vector<MidiEvent> note_events; // CC events within this note
};

class MidiClip {
public:
    // Metadata
    uint32_t length_ppqn;   // Clip length in PPQN
    uint32_t loop_start_ppqn;
    uint32_t loop_end_ppqn;

    // Notes
    void add_note(const MidiNote& note);
    void remove_note(uint32_t index);
    void clear();
    const std::vector<MidiNote>& notes() const;

    // Events (CC, pitch bend, etc.)
    void add_event(const MidiEvent& event);
    const std::vector<MidiEvent>& events() const;

    // Quantization
    void quantize(uint32_t grid_ppqn, double strength);
    void humanize(double timing_amount, double velocity_amount);

    // Transpose
    void transpose(int8_t semitones);

    // Serialization
    std::vector<uint8_t> serialize() const;
    static MidiClip deserialize(const std::vector<uint8_t>& data);
};
```

---

## 4. MIDI Graph

### 4.1. Graph Architecture

Similar to the audio graph, the MIDI graph is a DAG of MIDI processing nodes:

```cpp
class MidiNode {
public:
    struct ProcessContext {
        uint32_t frames;
        double sample_rate;
        uint64_t sample_position;
        const MidiEventList* input_events;
        MidiEventList* output_events;
    };

    virtual void process(ProcessContext& ctx) = 0;
    virtual void reset() {}
};
```

### 4.2. Node Types

| Node | Purpose |
|---|---|
| `MidiInputNode` | Hardware MIDI input |
| `MidiOutputNode` | Hardware MIDI output |
| `MidiRouterNode` | Route MIDI to multiple destinations |
| `MidiMonitorNode` | Echo MIDI input to track |
| `MidiTransformerNode` | Transpose, velocity, channel mapping |
| `MidiArpeggiatorNode` | Arpeggiator |
| `MidiChorderNode` | Chord generator |
| `MidiQuantizerNode` | Real-time quantizer |
| `MidiRecorderNode` | Capture MIDI input to clip |
| `MidiPlaybackNode` | Play MIDI clip data |
| `MidiFxNode` | MIDI effect plugin host |

### 4.3. MIDI Processing Order

```
MidiInputNode (hardware)
    │
    ▼
MidiMonitorNode (echo to armed track)
    │
    ▼
MidiRouterNode
    │
    ├──► MidiRecorderNode → Clip (if recording)
    │
    ├──► MidiTransformerNode (transpose/velocity)
    │       │
    │       ▼
    │   MidiArpeggiatorNode
    │       │
    │       ▼
    │   MidiChorderNode
    │       │
    │       ▼
    │   MidiFxNode (third-party MIDI plugins)
    │       │
    │       ▼
    │   Instrument Plugin (VST3/CLAP)
    │       │
    │       ▼
    │   Audio Graph
    │
    └──► MidiOutputNode (hardware thru)
```

---

## 5. MIDI Clock & Sync

### 5.1. Clock Generation

ARIA can act as a MIDI clock master or slave:

```cpp
class MidiClock {
public:
    enum class Role {
        Internal,   // ARIA is the clock master
        Slave,      // ARIA follows external clock
        Off         // No clock sync
    };

    // Start/stop clock transmission
    void start();
    void stop();
    void set_role(Role role);

    // Called from audio thread at project BPM
    void process(uint32_t frames, double bpm);

    // Callbacks for clock events
    std::function<void()> on_tick;
    std::function<void()> on_beat;
    std::function<void()> on_bar;

    // Timing accuracy
    double ppm_error() const;  // Parts per million deviation

private:
    Role role_ = Role::Internal;
    uint32_t tick_counter_ = 0;
    // MIDI clock: 24 ticks per quarter note
    static constexpr uint32_t TICKS_PER_QN = 24;
};
```

### 5.2. Clock Types

| Clock Type | Ticks per Quarter | Usage |
|---|---|---|
| MIDI Clock | 24 | Standard MIDI sync |
| PPQN (Internal) | 960 | ARIA internal resolution |
| MTC (MIDI Time Code) | N/A | SMPTE sync for video |
| Ableton Link | N/A | Network sync |

### 5.3. Sync Behavior

```
ARIA as Master:
    Audio Clock (TTL) → MIDI Clock Out → External Devices

ARIA as Slave:
    External MIDI Clock In → Audio Clock (TTL)
    
    If external clock stops:
        → Pause transport
        → Resume when clock resumes (catch up or stay)

Ableton Link:
    Network sync → Tempo detection → Audio Clock
```

---

## 6. MIDI Recording & Playback

### 6.1. Recording Pipeline

```
User arms track and presses Record
    │
    ▼
MIDI Input → Timestamped Event Buffer
    │
    ▼
NoteTracker allocates note start
    │
    ▼
NoteOff received → Note completed
    │
    ├──► Calculate duration from timestamps
    ├──► Append MidiNote to recording clip
    └──► Update NoteTracker
    │
    ▼
User stops recording
    │
    ├──► All pending NoteOns get NoteOff at stop position
    ├──► Finalize clip
    └──► Post-process (quantize if enabled)
```

### 6.2. Timestamp Accuracy

MIDI events are timestamped in microseconds at the driver level:

```cpp
// On Windows: timeGetTime() with 1ms resolution + interpolation
// On macOS: mach_absolute_time() with nanosecond resolution
// On Linux: clock_gettime(CLOCK_MONOTONIC)
//
// The timestamp is converted to sample position at recording time:
uint64_t timestamp_to_samples(uint64_t timestamp_us, double sample_rate, uint64_t base_sample) {
    return base_sample + (uint64_t)(timestamp_us * sample_rate / 1'000'000.0);
}
```

### 6.3. Playback Pipeline

```
Transport starts at position X
    │
    ▼
For each track with MIDI clips:
    │
    ├──► Find clip at position X
    │
    ├──► If clip is playing:
    │       ├──► Calculate read position within clip
    │       ├──► For each note in range:
    │       │       ├──► Generate NoteOn at sample position
    │       │       ├──► Generate NoteOff at (position + duration)
    │       │       └──► Generate CC events at correct positions
    │       │
    │       ├──► For each CC event in range:
    │       │       └──► Generate at correct position
    │       │
    │       └──► Send to MIDI Graph → Instrument → Audio
    │
    └──► If no clip: skip track
```

---

## 7. MPE (MIDI Polyphonic Expression)

### 7.1. MPE Data Model

MPE enables per-note expression: pitch bend, timbre, and pressure for each note independently.

```cpp
struct MPEExpression {
    // Per-note expression (MPE)
    int16_t pitch_bend;         // -8192 to 8191
    uint8_t timbre;             // CC 74 (0-127)
    uint8_t pressure;           // Channel pressure (0-127)

    bool has_data() const {
        return pitch_bend != 0 || timbre != 64 || pressure != 0;
    }
};

struct MPEState {
    // MPE configuration
    uint8_t member_channel;      // 2-15 (each note gets a channel)
    MPEExpression expression;

    // Zone configuration
    static constexpr uint8_t LOWER_ZONE = 0;  // Channel 0
    static constexpr uint8_t UPPER_ZONE_START = 2;  // Channels 2-15
    static constexpr uint8_t MASTER_CHANNEL = 1;  // Channel 1
};
```

### 7.2. MPE Encoding/Decoding

```cpp
class MPEDecoder {
public:
    // Input: standard MIDI CC/pitch bend events
    // Output: per-note MPE data
    void process(const MidiEvent& event);

    // Access per-note expression
    const MPEExpression* note_expression(uint8_t note) const;

private:
    // MPE zone: channels 2-15 are per-note
    std::unordered_map<uint8_t, MPEExpression> note_expressions_;
    // Channel → Note mapping
    uint8_t channel_to_note_[16];
};

class MPEEncoder {
public:
    // Input: per-note expression changes
    // Output: standard MIDI CC/pitch bend on member channels
    void set_note_expression(uint8_t note, const MPEExpression& expr);
    void get_events(std::vector<MidiEvent>& output);

private:
    // Zone assignment
    uint8_t assign_channel(uint8_t note);
    void release_channel(uint8_t note);
};
```

### 7.3. MPE Workflow in ARIA

```
MPE Controller (e.g., Roli Seaboard)
    │
    ▼
MIDI Input: channel 0-15 MPE events
    │
    ▼
MPEDecoder: map to per-note data
    │
    ▼
Piano Roll Display: per-note expression lanes
    │
    ▼
Note editing: per-note pitch/timbre/pressure curves
    │
    ▼
MPEEncoder: convert to channelized MIDI
    │
    ▼
MPE-compatible instrument plugin
```

---

## 8. MIDI Processing & Transformation

### 8.1. Built-in MIDI Processors

| Processor | Description | Parameters |
|---|---|---|
| **Transpose** | Shift notes by semitones | Amount (-24 to +24), Scale-aware |
| **Velocity** | Remap velocity curve | Curve (linear/exp/log), Min/Max, Randomize |
| **Channel Map** | Redirect MIDI channels | Source → Target channel mapping |
| **Note Filter** | Filter note range | Low note, High note, Mode (pass/block) |
| **CC Remap** | Remap MIDI CC numbers | Source → Target CC, Value scale |
| **Arpeggiator** | Generate arpeggio patterns | Rate, Pattern, Octave range, Gate, Swing |
| **Chord Generator** | Add harmony to single notes | Chord type, Voicing, Inversion |
| **Quantizer** | Snap timing to grid | Grid resolution, Strength, Swing |
| **Humanizer** | Add timing/velocity variation | Timing amount, Velocity amount, Random seed |
| **Scale Filter** | Snap notes to scale | Scale (major/minor/etc.), Root note |
| **Randomizer** | Generate random MIDI | Probability, Parameter targets |

### 8.2. Arpeggiator Architecture

```cpp
class ArpeggiatorNode : public MidiNode {
public:
    enum class Pattern {
        Up, Down, UpDown, DownUp, Random,
        Chord, AsPlayed
    };

    void process(ProcessContext& ctx) override {
        // Receive held notes (via sustain tracking)
        // Generate arpeggiated sequence from held notes
        // Apply pattern, rate, gate, octave range
        // Output generated notes
    }

private:
    Pattern pattern_ = Pattern::Up;
    double rate_ = 1.0 / 16;  // Sixteenth notes
    double gate_ = 0.75;       // Gate length
    uint32_t octave_range_ = 2;
    double swing_ = 0.0;
    std::vector<uint8_t> held_notes_;
};
```

### 8.3. Real-Time vs Offline Processing

| Processor | Real-Time | Offline (Clip) |
|---|---|---|
| Transpose | Apply at playback | Modify clip data |
| Velocity | - | Modify clip data |
| Arpeggiator | Live generation | Generate clip data |
| Chord Generator | Live generation | Generate clip data |
| Quantizer | Live snap (input) | Post-process clip |
| Humanizer | - | Post-process clip |
| Scale Filter | Live snap (input) | Post-process clip |

---

## 9. MIDI Learn & Mapping

### 9.1. Mapping Architecture

```cpp
struct MidiMapping {
    // Source
    MidiMessageType source_type;     // CC, Note, PitchBend, etc.
    uint8_t source_channel;          // 0-15 (any = 255)
    uint8_t source_number;           // CC number, note number, etc.

    // Target
    std::string target_id;           // "track.1.volume", "plugin.3.cutoff"
    double min_value;
    double max_value;

    // Mapping
    MappingCurve curve;              // Linear, Exponential, Logarithmic
    bool relative;                   // Relative mode (encoders)
    uint8_t relative_mode;           // Signed bit, 2's complement, etc.

    // State
    double current_value;
};

class MidiLearnManager {
public:
    // Start/stop MIDI learn mode
    void start_learn();
    void stop_learn();
    bool is_learning() const;

    // Called when user clicks a parameter
    void set_learn_target(const std::string& parameter_id);

    // Called on next MIDI input
    void on_midi_event(const MidiEvent& event);

    // Active mappings
    const std::vector<MidiMapping>& mappings() const;
    void add_mapping(const MidiMapping& mapping);
    void remove_mapping(uint32_t index);

    // Process incoming MIDI → apply to targets
    void process_midi_for_mapping(const MidiEvent& event);

private:
    bool learning_ = false;
    std::string learn_target_;
    std::vector<MidiMapping> mappings_;
};
```

### 9.2. Learn Workflow

```
1. User clicks a parameter (e.g., cutoff knob)
2. MIDI Engine enters "Learn" mode
    - UI shows: "Move a hardware controller..."
3. User moves a hardware knob/fader
4. MIDI Engine receives the event
    - Matches source type, channel, number
5. Mapping is created
6. UI shows: "Cutoff → MIDI CC 74 [Channel 1]"
7. User can edit: min/max, curve, relative mode
8. Mapping is saved in project file
```

### 9.3. Relative Mode

For endless rotary encoders (not pots), relative mode interprets movement:

```cpp
enum class RelativeMode {
    Absolute,           // Standard 0-127
    SignedBit,          // 0x41 (down) / 0x3F (up)  — most common
    TwoComplement,      // 0x7F (down) / 0x01 (up)
    BinaryOffset        // 0x00-0x3F (down) / 0x40-0x7F (up)
};
```

---

## 10. Timing & Jitter Management

### 10.1. Jitter Sources

| Source | Typical Jitter | Mitigation |
|---|---|---|
| USB MIDI | ±1-5ms | Timestamp at driver level |
| Classic MIDI DIN | ±0.5ms | Negligible |
| Virtual MIDI | ±0.1ms | Precise timestamps |
| OS scheduling | ±1-15ms | High-priority MIDI thread |

### 10.2. Recording Compensation

When recording MIDI, the engine compensates for known jitter patterns:

```cpp
class MidiJitterCompensator {
public:
    // Record an event with its raw timestamp
    void record_event(const MidiEvent& event, uint64_t raw_timestamp_us);

    // Get compensated sequence
    void get_compensated_events(std::vector<MidiNote>& notes);

    // Calibration
    void calibrate(const MidiDeviceInfo& device);

private:
    // Per-device latency
    uint32_t device_latency_us_ = 0;

    // Running average of timing offset
    double running_average_us_ = 0.0;
    uint32_t sample_count_ = 0;
};
```

### 10.3. Playback Timing

MIDI playback events are scheduled sample-accurately:

```cpp
// Each note's NoteOn is scheduled at an exact sample position
// within the audio buffer, not at the buffer boundary.
//
// Example: at 48kHz/128 buffer:
//   Note at 1.5 beats (bar 1, beat 1.5)
//   PPQN position: 1*960*4 + 0.5*960 = 4320
//   Sample position: (1.5 / 4.0) * (60.0 / 120) * 48000
//                   = 9000 samples from beat 1.0
```

The audio graph handles sample-accurate event insertion:

```cpp
void InstrumentNode::process(ProcessContext& ctx) {
    // For each MIDI event in the current buffer window:
    for (const auto& event : ctx.input_events) {
        uint32_t event_offset = event.sample_position - ctx.start_sample;
        if (event_offset < ctx.frames) {
            // Insert event at exact sample offset
            event_list_.insert(event_offset, event);
        }
    }
    // Process audio with events at correct sample positions
    process_with_events(ctx, event_list_);
}
```

---

## 11. API Reference

### 11.1. Public API

```cpp
class MidiEngine {
public:
    // Lifecycle
    Result init(const MidiEngineConfig& config);
    void shutdown();

    // Device management
    std::vector<MidiDeviceInfo> enumerate_devices();
    Result open_input_device(const MidiDeviceInfo& device);
    Result open_output_device(const MidiDeviceInfo& device);
    Result open_virtual_port(const std::string& name);
    void close_all_devices();

    // Send MIDI
    void send_event(const MidiEvent& event);
    void send_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
    void send_note_off(uint8_t channel, uint8_t note, uint8_t velocity);
    void send_cc(uint8_t channel, uint8_t cc, uint8_t value);
    void all_notes_off();
    void panic();                       // Reset all MIDI state

    // Recording
    void start_recording(track_id_t track);
    void stop_recording(track_id_t track);
    MidiClipPtr recorded_clip(track_id_t track) const;

    // MIDI Clock
    MidiClock& clock();
    void set_clock_role(MidiClock::Role role);

    // MIDI Learn
    MidiLearnManager& learn_manager();

    // Graph
    MidiGraph& graph();
    const MidiGraph& graph() const;

    // Note tracking
    const NoteTracker& note_tracker() const;

    // Diagnostics
    uint64_t events_received() const;
    uint64_t events_sent() const;
    uint32_t active_note_count() const;
};
```

### 11.2. Configuration

```cpp
struct MidiEngineConfig {
    uint32_t ppqn = 960;               // Internal PPQN resolution
    bool enable_thru = true;           // MIDI thru by default
    bool enable_virtual_ports = true;  // Create virtual MIDI ports
    bool enable_mpe = true;            // Enable MPE support
    JitterCompensation jitter = JitterCompensation::Automatic;
    LogLevel log_level = LogLevel::Warn;
};
```

---

## 12. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-ME-001 | MIDI Device Abstraction | 🔲 Pending |
| RFC-ME-002 | Virtual MIDI Ports | 🔲 Pending |
| RFC-ME-003 | MIDI Data Model & Event Types | 🔲 Pending |
| RFC-ME-004 | MIDI Graph Topology | 🔲 Pending |
| RFC-ME-005 | MIDI Clock Generation & Sync | 🔲 Pending |
| RFC-ME-006 | Sample-Accurate MIDI Recording | 🔲 Pending |
| RFC-ME-007 | MIDI Playback Scheduling | 🔲 Pending |
| RFC-ME-008 | MPE Decoder/Encoder | 🔲 Pending |
| RFC-ME-009 | Arpeggiator Implementation | 🔲 Pending |
| RFC-ME-010 | Chord Generator | 🔲 Pending |
| RFC-ME-011 | MIDI Learn & Mapping System | 🔲 Pending |
| RFC-ME-012 | Jitter Compensation | 🔲 Pending |
| RFC-ME-013 | MIDI Transform Nodes | 🔲 Pending |
| RFC-ME-014 | MIDI Automation Recording | 🔲 Pending |
| RFC-ME-015 | MIDI 2.0 Support (Future) | 🔲 Pending |

---

## Appendix A: MIDI 2.0 Roadmap

MIDI 2.0 support is planned for a future version (v2.0+). The architecture is designed to accommodate it:

| MIDI 2.0 Feature | Impact | Planned |
|---|---|---|
| 32-bit resolution (vs 7/14-bit) | Expand data types in MidiEvent | v2.0 |
| Per-note controllers (native) | Merges with MPE system | v2.0 |
| MIDI-CI (Capability Inquiry) | New discovery protocol | v2.0 |
| Property Exchange | New data protocol | v2.0 |
| UMP (Universal MIDI Packet) | New transport layer | v2.0 |

## Appendix B: Performance Targets

| Scenario | CPU | Latency |
|---|---|---|
| 16-channel MIDI recording | < 1% | < 1ms input-to-output |
| Clock master at 140 BPM | < 0.5% | < 0.1 BPM deviation |
| Arpeggiator on 8 notes | < 1% | < 0.5ms |
| MPE controller, 10 notes | < 2% | < 1ms |
| 100 simultaneous MIDI tracks | < 10% | n/a |
