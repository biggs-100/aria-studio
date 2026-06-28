# ARIA DAW — Automation Engine Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23
> **Real-Time Safety**: Audio parameter reads are lock-free atomic

---

## Table of Contents

1. [Overview](#1-overview)
2. [Data Model](#2-data-model)
3. [Automation Clips](#3-automation-clips)
4. [Envelope System](#4-envelope-system)
5. [Bezier Curves](#5-bezier-curves)
6. [Modulation Sources](#6-modulation-sources)
7. [Modulation Mapping](#7-modulation-mapping)
8. [Automation Lanes](#8-automation-lanes)
9. [Recording Automation](#9-recording-automation)
10. [Real-Time Parameter Access](#10-real-time-parameter-access)
11. [Automation within Piano Roll](#11-automation-within-piano-roll)
12. [Nested Modulation](#12-nested-modulation)
13. [API Reference](#13-api-reference)
14. [RFC Index](#14-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Automation Engine is responsible for all parameter automation, modulation, and expression in ARIA DAW. It combines the best of Bitwig's deep modulation ecosystem with Ableton's expressive automation curves, adding ARIA's own innovations in nested modulation and per-note automation.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Automation resolution | Sample-accurate |
| Modulation sources | Unlimited per parameter |
| Modulation chain depth | 8 levels of nested modulation |
| Curve types | 8 interpolation modes |
| Automation parameters | Unlimited per project |
| Real-time safety | Lock-free reads on audio thread |

### 1.3. Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Automation Engine                            │
│                                                                      │
│  ┌─────────────────────┐  ┌──────────────────┐  ┌────────────────┐  │
│  │   Automation Clip   │  │Modulation Sources │  │ Automation     │  │
│  │   Manager           │  │Manager            │  │ Lane Manager   │  │
│  └─────────┬───────────┘  └────────┬─────────┘  └───────┬────────┘  │
│            │                       │                     │          │
│  ┌─────────▼───────────────────────▼─────────────────────▼────────┐ │
│  │                    Modulation Graph                              │ │
│  │  (DAG of sources → modulators → targets, evaluated per-buffer)  │ │
│  └────────────────────────────────┬────────────────────────────────┘ │
│                                   │                                  │
│  ┌───────────────────────────────▼────────────────────────────────┐  │
│  │                  Parameter Cache (double-buffered)              │  │
│  │  [parameter_id → current_value] - atomic reads on audio thread │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2. Data Model

### 2.1. Core Types

```cpp
// Automation value is always a normalized double 0.0 - 1.0
// The target system (e.g., audio engine) maps to its native range

struct AutomationPoint {
    uint64_t ppqn_position;
    double value;                // 0.0 - 1.0
    InterpolationType interpolation;  // How to reach next point
    BezierControl bezier;        // Control points for bezier mode

    // Convenience
    bool has_bezier() const;
};

struct BezierControl {
    double x1, y1;  // Control point 1 (normalized 0-1 within segment)
    double x2, y2;  // Control point 2 (normalized 0-1 within segment)
};

enum class InterpolationType {
    Hold,           // No interpolation (square wave)
    Linear,         // Straight line
    Bezier,         // Cubic bezier curve
    Smooth,         // Catmull-Rom spline (smooth through points)
    EaseIn,         // Quadratic ease in
    EaseOut,        // Quadratic ease out
    EaseInOut,      // Smooth ease
    Exponential,    // Exponential curve
    Logarithmic,    // Logarithmic curve
    SCurve,         // S-curve (sigmoid)
};
```

### 2.2. Automation Clip

```cpp
class AutomationClip {
public:
    // Metadata
    void set_id(AutomationClipID id);
    AutomationClipID id() const;
    void set_name(const std::string& name);
    std::string name() const;
    void set_color(const Color& color);
    Color color() const;

    // Duration
    void set_length(uint64_t ppqn);
    uint64_t length() const;
    void set_loop(bool enabled);
    bool loop_enabled() const;
    void set_loop_range(uint64_t start_ppqn, uint64_t end_ppqn);

    // Points
    void add_point(const AutomationPoint& point);
    void remove_point(uint64_t ppqn);
    void move_point(uint64_t from_ppqn, uint64_t to_ppqn, double value);
    void clear_points();
    size_t point_count() const;

    // Query
    double evaluate(uint64_t ppqn) const;
    AutomationPoint* find_point(uint64_t ppqn);
    const AutomationPoint* find_point(uint64_t ppqn) const;
    std::vector<AutomationPoint> points_in_range(uint64_t start, uint64_t end) const;

    // Bulk operations
    void quantize_points(uint32_t grid_ppqn);
    void offset_values(double amount);   // Add constant to all values
    void scale_values(double factor);     // Multiply all values

    // Conversion to/from GPU-friendly representation
    std::vector<Vec2> generate_line_strip(uint64_t start_ppqn,
                                          uint64_t end_ppqn,
                                          double resolution_ppqn) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static AutomationClip deserialize(const std::vector<uint8_t>& data);

private:
    AutomationClipID id_;
    std::string name_;
    Color color_ = {0xFF, 0x7A, 0x00};  // Default ARIA orange

    uint64_t length_ppqn_ = 960 * 16;       // 4 bars default
    bool loop_enabled_ = false;
    uint64_t loop_start_ppqn_ = 0;
    uint64_t loop_end_ppqn_ = 960 * 16;

    // Points are kept sorted by ppqn_position for efficient evaluation
    std::vector<AutomationPoint> points_;
    mutable std::vector<AutomationPoint> cached_line_strip_;
    mutable bool cache_dirty_ = true;
};
```

### 2.3. Parameter Binding

```cpp
struct ParameterBinding {
    // Target
    ParameterID target_id;       // Unique parameter identifier
    
    // Optional range mapping (target may not use 0.0-1.0)
    double target_min = 0.0;
    double target_max = 1.0;

    // Value conversion
    double normalized_to_target(double normalized_value) const;
    double target_to_normalized(double target_value) const;
};

enum class ParameterDomain {
    Continuous,     // Any value in range (e.g., filter cutoff)
    Discrete,       // Integer steps (e.g., waveform selector)
    Boolean,        // On/off (e.g., bypass)
    Enum            // Named values (e.g., filter type)
};

struct ParameterInfo {
    ParameterID id;
    std::string name;
    std::string unit;            // "Hz", "dB", "%", etc.
    ParameterDomain domain;
    double min_value;
    double max_value;
    double default_value;
    std::vector<std::string> enum_values;  // For enum domain
};
```

---

## 3. Automation Clips

### 3.1. Clip Types

| Clip Type | Description | Use Case |
|---|---|---|
| **Standard** | Points + interpolation curves | Volume, pan, send levels |
| **Expression** | Continuous expression data | Pitch bend, CC, MPE |
| **Step** | Hold-type stepped values | Step sequencer-style automation |
| **LFO** | LFO waveform with parameters | Cyclic modulation (pulse width, etc.) |
| **Envelope** | ADSR/HADSR envelope | Synthesizer-style envelope shapes |
| **Audio Follow** | Tracks audio amplitude | Sidechain-style ducking |
| **Scripted** | Lua-generated automation | Custom mathematical curves |

### 3.2. Step Automation Clip

```cpp
class StepAutomationClip : public AutomationClip {
public:
    void set_step_count(uint32_t steps);      // e.g., 16 steps
    void set_step_value(uint32_t step, double value);
    double step_value(uint32_t step) const;
    void set_smooth(bool smooth);              // Linear interpolation between steps
    void set_swing(double amount);             // Swing per step

    // Override: steps generate discrete points
    double evaluate(uint64_t ppqn) const override;
    
private:
    uint32_t step_count_ = 16;
    std::vector<double> step_values_;
    bool smooth_ = false;
    double swing_ = 0.0;
};
```

### 3.3. LFO Automation Clip

```cpp
class LFOAutomationClip : public AutomationClip {
public:
    enum class Waveform {
        Sine, Triangle, Saw, SawInv, Square,
        SampleAndHold, Noise, RampUp, RampDown
    };

    void set_waveform(Waveform waveform);
    Waveform waveform() const;
    void set_rate(double rate_hz);          // LFO rate in Hz
    void set_rate_sync(double rate_notes);  // LFO rate synced to note division
    bool is_rate_synced() const;
    void set_phase(double phase);           // Initial phase 0.0-1.0
    void set_pulse_width(double width);     // 0.0-1.0 (for square/pulse)
    void set_offset(double offset);         // DC offset 0.0-1.0
    void set_bipolar(bool bipolar);         // -1.0 to 1.0 (vs 0.0-1.0)

    double evaluate(uint64_t ppqn) const override;

private:
    Waveform waveform_ = Waveform::Sine;
    double rate_hz_ = 1.0;
    double rate_note_ = 1.0 / 4;           // Quarter note
    bool rate_synced_ = true;
    double phase_ = 0.0;
    double pulse_width_ = 0.5;
    double offset_ = 0.5;
    bool bipolar_ = false;
};
```

---

## 4. Envelope System

### 4.1. Envelope Shapes

```cpp
class EnvelopeClip : public AutomationClip {
public:
    enum class EnvelopeType {
        ADSR,       // Attack-Decay-Sustain-Release
        AHDSR,      // Attack-Hold-Decay-Sustain-Release
        AD,         // Attack-Decay (no sustain)
        AR,         // Attack-Release
        DADSR       // Delay-Attack-Decay-Sustain-Release
    };

    void set_type(EnvelopeType type);
    EnvelopeType type() const;

    // Times in PPQN or milliseconds
    void set_delay(double time_ms);
    void set_attack(double time_ms);
    void set_hold(double time_ms);        // AHDSR only
    void set_decay(double time_ms);
    void set_sustain(double level);       // 0.0-1.0
    void set_release(double time_ms);

    // Curve shapes for each segment
    void set_attack_curve(InterpolationType curve);
    void set_decay_curve(InterpolationType curve);
    void set_release_curve(InterpolationType curve);

    // Evaluate with gate signal
    double evaluate_gated(uint64_t ppqn, bool gate_on, uint64_t gate_start_ppqn) const;

private:
    EnvelopeType type_ = EnvelopeType::ADSR;
    double delay_ms_ = 0;
    double attack_ms_ = 10;
    double hold_ms_ = 0;
    double decay_ms_ = 300;
    double sustain_level_ = 0.7;
    double release_ms_ = 500;

    InterpolationType attack_curve_ = InterpolationType::EaseOut;
    InterpolationType decay_curve_ = InterpolationType::EaseIn;
    InterpolationType release_curve_ = InterpolationType::EaseOut;
};
```

### 4.2. Envelope Evaluation

```
Envelope timeline (ADSR):
                
    Attack   Decay    Sustain          Release
    ──────   ──────   ───────────────  ──────
    ▲                ┌────────────┐           ▲
  1.0│              ╱│            │╲          │
     │            ╱  │            │  ╲        │
     │          ╱    │            │    ╲      │
     │        ╱      │            │      ╲    │
     │      ╱        │            │        ╲  │
     │    ╱          │            │          ╲│
  0.0│──╱────────────┘            └───────────┘
     │
     └─────────────────────────────────────────
     Key:  [Gate On]              [Gate Off]
```

---

## 5. Bezier Curves

### 5.1. Bezier Math

```cpp
class BezierEvaluator {
public:
    // Cubic bezier: P0 (start), P1, P2 (control), P3 (end)
    // P0 and P3 are the automation points
    // P1 and P2 are the control points (stored as relative offsets)
    struct CubicBezier {
        double x1, y1;  // Control point 1 (relative to segment start-end)
        double x2, y2;  // Control point 2
    };

    // Evaluate cubic bezier at parameter t (0.0 - 1.0)
    static double evaluate(double t, const CubicBezier& bezier);

    // Evaluate at a specific position along the segment
    // segment_start_value, segment_end_value = value at endpoints
    // t = position within segment (0.0-1.0)
    static double evaluate_segment(double t,
                                    double segment_start_value,
                                    double segment_end_value,
                                    const CubicBezier& bezier);

    // Convert from user-editable control points to canonical form
    static CubicBezier from_user_controls(double cx1, double cy1,
                                           double cx2, double cy2);

    // Visual preview generation
    static std::vector<Vec2> generate_preview(
        const CubicBezier& bezier, uint32_t segments = 32);
};
```

### 5.2. Bezier Editing

```
Automation clip with Bezier curve:
┌─────────────────────────────────────────┐
│ Automation: Filter Cutoff               │
│                                         │
│  ┌──┐                                   │
│  │  │╲         ┌──┐                    │
│  │  │ ╲       ╱  ╲ ┌──┐               │
│  │  │  ╲     ╱    ╲│  │╲             │
│  │  │   ╲   ╱      ╲  │ ╲           │
│  │  │    ╲ ╱        ╲ │  ╲   ┌──┐  │
│  │  │     ╲          ──│   ╲ ╱  │  │  │
│  │  │     │           │    ╲   ╱   │  │
│  │  └──────┘           └────┘ └────┘  │
│  └─────────────────────────────────────┘
│  Points: ● Active  ○ Selected  △ Hover │
│                                         │
│  [▸ Point 1: (2.1.0, 0.3)]             │
│  [▸ Control 1: (+0.2, +0.4)]           │
│  [▸ Control 2: (+0.8, +0.1)]           │
│  [▸ Interpolation: Bezier ▼]           │
└─────────────────────────────────────────┘
```

### 5.3. Preset Curve Shapes

```cpp
class CurvePresets {
public:
    // Standard curve shapes
    static BezierEvaluator::CubicBezier linear();
    static BezierEvaluator::CubicBezier ease_in();
    static BezierEvaluator::CubicBezier ease_out();
    static BezierEvaluator::CubicBezier ease_in_out();
    static BezierEvaluator::CubicBezier slow_in_fast_out();
    static BezierEvaluator::CubicBezier fast_in_slow_out();
    static BezierEvaluator::CubicBezier anticipation();   // Dip before rise
    static BezierEvaluator::CubicBezier overshoot();       // Overshoot then settle
    static BezierEvaluator::CubicBezier bounce();          // Bounce at endpoint
};
```

---

## 6. Modulation Sources

### 6.1. Source Types

```cpp
class ModulationSource {
public:
    enum class SourceType {
        LFO,               // Low-frequency oscillator
        EnvelopeFollower,  // Audio amplitude follower
        StepSequencer,     // Step sequencer pattern
        Random,            // Sample & hold / random walk
        Macro,             // User-assigned macro (knob)
        NoteProperty,      // Note velocity, pitch, pressure
        MPE,               // MPE input (per-note)
        MIDICC,            // MIDI CC input
        AudioSidechain,    // Sidechain audio input
        Scripted,          // Lua script
        Formula,           // Mathematical expression
        Nested             // Output of another modulation chain
    };

    SourceType type() const;
    virtual double evaluate(uint64_t sample_position,
                            const ModulationContext& ctx) const = 0;
    virtual void reset() = 0;
};

struct ModulationContext {
    double current_note_velocity;   // For note-property sources
    double current_note_pitch;
    double sidechain_amplitude;     // For audio sidechain
    MPEExpression current_mpe;
    const AutomationEngine* engine; // For nested modulation
};
```

### 6.2. LFO Source

```cpp
class LFOSource : public ModulationSource {
public:
    enum class Waveform {
        Sine, Triangle, Saw, SawInv, Square, SampleAndHold, Noise,
        RampUp, RampDown, Pulse,   // With variable width
        Custom                     // User-defined wavetable
    };

    // Rate
    void set_rate_sync(double note_value);  // Note-synced (1/4, 1/8, etc.)
    void set_rate_hz(double hz);            // Free-running Hz
    bool is_rate_synced() const;

    // Shape
    void set_waveform(Waveform wf);
    Waveform waveform() const;
    void set_pulse_width(double width);     // 0.0-1.0 for pulse/square
    void set_phase(double start_phase);     // 0.0-1.0 initial phase
    void set_bipolar(bool bipolar);         // -1 to 1 output
    void set_smooth(double ms);             // Smoothing time (for S&H)

    // Sync
    void set_key_sync(bool sync);           // Retrigger on note-on
    void set_tempo_sync(bool sync);         // Sync to project tempo

    double evaluate(uint64_t sample_position,
                    const ModulationContext& ctx) const override;

private:
    Waveform waveform_ = Waveform::Sine;
    double rate_hz_ = 1.0;
    double rate_note_ = 1.0 / 4;
    bool rate_synced_ = true;
    double pulse_width_ = 0.5;
    double phase_ = 0.0;
    bool bipolar_ = false;
    bool key_sync_ = false;
    bool tempo_sync_ = true;
    double smoothing_ms_ = 0.0;
};
```

### 6.3. Envelope Follower

```cpp
class EnvelopeFollowerSource : public ModulationSource {
public:
    void set_attack_ms(double ms);     // 0-100ms
    void set_release_ms(double ms);    // 0-1000ms
    void set_source(sidechain_id_t source);  // Which audio input to follow
    
    // Output modes
    enum class OutputMode {
        Amplitude,      // Raw amplitude envelope
        Envelope,       // Smoothed envelope
        Peak,           // Peak detection
        RMS             // RMS level
    };
    void set_output_mode(OutputMode mode);

    double evaluate(uint64_t sample_position,
                    const ModulationContext& ctx) const override;

private:
    double attack_ms_ = 5.0;
    double release_ms_ = 50.0;
    sidechain_id_t source_;
    OutputMode mode_ = OutputMode::Envelope;
    
    // Internal state
    mutable double current_envelope_ = 0.0;
};
```

### 6.4. Step Sequencer Source

```cpp
class StepSequencerSource : public ModulationSource {
public:
    void set_step_count(uint32_t steps);           // 1-64 steps
    void set_step_value(uint32_t step, double value);
    double step_value(uint32_t step) const;
    void set_step_count_per_beat(uint32_t steps);  // Steps per beat
    
    void set_smooth(bool smooth);                   // Smooth transitions
    void set_swing(double amount);                  // 0.0-1.0
    void set_randomize_amount(double amount);       // 0.0-1.0 random variation
    
    // Trigger modes
    enum class Trigger {
        Free,           // Free running
        NoteOn,         // Advance on each note-on
        NoteGate,       // Reset on each note-on
        Transport       // Advance with transport
    };
    void set_trigger(Trigger trigger);

    double evaluate(uint64_t sample_position,
                    const ModulationContext& ctx) const override;

private:
    uint32_t step_count_ = 16;
    std::vector<double> step_values_;
    uint32_t steps_per_beat_ = 4;
    bool smooth_ = false;
    double swing_ = 0.0;
    double randomize_ = 0.0;
    Trigger trigger_ = Trigger::Free;
    mutable uint32_t current_step_ = 0;
};
```

### 6.5. Macro Source

```cpp
class MacroSource : public ModulationSource {
public:
    // Macros are user-assigned knobs that can modulate multiple parameters
    // They appear in the UI as a "Macro" panel
    
    void set_name(const std::string& name);
    std::string name() const;
    
    void set_value(double normalized);  // Set by user via UI
    void set_value_by_midi(uint8_t cc_value);  // Set via MIDI learn
    double value() const;               // Current value 0.0-1.0

    // Mapping to parameters
    void add_mapping(ParameterID target, double min, double max);
    void remove_mapping(ParameterID target);
    
    double evaluate(uint64_t sample_position,
                    const ModulationContext& ctx) const override;

private:
    std::string name_;
    std::atomic<double> value_{0.5};    // Lock-free for UI read
    std::vector<MacroMapping> mappings_;
};
```

---

## 7. Modulation Mapping

### 7.1. Modulation Stack

A single parameter can have multiple modulation sources stacked on it:

```cpp
class ModulationStack {
public:
    struct ModulationEntry {
        ModulationSource* source;      // The modulation source
        double amount;                 // Modulation amount (0.0-1.0)
        double offset;                 // Offset (0.0-1.0)
        double min_clamp;              // Clamp output minimum
        double max_clamp;              // Clamp output maximum
        ModulationPolarity polarity;   // Unipolar / Bipolar
        bool bypassed;                 // Temporarily bypass
    };

    // Add modulation source to stack
    ModulationEntryID add_source(ModulationSource* source,
                                  double amount = 1.0);
    void remove_source(ModulationEntryID id);
    void set_amount(ModulationEntryID id, double amount);
    void set_bypass(ModulationEntryID id, bool bypassed);

    // Evaluate: apply all modulation sources to a base value
    double evaluate(double base_value, uint64_t sample_position,
                    const ModulationContext& ctx) const;

    // Stack order: first added = first applied
    // Subsequent modulations modulate the output of the previous
    size_t stack_depth() const;

private:
    std::vector<ModulationEntry> entries_;
};

enum class ModulationPolarity {
    Unipolar,    // 0.0 - 1.0 output
    Bipolar      // -1.0 - 1.0 output
};
```

### 7.2. Modulation Matrix

```cpp
class ModulationMatrix {
public:
    // Connect source to target with amount
    ModulationEntryID connect(ModulationSource* source,
                               ParameterID target,
                               double amount,
                               ModulationPolarity polarity);

    void disconnect(ParameterID target, ModulationEntryID entry);
    void clear_target(ParameterID target);

    // Query connections
    std::vector<ModulationEntry> connections_for_target(ParameterID target) const;
    std::vector<ModulationEntry> connections_from_source(ModulationSource* source) const;

    // Evaluate all modulations for a target
    double apply_modulations(ParameterID target,
                              double base_value,
                              uint64_t sample_position,
                              const ModulationContext& ctx) const;

    // Total connection count
    uint32_t total_connections() const;

private:
    // Target → List of modulation entries
    std::unordered_map<ParameterID, std::vector<ModulationEntry>> matrix_;
};
```

### 7.3. Modulation Visualization

```
Modulation Matrix View:
┌─────────────────────────────────────────────────────────────────┐
│ Modulation Matrix                                               │
│                                                                │
│ Sources \ Targets   Cutoff  Resonance  Volume  Pan  FX Mix    │
│ ─────────────────────────────────────────────────────────────  │
│ LFO 1               0.75    0.0        0.0     0.0  0.0      │
│ LFO 2               0.0     0.5        0.0     0.0  0.0      │
│ Env Follower         0.0     0.0        1.0     0.0  0.3      │
│ Macro 1              0.0     0.0        0.0     0.5  0.0      │
│ Step Seq              0.3     0.0        0.0     0.0  0.0      │
│ Note Velocity         0.0     0.0        0.2     0.0  0.0      │
│                                                               │
│ [Add Source ▼]                     [Add Target ▼]             │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. Automation Lanes

### 8.1. Lane Architecture

```cpp
class AutomationLane {
public:
    AutomationLaneID id() const;
    void set_name(const std::string& name);
    std::string name() const;
    void set_color(const Color& color);
    Color color() const;

    // Target parameter
    void set_target(ParameterID parameter);
    ParameterID target() const;

    // The automation clip in this lane
    void set_clip(std::shared_ptr<AutomationClip> clip);
    std::shared_ptr<AutomationClip> clip() const;
    bool has_clip() const;

    // Arm for recording
    void set_armed(bool armed);
    bool is_armed() const;

    // Bypass
    void set_bypass(bool bypass);
    bool is_bypassed() const;
    bool is_active() const;  // !bypassed && has_clip && clip has data

    // Visibility
    void set_visible(bool visible);
    bool is_visible() const;
    void set_lane_height(double height_px);
    double lane_height() const;

    // Evaluation
    double evaluate(uint64_t ppqn) const;

private:
    AutomationLaneID id_;
    std::string name_;
    Color color_;
    ParameterID target_;
    std::shared_ptr<AutomationClip> clip_;
    bool armed_ = false;
    bool bypassed_ = false;
    bool visible_ = true;
    double lane_height_px_ = 80;
};
```

### 8.2. Lane Display

```
Track automation lanes (collapsed):
┌──────────────────────────────────────────────┐
│ Mixer ▼  │ Volume  │ Pan  │ FX1 on │  +  │  ← Lane headers
└──────────────────────────────────────────────┘

Track automation lanes (expanded):
┌──────────────────────────────────────────────┐
│ 🔴 Volume  [─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─]─── │
│            ┌──┐                 ┌──┐        │
│            │  │  ┌──┐          │  │        │
│            │  │  │  │  ┌──┐   │  │        │
│            └──┘  └──┘  └──┘   └──┘        │
├──────────────────────────────────────────────┤
│ 🔴 Pan     [─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─]─── │
│            ──────┐    ┌────┐    ┌──         │
│                 └────┘    └────┘            │
├──────────────────────────────────────────────┤
│ ○ FX Cutoff [─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─]   │  ← Not armed
│            ────┐       ┌───┐                │
│                └───────┘   └──────           │
└──────────────────────────────────────────────┘

Legend:
🔴 = Armed for recording
○ = Not armed
[  ] = Clip present
─── = No clip (empty lane)
```

---

## 9. Recording Automation

### 9.1. Recording Pipeline

```cpp
class AutomationRecorder {
public:
    void start_recording(AutomationLane* lane);
    void stop_recording();

    // Called from audio thread with current parameter value
    void record_value(uint64_t sample_position, double value);

    // Optional: record with smoothing
    void set_smoothing(double ms);         // Smooth incoming data

    // Post-processing
    enum class ReduceMode {
        None,            // Keep every sample (dense)
        ReduceOnStop,    // Simplify on record stop
        Adaptive         // Simplify during recording (real-time)
    };
    void set_reduction(ReduceMode mode, double tolerance = 0.01);

    // Get recorded clip
    std::shared_ptr<AutomationClip> recorded_clip() const;

    // State
    bool is_recording() const;
    uint64_t recorded_samples() const;

private:
    AutomationLane* target_lane_ = nullptr;
    bool recording_ = false;

    // Raw recording buffer (ring buffer)
    struct Sample {
        uint64_t sample_position;
        double value;
    };
    static constexpr size_t RING_BUFFER_SIZE = 44100;  // ~1s at 44.1k
    std::array<Sample, RING_BUFFER_SIZE> ring_buffer_;
    std::atomic<uint64_t> write_index_{0};

    // Point reduction (Douglas-Peucker algorithm)
    std::vector<AutomationPoint> reduce_points(
        const std::vector<AutomationPoint>& input,
        double tolerance) const;
};
```

### 9.2. Point Reduction

```cpp
// Douglas-Peucker algorithm for reducing recorded automation points
// Without reduction: turning a knob for 5 seconds at 128 buffer = ~1800 points
// With reduction (0.01 tolerance): ~20-50 points (preserving the curve)

std::vector<AutomationPoint> AutomationRecorder::reduce_points(
    const std::vector<AutomationPoint>& input,
    double tolerance) const
{
    if (input.size() <= 2) return input;

    // Find point with maximum distance from line
    double max_dist = 0.0;
    size_t max_idx = 0;
    
    for (size_t i = 1; i < input.size() - 1; ++i) {
        double dist = perpendicular_distance(
            input[i], input.front(), input.back());
        if (dist > max_dist) {
            max_dist = dist;
            max_idx = i;
        }
    }

    // Recursively simplify
    if (max_dist > tolerance) {
        auto left = reduce_points(
            {input.begin(), input.begin() + max_idx + 1}, tolerance);
        auto right = reduce_points(
            {input.begin() + max_idx, input.end()}, tolerance);
        
        left.pop_back();  // Remove duplicate
        left.insert(left.end(), right.begin(), right.end());
        return left;
    } else {
        return {input.front(), input.back()};
    }
}
```

---

## 10. Real-Time Parameter Access

### 10.1. Parameter Cache

```cpp
class ParameterCache {
public:
    // Main thread writes (when automation/modulation changes)
    void update_value(ParameterID id, double normalized_value);

    // Audio thread reads (lock-free, every buffer)
    double read_value(ParameterID id) const;

    // Double-buffered for lock-free access
    void swap_buffers();

private:
    // Two copies of parameter state
    struct Buffer {
        std::unordered_map<ParameterID, double> values;
    };
    Buffer buffers_[2];
    std::atomic<uint32_t> active_buffer_{0};
};
```

### 10.2. Audio Thread Evaluation

```cpp
// Called every audio callback
void AutomationEngine::process_audio_thread(uint32_t frames,
                                             uint64_t sample_position)
{
    // For each active modulation connection:
    for (auto& [target_id, stack] : modulation_stacks_) {
        double base_value = parameter_cache_.read_value(target_id);

        // Apply all modulations in stack order
        ModulationContext ctx = build_context(sample_position);

        for (auto& entry : stack.entries()) {
            if (!entry.bypassed) {
                double mod_value = entry.source->evaluate(
                    sample_position, ctx);
                base_value = apply_modulation(base_value, mod_value,
                                              entry.amount, entry.polarity);
            }
        }

        // Clamp and cache
        base_value = clamp(base_value, 0.0, 1.0);
        parameter_cache_.update_value(target_id, base_value);
    }

    // Check for parameter changes from UI automation

    // Swap buffer for consumer reads
    parameter_cache_.swap_buffers();
}
```

---

## 11. Automation within Piano Roll

### 11.1. Integrated Editing

Automation can be edited directly within the piano roll, aligned with notes:

```
┌──────────────────────────────────────────────┐
│  Piano Roll  │  Automation: Filter Cutoff    │  ← Overlay toggle
├──────────────────────────────────────────────┤
│  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐  │
│  │  │  │██│██│  │██│██│  │██│██│  │  │  │  │  ← Note grid
│  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘  │
│  ┌────────────────────────────────────────┐  │
│  │  Filter Cutoff    ◉ auto overlay       │  │  ← Automation lane
│  │   ───┐    ┌──┐    ┌──┐    ┌───        │  │
│  │      └────┘  └────┘  └────┘           │  │
│  └────────────────────────────────────────┘  │
└──────────────────────────────────────────────┘

// Automation points snap to note boundaries when editing within piano roll
// Copy/paste operations include automation data when selected
// Automation points can be linked to individual notes
```

### 11.2. Per-Note Automation

```cpp
// Automation data embedded in individual notes
// Note expressions are not separate automation clips —
// they are properties of the Note object itself

struct NoteExpressionEvent {
    uint64_t offset_ppqn;        // Offset from note start
    ExpressionType type;         // PitchBend, CC, Pressure, Timbre
    uint8_t cc_number;           // For CC type
    uint16_t value;              // 14-bit value
    InterpolationType interpolation;
};

// These are stored in the Note and rendered in the
// piano roll's expression lanes. They are tied to the
// note's lifetime — copy/move/delete the note and
// its expressions follow.
```

---

## 12. Nested Modulation

### 12.1. Modulation Chain

```cpp
// Modulation sources can themselves be modulated:
// LFO 1 → Rate → LFO 2 → Amount → Filter Cutoff
//                        ↓
//                   Macro 1 → Offset → Filter Cutoff

// This creates a modulation chain:
// Source A → modulates parameter of → Source B → modulates parameter of → Target

class NestedModulation : public ModulationSource {
public:
    struct ChainLink {
        ModulationSource* source;
        ParameterID target_source_parameter;  // Which parameter of the next source to modulate
        double amount;
    };

    // Build modulation chain
    void add_link(const ChainLink& link);
    void remove_link(uint32_t index);

    // Evaluate entire chain
    double evaluate(uint64_t sample_position,
                    const ModulationContext& ctx) const override;

private:
    std::vector<ChainLink> chain_;
};
```

### 12.2. Example: LFO Rate modulated by Envelope Follower

```
Setup:
  1. Envelope Follower → Rate → LFO 1
  2. LFO 1 → Amount → Filter Cutoff

Behavior:
  - When audio is quiet: LFO 1 runs slow
  - When audio hits: LFO 1 rate increases
  - Filter cutoff wobbles with audio-reactive speed
  - Creates a dynamic, responsive modulation effect
```

---

## 13. API Reference

### 13.1. Public API

```cpp
class AutomationEngine {
public:
    // Lifecycle
    Result init(const AutomationConfig& config);
    void shutdown();

    // Automation clips
    AutomationClipID create_clip();
    bool destroy_clip(AutomationClipID id);
    AutomationClip* get_clip(AutomationClipID id);
    std::vector<AutomationClipID> list_clips() const;

    // Modulation sources
    template<typename T, typename... Args>
    T* create_source(Args&&... args);
    void destroy_source(ModulationSource* source);
    std::vector<ModulationSource*> list_sources() const;

    // Macros (convenience)
    MacroSource* create_macro(const std::string& name);
    void destroy_macro(MacroSource* macro);

    // Modulation matrix
    ModulationMatrix& matrix();

    // Automation lanes
    AutomationLane* create_lane(ParameterID target);
    void destroy_lane(AutomationLaneID id);
    AutomationLane* get_lane(AutomationLaneID id);
    std::vector<AutomationLane*> lanes_for_track(track_id_t track) const;

    // Recording
    void start_recording(AutomationLane* lane);
    void stop_recording();
    std::shared_ptr<AutomationClip> stop_and_get_clip();
    bool is_recording() const;

    // Real-time processing (called from audio thread)
    void process_audio_thread(uint32_t frames, uint64_t sample_position);

    // Parameter cache (for real-time reads)
    ParameterCache& parameter_cache();

    // Undo
    void push_undo_state(const std::string& description);
    void undo();
    void redo();
};
```

### 13.2. Configuration

```cpp
struct AutomationConfig {
    // Performance
    uint32_t max_sources_per_parameter = 16;
    uint32_t max_nested_depth = 8;
    bool enable_wavetable_lfo = true;
    
    // Recording
    double default_recording_smoothing_ms = 10.0;
    double default_point_reduction_tolerance = 0.01;
    AutomationRecorder::ReduceMode default_reduce_mode =
        AutomationRecorder::ReduceMode::ReduceOnStop;
    
    // Visual
    double default_lane_height_px = 80;
    double min_lane_height_px = 30;
    double max_lane_height_px = 300;

    // Compatibility
    bool enable_bitwig_style_nesting = true;
    bool enable_ableton_style_bypass = true;
};
```

---

## 14. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-AU-001 | Automation Data Model | 🔲 Pending |
| RFC-AU-002 | Automation Clip Types | 🔲 Pending |
| RFC-AU-003 | Bezier Curve Evaluation | 🔲 Pending |
| RFC-AU-004 | Interpolation Modes | 🔲 Pending |
| RFC-AU-005 | Modulation Sources (LFO, EnvFollower, etc.) | 🔲 Pending |
| RFC-AU-006 | Step Sequencer Source | 🔲 Pending |
| RFC-AU-007 | Macro System | 🔲 Pending |
| RFC-AU-008 | Modulation Matrix | 🔲 Pending |
| RFC-AU-009 | Modulation Stack & Evaluation | 🔲 Pending |
| RFC-AU-010 | Automation Lanes | 🔲 Pending |
| RFC-AU-011 | Real-Time Automation Recording | 🔲 Pending |
| RFC-AU-012 | Point Reduction (Douglas-Peucker) | 🔲 Pending |
| RFC-AU-013 | Lock-Free Parameter Cache | 🔲 Pending |
| RFC-AU-014 | Automation within Piano Roll | 🔲 Pending |
| RFC-AU-015 | Per-Note Automation Expressions | 🔲 Pending |
| RFC-AU-016 | Nested Modulation | 🔲 Pending |
| RFC-AU-017 | Envelope Shapes (ADSR/HADSR) | 🔲 Pending |
| RFC-AU-018 | Audio Sidechain & Envelope Follower | 🔲 Pending |
| RFC-AU-019 | Scripted Modulation (Lua) | 🔲 Pending |
| RFC-AU-020 | Automation Presets & Templates | 🔲 Pending |

---

## Appendix A: Modulation Complexity Examples

```
Example 1: Classic wobble bass
  LFO (Sine, 1/8, bipolar) → Amount 0.8 → Filter Cutoff

Example 2: Evolving pad
  LFO 1 (Sine, 1 bar) → Amount 0.6 → Filter Cutoff
  LFO 2 (Triangle, 4 bars) → Amount 0.3 → LFO 1 Rate
  Env (ADSR, 8s) → Amount 0.5 → Volume

Example 3: Rhythmic filter
  Step Seq (16 steps, 1/16) → Amount 0.7 → Filter Cutoff
  LFO (Square, 1/4 sync) → Amount 0.3 → Step Seq Rate

Example 4: Sidechain ducking
  Env Follower (Kick bus) → Amount 1.0 → Pad Volume
  → Attack: 1ms, Release: 100ms
  
Example 5: Macro-controlled multi-parameter
  Macro "Wobble": → Filter Cutoff (0.3-0.9)
                   → LFO 1 Rate (0.2-1.0)
                   → Reverb Mix (0.0-0.4)
```

## Appendix B: Performance Budget

| Operation | Budget | Notes |
|---|---|---|
| Point evaluation (1 clip, 100 pts) | < 1μs | Binary search + interpolation |
| Modulation stack (8 sources) | < 5μs | Per-parameter, per-buffer |
| Recording automation (1 lane) | < 2μs | Ring buffer write |
| Point reduction (1000 pts) | < 1ms | Post-recording |
| Full matrix evaluation (100 targets) | < 50μs | Per audio callback |
