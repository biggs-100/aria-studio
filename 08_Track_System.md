# ARIA DAW — Track System Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23 (model) + TypeScript (UI)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Track Model](#2-track-model)
3. [Track Types](#3-track-types)
4. [Clip Model](#4-clip-model)
5. [Arrangement View](#5-arrangement-view)
6. [Session View](#6-session-view)
7. [Track Groups & VCAs](#7-track-groups--vcas)
8. [Routing](#8-routing)
9. [Track FX Chain](#9-track-fx-chain)
10. [Freeze & Bounce](#10-freeze--bounce)
11. [API Reference](#11-api-reference)
12. [RFC Index](#12-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Track System manages all track and clip data in ARIA DAW. It is the bridge between the project model and the audio/MIDI engines, providing the data structures that represent the user's arrangement, session, clips, and routing.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Max tracks | 1000+ |
| Max clips per project | Unlimited (memory-bound) |
| Track types | 6 (Audio, MIDI, Group, VCA, Return, Master) |
| Routing | Flexible: pre/post, send/return, sidechain |
| Views | Arrangement + Session (dual-view synchronized) |
| Undo | Full undo stack for all track/clip operations |

### 1.3. Track Hierarchy

```
Master Track
│
├── Group Track 1
│   ├── Audio Track
│   ├── MIDI Track (→ Instrument)
│   └── Audio Track
│
├── Group Track 2
│   ├── MIDI Track (→ External Instrument)
│   └── Return Track
│
├── VCA Track "Drums"
│   ├── Audio Track
│   └── Audio Track
│
├── Audio Track (solo, no group)
├── MIDI Track (solo)
│
└── Return Tracks (Reverb, Delay, Compressor)
```

---

## 2. Track Model

### 2.1. Core Track

```cpp
class Track {
public:
    // Identity
    TrackID id() const;
    void set_name(const std::string& name);
    std::string name() const;
    void set_color(const Color& color);
    Color color() const;
    void set_icon(const std::string& icon_name);  // Optional track icon
    std::string icon() const;

    // Type
    TrackType type() const;

    // State
    void set_muted(bool muted);
    bool is_muted() const;
    void set_soloed(bool soloed);
    bool is_soloed() const;
    void set_armed(bool armed);        // Arm for recording
    bool is_armed() const;
    void set_monitoring(MonitorMode mode);
    MonitorMode monitoring() const;

    // Volume & Pan
    void set_volume(double db);        // -∞ to +24 dB
    double volume() const;
    void set_pan(double pan);           // -1.0 (L) to +1.0 (R)
    double pan() const;

    // Routing
    void set_output(RouteTarget target);
    RouteTarget output() const;
    void add_send(const RouteTarget& target, double level);
    void remove_send(uint32_t index);
    void set_send_level(uint32_t index, double level);
    std::vector<SendSlot> sends() const;
    void set_sidechain_source(TrackID source_track);
    TrackID sidechain_source() const;

    // Automation
    AutomationLane* automation_lane(ParameterID param);
    const AutomationLane* automation_lane(ParameterID param) const;
    std::vector<AutomationLane*> all_automation_lanes() const;
    AutomationLane* add_automation(ParameterID param);
    void remove_automation(ParameterID param);

    // Freeze
    void set_frozen(bool frozen);
    bool is_frozen() const;
    void set_show_frozen_audio(bool show);  // Show frozen audio clip
    bool show_frozen_audio() const;

    // Input
    void set_input(RouteInput source);
    RouteInput input() const;
    void set_input_monitoring(bool monitor);
    bool input_monitoring() const;

    // Track groups
    void set_parent_group(TrackID group_id);
    TrackID parent_group() const;

    // Height in UI
    void set_height(double height_px);
    double height() const;
    void set_minimized(bool minimized);
    bool is_minimized() const;

    // State persistence
    void serialize(Serializer& s) const;
    static std::unique_ptr<Track> deserialize(Deserializer& d);

protected:
    TrackID id_;
    std::string name_;
    Color color_ = {0xCC, 0xCC, 0xCC};
    
    TrackType type_ = TrackType::Audio;
    bool muted_ = false;
    bool soloed_ = false;
    bool armed_ = false;
    MonitorMode monitoring_ = MonitorMode::Auto;
    bool frozen_ = false;
    bool show_frozen_ = false;

    double volume_db_ = 0.0;          // 0 dB
    double pan_ = 0.0;                // Center
    
    RouteInput input_;
    RouteTarget output_;
    std::vector<SendSlot> sends_;
    TrackID sidechain_source_ = INVALID_TRACK_ID;
    TrackID parent_group_ = INVALID_TRACK_ID;

    std::unordered_map<ParameterID, std::unique_ptr<AutomationLane>> automation_lanes_;

    double height_px_ = 80;
    bool minimized_ = false;
};
```

### 2.2. Track Identity

```cpp
struct TrackID {
    uint64_t value;
    
    bool operator==(const TrackID& other) const { return value == other.value; }
    bool operator!=(const TrackID& other) const { return value != other.value; }
    
    static constexpr TrackID INVALID = {0};
    static TrackID generate();  // Thread-safe unique ID
};

enum class TrackType {
    Audio,          // Audio clip playback and recording
    MIDI,           // MIDI clip playback and instrument hosting
    Group,          // Container for child tracks (audio summing)
    VCA,            // VCA fader control (volt/octave group fader)
    Return,         // Return track (FX bus)
    Master          // Master output bus
};

enum class MonitorMode {
    Auto,           // Auto-monitor (on when armed)
    On,             // Always monitor input
    Off             // Never monitor input
};
```

### 2.3. Routing Types

```cpp
struct RouteTarget {
    enum class Type {
        Master,
        Track,          // Send to another track
        Group,          // Send to parent group
        External,       // External hardware output
        None            // No output
    };
    
    Type type = Type::Master;
    TrackID track_id = TrackID::INVALID;
    std::string external_port;   // For external routing
    
    bool operator==(const RouteTarget& other) const;
};

struct RouteInput {
    enum class Type {
        None,           // No input
        Internal,       // Internal track (for routing)
        ExternalAudio,  // Audio interface input
        ExternalMIDI,   // MIDI input
        Sidechain       // Sidechain input
    };
    
    Type type = Type::None;
    uint32_t channel_index = 0;     // Audio interface channel
    std::string device_id;          // Device identifier
    uint32_t num_channels = 1;      // 1 (mono) or 2 (stereo)
};

struct SendSlot {
    RouteTarget target;     // Where to send
    double level = -∞;      // Send level in dB
    bool pre_fader = true;  // Pre-fader or post-fader
    bool pre_fx = false;    // Pre-fx or post-fx
};
```

---

## 3. Track Types

### 3.1. Audio Track

```cpp
class AudioTrack : public Track {
public:
    AudioTrack();
    
    // Audio routing
    void set_input_channel(uint32_t channel);
    uint32_t input_channel() const;
    void set_input_format(AudioFormat format);  // Mono/Stereo
    AudioFormat input_format() const;
    
    // Clip management
    void add_clip(std::shared_ptr<AudioClip> clip);
    void remove_clip(ClipID clip_id);
    AudioClip* clip_at(uint64_t ppqn);
    std::vector<AudioClip*> clips_in_range(uint64_t start, uint64_t end);
    const std::vector<std::shared_ptr<AudioClip>>& clips() const;
    
    // Crossfades
    void set_crossfade(ClipID clip_a, ClipID clip_b, uint32_t samples);
    void remove_crossfade(ClipID clip_a, ClipID clip_b);
    
    // Warp markers
    void add_warp_marker(uint64_t ppqn, uint64_t sample_offset);
    void remove_warp_marker(uint64_t ppqn);
    
    // Recording
    void prepare_recording();  // Allocate recording buffer
    bool recording() const;
    
private:
    std::vector<std::shared_ptr<AudioClip>> clips_;
    std::unordered_map<std::pair<ClipID, ClipID>, Crossfade> crossfades_;
    std::vector<WarpMarker> warp_markers_;
};
```

### 3.2. MIDI Track

```cpp
class MidiTrack : public Track {
public:
    MidiTrack();
    
    // Instrument
    void set_instrument(PluginID plugin);
    PluginID instrument() const;
    bool has_instrument() const;
    
    // MIDI routing
    void set_midi_output(MidiOutputTarget target);
    MidiOutputTarget midi_output() const;
    void set_midi_input(MidiInputSource source);
    MidiInputSource midi_input() const;
    
    // MIDI transforms (applied on playback)
    void set_transpose(int8_t semitones);
    int8_t transpose() const;
    void set_velocity_curve(VelocityCurve curve);
    VelocityCurve velocity_curve() const;
    
    // Clip management
    void add_clip(std::shared_ptr<MidiClip> clip);
    void remove_clip(ClipID clip_id);
    MidiClip* clip_at(uint64_t ppqn);
    std::vector<MidiClip*> clips_in_range(uint64_t start, uint64_t end);
    const std::vector<std::shared_ptr<MidiClip>>& clips() const;
    
    // Key range (for drum maps, key switches)
    void set_key_range(uint8_t low, uint8_t high);
    std::pair<uint8_t, uint8_t> key_range() const;
    
    // Drum map
    void set_drum_map(const DrumMap& map);
    DrumMap drum_map() const;
    bool has_drum_map() const;

private:
    PluginID instrument_;
    MidiOutputTarget midi_output_;
    MidiInputSource midi_input_;
    int8_t transpose_ = 0;
    VelocityCurve velocity_curve_;
    
    std::vector<std::shared_ptr<MidiClip>> clips_;
    uint8_t key_range_low_ = 0;
    uint8_t key_range_high_ = 127;
    std::optional<DrumMap> drum_map_;
};
```

### 3.3. Group Track

```cpp
class GroupTrack : public Track {
public:
    GroupTrack();
    
    // Child track management
    void add_child(TrackID track);
    void remove_child(TrackID track);
    std::vector<TrackID> children() const;
    bool has_child(TrackID track) const;
    
    // Group mode
    enum class GroupMode {
        Summing,        // Sum all children (default)
        Routing,        // Route children individually (like Ableton)
        Folder          // Folder mode (no summing, visual only)
    };
    void set_group_mode(GroupMode mode);
    GroupMode group_mode() const;
    
    // Fold/unfold
    void set_folded(bool folded);
    bool is_folded() const;
    void set_fold_depth(uint32_t depth);
    uint32_t fold_depth() const;
    
private:
    std::vector<TrackID> children_;
    GroupMode group_mode_ = GroupMode::Summing;
    bool folded_ = false;
    uint32_t fold_depth_ = 0;
};
```

### 3.4. VCA Track

```cpp
class VCATrack : public Track {
public:
    VCATrack();
    
    // VCA slave tracks
    void add_slave(TrackID track);
    void remove_slave(TrackID track);
    std::vector<TrackID> slaves() const;
    
    // VCA behavior:
    // Moving the VCA fader moves ALL assigned slave faders
    // The slave fader position reflects the VCA + slave fader position
    // Automation on VCA track affects all slaves proportionally
    
    // VCA fader (the actual value is Track::volume_db_)
    // VCA affects: volume, pan, mute, send levels (configurable)
    
    enum class VCAAffects {
        Volume,     // Volume only
        VolumePan,  // Volume + Pan
        All         // Volume + Pan + Sends
    };
    void set_affects(VCAAffects affects);
    VCAAffects affects() const;
    
private:
    std::vector<TrackID> slaves_;
    VCAAffects affects_ = VCAAffects::All;
};
```

### 3.5. Return Track

```cpp
class ReturnTrack : public Track {
public:
    ReturnTrack();
    
    // Return tracks always receive from sends
    // They have no clips, no input, no recording
    
    // FX chain (the return processes audio through FX)
    void add_fx(PluginID plugin);
    void remove_fx(uint32_t index);
    void set_fx_order(const std::vector<PluginID>& order);
    std::vector<PluginID> fx_chain() const;
    
    // Return always goes to Master (or parent group)
    // Return level is controlled by Track::volume_db_
    
    // Solo safe (return is never affected by track solo)
    void set_solo_safe(bool safe);
    bool solo_safe() const;
    
private:
    std::vector<PluginID> fx_chain_;
    bool solo_safe_ = true;
};
```

### 3.6. Master Track

```cpp
class MasterTrack : public Track {
public:
    MasterTrack();
    
    // Master track is the final output bus
    // Always exists, cannot be deleted
    // Always last in routing chain
    // No input selection (receives all routed tracks)
    
    // Master FX chain (post-mixer processing)
    void add_fx(PluginID plugin);
    void remove_fx(uint32_t index);
    std::vector<PluginID> fx_chain() const;
    
    // Hardware output
    void set_hardware_output(const std::string& device_id, uint32_t channels);
    std::pair<std::string, uint32_t> hardware_output() const;
    
    // Dithering
    void set_dither_type(DitherType type);
    DitherType dither_type() const;
    
    // Monitoring
    void set_monitor_output(const std::string& device_id);  // Separate monitor/headphone output
    std::string monitor_output() const;
    
    // No automation lanes (master is always manual)
    
private:
    std::vector<PluginID> master_fx_;
    std::string hardware_device_;
    uint32_t hardware_channels_ = 2;
    DitherType dither_ = DitherType::None;
    std::string monitor_device_;
};
```

---

## 4. Clip Model

### 4.1. Base Clip

```cpp
class Clip {
public:
    ClipID id() const;
    void set_name(const std::string& name);
    std::string name() const;
    void set_color(const Color& color);
    Color color() const;
    
    // Position & length
    void set_start(uint64_t ppqn);
    uint64_t start() const;
    void set_length(uint64_t ppqn);
    uint64_t length() const;
    uint64_t end() const;  // start + length
    
    // Looping
    void set_looping(bool loop);
    bool is_looping() const;
    void set_loop_start(uint64_t ppqn);
    uint64_t loop_start() const;
    void set_loop_end(uint64_t ppqn);
    uint64_t loop_end() const;
    
    // Fades
    void set_fade_in(uint64_t ppqn);
    uint64_t fade_in() const;
    void set_fade_out(uint64_t ppqn);
    uint64_t fade_out() const;
    void set_fade_in_shape(FadeShape shape);
    FadeShape fade_in_shape() const;
    void set_fade_out_shape(FadeShape shape);
    FadeShape fade_out_shape() const;
    
    // Gain
    void set_gain(double db);
    double gain() const;
    
    // Mute
    void set_muted(bool mute);
    bool is_muted() const;
    
    // Time stretch (warp)
    void set_time_stretch(bool stretch);
    bool is_time_stretched() const;
    void set_stretch_ratio(double ratio);  // 1.0 = original
    double stretch_ratio() const;
    
    // Pitch shift
    void set_pitch_shift(int32_t cents);
    int32_t pitch_shift() const;  // Cents
    
    // Reverse
    void set_reversed(bool reverse);
    bool is_reversed() const;
    
    // Clip type
    ClipType type() const;
    
    // Serialization
    virtual void serialize(Serializer& s) const = 0;
    virtual void deserialize(Deserializer& d) = 0;

protected:
    ClipID id_;
    std::string name_;
    Color color_;
    
    uint64_t start_ppqn_ = 0;
    uint64_t length_ppqn_ = 960 * 4;  // 1 bar default
    
    bool looping_ = false;
    uint64_t loop_start_ppqn_ = 0;
    uint64_t loop_end_ppqn_ = 960 * 4;
    
    uint64_t fade_in_ppqn_ = 0;
    uint64_t fade_out_ppqn_ = 0;
    FadeShape fade_in_shape_ = FadeShape::Linear;
    FadeShape fade_out_shape_ = FadeShape::Linear;
    
    double gain_db_ = 0.0;
    bool muted_ = false;
    
    bool time_stretch_ = false;
    double stretch_ratio_ = 1.0;
    int32_t pitch_shift_cents_ = 0;
    bool reversed_ = false;
    
    ClipType clip_type_;
};

enum class ClipType {
    Audio,
    MIDI,
    Automation
};

enum class FadeShape {
    Linear,
    EqualPower,
    Exponential,
    Logarithmic,
    SCurve
};
```

### 4.2. Audio Clip

```cpp
class AudioClip : public Clip {
public:
    AudioClip();
    
    // Audio file reference
    void set_file_path(const std::string& path);
    std::string file_path() const;
    void set_file_hash(const std::string& hash);  // For change detection
    std::string file_hash() const;
    
    // File info (cached from analysis)
    AudioFileInfo file_info() const;
    uint32_t sample_rate() const;
    uint16_t bit_depth() const;
    uint32_t channels() const;
    uint64_t total_samples() const;
    
    // Sample offset within source file
    void set_sample_offset(uint64_t samples);
    uint64_t sample_offset() const;
    void set_sample_length(uint64_t samples);  // How many samples to use
    uint64_t sample_length() const;
    
    // Warp markers (for time alignment)
    void add_warp_marker(uint64_t source_sample, uint64_t target_ppqn);
    void remove_warp_marker(uint64_t source_sample);
    std::vector<WarpMarker> warp_markers() const;
    void clear_warp_markers();
    
    // Transient markers
    std::vector<uint64_t> transient_positions() const;
    void analyze_transients();  // Trigger transient analysis
    
    // Waveform cache
    struct WaveformCache {
        std::vector<float> peaks;    // Peak values (downsampled for display)
        std::vector<float> minima;   // Minimum values
        uint32_t resolution;         // Samples per pixel
    };
    WaveformCache get_waveform(uint32_t resolution) const;
    void invalidate_waveform_cache();
    
    // Clip gain envelope (non-automation, clip-specific)
    void set_gain_envelope(const std::vector<GainPoint>& envelope);
    std::vector<GainPoint> gain_envelope() const;

private:
    std::string file_path_;
    std::string file_hash_;
    
    uint64_t sample_offset_ = 0;
    uint64_t sample_length_ = 0;  // 0 = entire file
    
    std::vector<WarpMarker> warp_markers_;
    std::vector<uint64_t> transients_;
    
    mutable std::optional<WaveformCache> waveform_cache_;
    
    std::vector<GainPoint> gain_envelope_;
};

struct AudioFileInfo {
    uint32_t sample_rate;
    uint16_t bit_depth;
    uint32_t channels;
    uint64_t total_samples;
    double duration_seconds;
    double peak_level;
    double rms_level;
};
```

### 4.3. MIDI Clip

```cpp
class MidiClip : public Clip {
public:
    MidiClip();
    
    // MIDI data
    std::shared_ptr<MidiData> midi_data() const;
    void set_midi_data(std::shared_ptr<MidiData> data);
    
    // Note management
    void add_note(const MidiNote& note);
    void remove_note(NoteID id);
    MidiNote* find_note(NoteID id);
    const std::vector<MidiNote>& notes() const;
    size_t note_count() const;
    
    // Quantization
    void quantize(uint32_t grid_ppqn, double strength, bool swing);
    void humanize(double timing, double velocity);
    
    // Transpose
    void transpose(int8_t semitones, bool scale_aware);
    
    // Drum map
    void set_drum_map(const DrumMap& map);
    DrumMap drum_map() const;
    bool has_drum_map() const;
    
    // Piano roll integration
    std::vector<MidiNote> notes_in_range(uint64_t start_ppqn, uint64_t end_ppqn,
                                          uint8_t key_low, uint8_t key_high) const;

private:
    std::shared_ptr<MidiData> midi_data_;
    std::optional<DrumMap> drum_map_;
};
```

### 4.4. Automation Clip (wrapper)

```cpp
class AutomationClipWrapper : public Clip {
public:
    AutomationClipWrapper();
    
    // Link to automation engine clip
    void set_automation_clip(AutomationClipID id);
    AutomationClipID automation_clip_id() const;
    
    // The actual automation data lives in AutomationEngine
    // This clip is a timeline/arrangement reference to it
    
    // Parameter
    void set_target_parameter(ParameterID param);
    ParameterID target_parameter() const;
    
    // Display
    double min_display_value() const;
    double max_display_value() const;
    void set_display_range(double min, double max);
    
private:
    AutomationClipID automation_clip_;
    ParameterID target_parameter_;
    double display_min_ = 0.0;
    double display_max_ = 1.0;
};
```

---

## 5. Arrangement View

### 5.1. Arrangement Model

```cpp
class Arrangement {
public:
    Arrangement();
    
    // Track ordering
    std::vector<TrackID> track_order() const;
    void move_track(TrackID id, uint32_t new_index);
    void insert_track(TrackID id, uint32_t index);
    void remove_track(TrackID id);
    
    // Track groups
    void set_track_parent(TrackID child, TrackID parent);
    void remove_track_from_group(TrackID child);
    std::vector<TrackID> child_tracks(TrackID parent) const;
    
    // Time range
    void set_length(uint64_t ppqn);
    uint64_t length() const;
    
    // Loop
    void set_loop_range(uint64_t start, uint64_t end);
    std::pair<uint64_t, uint64_t> loop_range() const;
    void set_loop_enabled(bool enabled);
    bool loop_enabled() const;
    
    // Punch range (recording)
    void set_punch_range(uint64_t start, uint64_t end);
    std::pair<uint64_t, uint64_t> punch_range() const;
    void set_punch_enabled(bool enabled);
    bool punch_enabled() const;
    
    // Markers
    MarkerID add_marker(uint64_t ppqn, const std::string& name);
    void remove_marker(MarkerID id);
    void move_marker(MarkerID id, uint64_t ppqn);
    std::vector<Marker> markers() const;
    
    // Tempo & time signature (reference to Timeline)
    TempoMap& tempo_map();
    const TempoMap& tempo_map() const;
    
    // Time stretch mode (for all audio clips)
    enum class TimeStretchMode {
        PreservePitch,   // Rubber band (default)
        ChangePitch,     // Classic tape-style
        Resample         // Raw resampling
    };
    void set_default_stretch_mode(TimeStretchMode mode);
    TimeStretchMode default_stretch_mode() const;
    
    // Serialization
    void serialize(Serializer& s) const;
    static Arrangement deserialize(Deserializer& d);
    
private:
    std::vector<TrackID> track_order_;
    uint64_t length_ppqn_ = 960 * 64;     // 16 bars default
    
    bool loop_enabled_ = false;
    uint64_t loop_start_ppqn_ = 0;
    uint64_t loop_end_ppqn_ = 960 * 16;
    
    bool punch_enabled_ = false;
    uint64_t punch_start_ppqn_ = 0;
    uint64_t punch_end_ppqn_ = 960 * 16;
    
    std::vector<Marker> markers_;
    
    TimeStretchMode default_stretch_ = TimeStretchMode::PreservePitch;
};
```

### 5.2. Arrangement UI

```
┌───────────────────────────────────────────────────────────────┐
│  ⏵ ⏸ ⏹  │ 2.1.0  │ 140 BPM  │ 4/4  │  ● REC              │
├───────┬───────────────────────────────────────────────────────┤
│       │  Bar 1    Bar 2    Bar 3    Bar 4    Bar 5    Bar 6  │
│───────┼───────────────────────────────────────────────────────│
│ Kick  │ ████     ████     ████     ████     ████             │  ← Audio clip
│───────┼───────────────────────────────────────────────────────│
│ Snare │ ┌──────┐          ┌──────┐          ┌──────┐        │
│       │ │      │          │      │          │      │        │
│───────┼───────────────────────────────────────────────────────│
│ Synth │ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  ← MIDI clip
│       │ (MIDI notes displayed as mini piano roll)             │
│───────┼───────────────────────────────────────────────────────│
│ Pad   │ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│───────┼───────────────────────────────────────────────────────│
│ Reverb│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  ← Return track
│───────┼───────────────────────────────────────────────────────│
│ Master│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│       │ ↓↓↓↓         ↓↓↓↓         ↓↓↓↓                      │  ← Automation clip
└───────┴───────────────────────────────────────────────────────┘
```

---

## 6. Session View

### 6.1. Session Model

```cpp
class Session {
public:
    Session();
    
    // Clip slots (grid: tracks × scenes)
    void set_clip_slot(TrackID track, SceneID scene, std::shared_ptr<Clip> clip);
    void remove_clip_slot(TrackID track, SceneID scene);
    Clip* clip_at(TrackID track, SceneID scene) const;
    
    // Scenes
    SceneID add_scene(const std::string& name);
    void remove_scene(SceneID id);
    void rename_scene(SceneID id, const std::string& name);
    void move_scene(SceneID id, uint32_t new_index);
    std::vector<SceneID> scene_order() const;
    std::string scene_name(SceneID id) const;
    
    // Scene launch
    void launch_scene(SceneID id, LaunchOptions options);
    void stop_scene(SceneID id);
    bool scene_is_playing(SceneID id) const;
    
    // Clip launch
    void launch_clip(TrackID track, SceneID scene, LaunchOptions options);
    void stop_clip(TrackID track);
    Clip* playing_clip(TrackID track) const;
    
    // Follow actions
    void set_clip_follow_action(ClipID clip, FollowAction action);
    FollowAction clip_follow_action(ClipID clip) const;
    void set_scene_follow_action(SceneID scene, FollowAction action);
    FollowAction scene_follow_action(SceneID scene) const;
    
    // Crossfader
    void set_crossfader(double position);  // -1.0 to 1.0
    double crossfader() const;
    void assign_track_to_crossfader(TrackID track, CrossfaderAssignment assignment);
    CrossfaderAssignment track_crossfader(TrackID track) const;
    
    // Quantization
    void set_global_quantization(uint32_t ppqn);
    uint32_t global_quantization() const;
    enum class LaunchQuantization {
        None, Global, Bar, Beat, Half, Quarter, Eighth, Sixteenth
    };
    void set_launch_quantization(LaunchQuantization q);
    LaunchQuantization launch_quantization() const;
    
private:
    // Grid: track → scene → clip
    std::unordered_map<TrackID, std::unordered_map<SceneID, std::shared_ptr<Clip>>> slots_;
    
    std::vector<SceneID> scene_order_;
    std::unordered_map<SceneID, std::string> scene_names_;
    
    // Playing state
    std::unordered_map<TrackID, ClipID> playing_clips_;
    std::unordered_set<SceneID> playing_scenes_;
    
    // Follow actions
    std::unordered_map<ClipID, FollowAction> clip_follow_actions_;
    std::unordered_map<SceneID, FollowAction> scene_follow_actions_;
    
    double crossfader_ = 0.0;
    std::unordered_map<TrackID, CrossfaderAssignment> crossfader_assignments_;
    
    uint32_t global_quantization_ = 240;  // 16th notes
    LaunchQuantization launch_q_ = LaunchQuantization::Global;
};

struct LaunchOptions {
    enum class Mode {
        Trigger,        // Start from beginning
        Toggle,         // Toggle play/stop
        Retrigger,      // Restart if already playing
        Gate,           // Play while holding
        ToggleGate      // Toggle between play and gate
    };
    Mode mode = Mode::Trigger;
    bool quantize = true;        // Wait for quantization boundary
    uint8_t velocity = 127;      // Launch velocity (affects volume)
};

enum class FollowAction {
    None,
    Stop,
    PlayNext,
    PlayPrevious,
    PlayRandom,
    PlayFirst,
    PlayLast,
    ContinueAsLoop,
    ContinueToNext
};

enum class CrossfaderAssignment {
    None,
    A,           // Left (track A)
    B,           // Right (track B)
    Both         // Always audible
};
```

### 6.2. Session UI

```
Session View:
┌───────────────────────────────────────────────────────────────┐
│  Scene 1   │ Kick    │ Snare  │ Synth   │ Pad    │ Master   │
│  ● Scene 2 │██████  │        │ ░░░░░░  │░░░░░░ │           │
│  ○ Scene 3 │        │██████  │ ░░░░░░  │░░░░░░ │           │
│  ○ Scene 4 │██████  │██████  │ ░░░░░░  │       │           │
│  ○ Scene 5 │        │        │ ░░░░░░  │░░░░░░ │           │
│            │        │        │         │       │           │
│  [Stop]    │ [ ● ]  │ [ ○ ]  │ [ ● ]   │ [ ○ ] │           │
└───────────────────────────────────────────────────────────────┘
  Crossfader: [A ────────────●──────────── B]
```

---

## 7. Track Groups & VCAs

### 7.1. Group vs VCA Comparison

| Feature | Group Track | VCA Track |
|---|---|---|
| Audio summing | Yes (sums children) | No (controls faders only) |
| Fader control | Controls group output | Controls all slave faders |
| FX insertion | Yes (post-sum) | No |
| Automation | Group output only | Proportional slave control |
| Visual grouping | Yes (can fold) | No (no children shown) |
| Use case | Drums bus, instrument bus | Mastering, arrangement control |

### 7.2. Group Summing

```
Group Track "Drums":
    ┌──────────────────────┐
    │ Kick (Audio Track)   │──┐
    ├──────────────────────┤  │
    │ Snare (Audio Track)  │──┼──► Group Sum Bus ──► FX ──► Master
    ├──────────────────────┤  │
    │ HiHat (Audio Track)  │──┘
    └──────────────────────┘
    
    Group Track Fader: Controls sum of all children
    Group Track FX: Applied to the sum (compressor on drum bus)
```

### 7.3. VCA Fader Movement

```
VCA Fader at -6 dB:
    ┌──────────────────────┐
    │ VCA "Drums" │ -6dB   │  ← Moving this fader moves ALL slaves
    ├──────────────────────┤
    │ Kick:  -3dB → -9dB   │  ← Slave faders move proportionally
    │ Snare: -6dB → -12dB  │
    │ HiHat: -∞  → -∞      │  ← Muted stays muted
    └──────────────────────┘
    
    The VCA does NOT sum audio — it only controls faders.
    Each slave track still routes individually.
```

---

## 8. Routing

### 8.1. Signal Flow Diagram

```
Audio Track Input → Pre-FX → Pre-Fader Send → Track FX → Post-FX Send → Fader → Post-Fader Send → Route Output
                         │                          │           │
                         ▼                          ▼           ▼
                    Sidechain Out               Send Bus    Send Bus
                         │                          │           │
                         ▼                          ▼           ▼
                    Sidechain Input → Other Track → Return Track → Master
                                                                        │
                                       MIDI Track → Instrument → Audio ─┘
                                                                        │
                                                                   Master FX
                                                                        │
                                                                   Hardware Out
```

### 8.2. Routing Matrix

```cpp
class RoutingManager {
public:
    // Connect source track output to target
    void connect(TrackID source, const RouteTarget& target);
    void disconnect(TrackID source);
    
    // Sends
    SendID add_send(TrackID source, const RouteTarget& target);
    void remove_send(SendID id);
    void set_send_level(SendID id, double level_db);
    void set_send_prefader(SendID id, bool pre_fader);
    
    // Sidechain
    void set_sidechain(TrackID source, TrackID target);
    void remove_sidechain(TrackID target);
    
    // Get signal path
    std::vector<RouteTarget> signal_path(TrackID source) const;
    
    // Validate routing (detect cycles)
    bool would_create_cycle(TrackID source, const RouteTarget& target) const;
    
    // Audio graph integration
    void rebuild_audio_graph(AudioGraph& graph);
    void rebuild_midi_graph(MidiGraph& graph);
    
private:
    struct Connection {
        TrackID source;
        RouteTarget target;
    };
    
    std::vector<Connection> connections_;
    std::unordered_map<SendID, SendConfig> sends_;
    std::unordered_map<TrackID, TrackID> sidechain_connections_;
};
```

---

## 9. Track FX Chain

### 9.1. FX Chain Model

```cpp
class FXChain {
public:
    FXChain();
    
    // Slot management
    SlotID add_plugin(PluginID plugin, uint32_t position = -1);
    void remove_plugin(SlotID id);
    void move_plugin(SlotID id, uint32_t new_position);
    void swap_plugins(SlotID a, SlotID b);
    void clear();
    
    // Bypass
    void set_slot_bypass(SlotID id, bool bypass);
    bool slot_bypassed(SlotID id) const;
    void set_chain_bypass(bool bypass);
    bool chain_bypassed() const;
    
    // Plugins in chain
    uint32_t slot_count() const;
    PluginID plugin_at(uint32_t index) const;
    std::vector<SlotID> all_slots() const;
    
    // Dry/Wet
    void set_slot_mix(SlotID id, double wet_percent);
    double slot_mix(SlotID id) const;
    
    // PDC (Plugin Delay Compensation)
    uint32_t total_latency() const;       // Sum of all plugins
    struct LatencyPerSlot {
        SlotID slot;
        uint32_t samples;
    };
    std::vector<LatencyPerSlot> latency_per_slot() const;
    
    // Presets
    void save_as_preset(const std::string& name);
    static FXChain load_from_preset(const std::string& name);
    
    // Serialization
    void serialize(Serializer& s) const;
    static FXChain deserialize(Deserializer& d);
    
private:
    struct FXSlot {
        PluginID plugin;
        bool bypassed = false;
        double mix = 1.0;       // 0.0-1.0
        uint32_t latency = 0;   // Samples
    };
    
    std::vector<FXSlot> slots_;
    bool chain_bypassed_ = false;
};
```

### 9.2. FX Rack UI

```
Track FX Chain:
┌──────────────────────────────────────────────┐
│  Track FX  │  [Bypass All]  [Save Preset ▼]  │
├──────────────────────────────────────────────┤
│  ┌────────────────────────────────────────┐  │
│  │ 1. EQ Eight       ○  ○  ○  ○  ○  ○  │  │  ← Plugin slot
│  │   ▲ EQ Eight - Clean Equalizer       │  │
│  │   └─────────────── ●  dry ◄──●──► wet│  │
│  └────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────┐  │
│  │ 2. Compressor      ○  ○  ○  ○  ○  ○  │  │  ← Bypass button
│  │   ▲ Compressor - DynaMic              │  │
│  │   └──── Threshold: -18dB ─────────────│  │
│  └────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────┐  │
│  │ 3. Reverb          ○  ○  ○  ○  ○  ○  │  │
│  │   ▲ Reverb - Hall                     │  │
│  │   └─────────────── Mix: 35% ──────────│  │
│  └────────────────────────────────────────┘  │
│  [Add Plugin ▼]                              │
└──────────────────────────────────────────────┘
```

---

## 10. Freeze & Bounce

### 10.1. Freeze

```cpp
// Freezing renders a track's FX to an audio clip, freeing CPU

class FreezeManager {
public:
    // Freeze a track (render + replace with audio)
    void freeze_track(TrackID id, FreezeOptions options);
    void unfreeze_track(TrackID id);
    bool is_frozen(TrackID id) const;
    
    // Flatten (commit freeze permanently)
    void flatten_track(TrackID id);  // Can't unfreeze after flatten
    
    // Show frozen audio clip (to see what was rendered)
    void show_frozen_audio(TrackID id, bool show);
    
    // Render through entire track FX chain
    AudioClipPtr render_track(TrackID id, uint64_t start, uint64_t end);
    
    // Bounce (render selected tracks to new audio clips)
    struct BounceOptions {
        std::vector<TrackID> tracks;
        uint64_t range_start;
        uint64_t range_end;
        bool include_fx = true;
        bool include_sends = false;
        BounceFormat format;  // Individual, Stem, Master
    };
    void bounce(const BounceOptions& options);
    
private:
    struct FrozenState {
        AudioClipPtr frozen_clip;
        bool show_original = false;
        std::vector<PluginID> bypassed_fx;  // FX that were frozen
    };
    std::unordered_map<TrackID, FrozenState> frozen_tracks_;
};
```

---

## 11. API Reference

### 11.1. Public API

```cpp
class TrackProject {
public:
    // Track management
    TrackID create_track(TrackType type, const std::string& name = "");
    bool delete_track(TrackID id);
    Track* get_track(TrackID id);
    const Track* get_track(TrackID id) const;
    std::vector<TrackID> all_tracks() const;
    std::vector<TrackID> tracks_of_type(TrackType type) const;
    
    // Master track (always exists)
    MasterTrack* master_track();
    
    // Arrangement
    Arrangement& arrangement();
    const Arrangement& arrangement() const;
    
    // Session
    Session& session();
    const Session& session() const;
    
    // Routing
    RoutingManager& routing();
    const RoutingManager& routing() const;
    
    // Freeze
    FreezeManager& freeze();
    const FreezeManager& freeze() const;
    
    // Track count
    uint32_t track_count() const;
    uint32_t visible_track_count() const;
    
    // Track ordering
    std::vector<TrackID> track_display_order() const;
    void set_track_display_order(const std::vector<TrackID>& order);
};
```

---

## 12. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-TR-001 | Track Model & Identity | 🔲 Pending |
| RFC-TR-002 | Audio Track (clips, recording, warp) | 🔲 Pending |
| RFC-TR-003 | MIDI Track (instrument, drum map) | 🔲 Pending |
| RFC-TR-004 | Group Track (summing, folder) | 🔲 Pending |
| RFC-TR-005 | VCA Track (fader control, proportional) | 🔲 Pending |
| RFC-TR-006 | Return Track (FX bus, solo safe) | 🔲 Pending |
| RFC-TR-007 | Master Track (output, dither) | 🔲 Pending |
| RFC-TR-008 | Audio Clip (file ref, warp, transients) | 🔲 Pending |
| RFC-TR-009 | MIDI Clip (notes, quantize) | 🔲 Pending |
| RFC-TR-010 | Automation Clip (timeline wrapper) | 🔲 Pending |
| RFC-TR-011 | Arrangement View (timeline, markers) | 🔲 Pending |
| RFC-TR-012 | Session View (slots, scenes, launch) | 🔲 Pending |
| RFC-TR-013 | Track Groups & VCA Behavior | 🔲 Pending |
| RFC-TR-014 | Routing Matrix & Signal Flow | 🔲 Pending |
| RFC-TR-015 | Track FX Chain (slots, bypass, mix) | 🔲 Pending |
| RFC-TR-016 | Freeze & Bounce | 🔲 Pending |
| RFC-TR-017 | Arrangement vs Session Sync | 🔲 Pending |
| RFC-TR-018 | Track Undo/Redo | 🔲 Pending |
| RFC-TR-019 | Track Templates & Presets | 🔲 Pending |
