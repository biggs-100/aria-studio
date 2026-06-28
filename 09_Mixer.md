# ARIA DAW — Mixer Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23 (DSP engine) + TypeScript (UI)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Mixer Architecture](#2-mixer-architecture)
3. [Channel Strip](#3-channel-strip)
4. [Summing](#4-summing)
5. [Routing & Buses](#5-routing--buses)
6. [FX Racks](#6-fx-racks)
7. [Sends & Returns](#7-sends--returns)
8. [Sidechain](#8-sidechain)
9. [Metering](#9-metering)
10. [Master Channel](#10-master-channel)
11. [Mixer UI](#11-mixer-ui)
12. [API Reference](#12-api-reference)
13. [RFC Index](#13-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Mixer is the central hub for audio level control, routing, and effects processing. It receives audio from all tracks, processes it through channel strips and FX racks, sums it into buses, and delivers the final mix to the master output. Designed for professional mixing workflows with flexibility, precision, and high channel counts.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Max channels | 1000+ simultaneous |
| Internal precision | 64-bit float end-to-end |
| Summing | Linear gain-staging, no bit reduction between stages |
| Metering | Peak, RMS, LUFS, phase correlation |
| Fader resolution | 0.1 dB precision |
| PDC | Automatic, sample-accurate |
| Routing | Unlimited sends, flexible bus structure |

### 1.3. Mixer Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│                         Mixer                                      │
│                                                                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  │
│  │ Channel  │  │ Channel  │  │ Channel  │  │   ... 1000+      │  │
│  │ Strip 1  │  │ Strip 2  │  │ Strip 3  │  │                  │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────────┘  │
│       │             │             │               │              │
│       └─────────────┼─────────────┼───────────────┘              │
│                     │             │                               │
│               ┌─────▼─────────────▼──────┐                       │
│               │     Summing Bus          │                       │
│               │  (Groups, VCAs, Master)  │                       │
│               └────────────┬─────────────┘                       │
│                            │                                      │
│                    ┌───────▼────────┐                             │
│                    │   Master FX    │                             │
│                    └───────┬────────┘                             │
│                            │                                      │
│                    ┌───────▼────────┐                             │
│                    │  Master Fader  │                             │
│                    └───────┬────────┘                             │
│                            │                                      │
│                    ┌───────▼────────┐                             │
│                    │ Hardware Output│                             │
│                    └────────────────┘                             │
└───────────────────────────────────────────────────────────────────┘
```

---

## 2. Mixer Architecture

### 2.1. Mixer Engine

```cpp
class Mixer {
public:
    Result init(const MixerConfig& config);
    void shutdown();

    // Channel management
    ChannelID create_channel(const std::string& name, ChannelType type);
    void destroy_channel(ChannelID id);
    Channel* get_channel(ChannelID id);
    const Channel* get_channel(ChannelID id) const;
    std::vector<ChannelID> all_channels() const;
    
    // Master channel
    Channel* master_channel();
    const Channel* master_channel() const;
    
    // Buses
    BusID create_bus(const std::string& name);
    void destroy_bus(BusID id);
    void assign_channel_to_bus(ChannelID channel, BusID bus);
    void remove_channel_from_bus(ChannelID channel);
    
    // Processing (called from audio thread)
    void process(uint32_t frames, uint64_t sample_position);
    
    // Metering
    const GlobalMetering& global_metering() const;
    
    // PDC
    void recalculate_pdc();
    uint32_t total_latency() const;
    
private:
    std::unordered_map<ChannelID, std::unique_ptr<Channel>> channels_;
    ChannelID master_id_;
    std::unordered_map<BusID, std::unique_ptr<Bus>> buses_;
    
    // Processing order (topologically sorted)
    std::vector<ChannelID> processing_order_;
    
    GlobalMetering global_metering_;
    PDCManager pdc_;
    
    MixerConfig config_;
};

struct MixerConfig {
    uint32_t max_channels = 1024;
    uint32_t max_buses = 128;
    uint32_t max_sends_per_channel = 16;
    bool enable_lufs_metering = true;
    bool enable_spectrum_analyzer = true;
    uint32_t spectrum_fft_size = 2048;
    double master_volume_min_db = -60.0;
    double master_volume_max_db = +24.0;
};
```

### 2.2. Processing Order

```
1. Input channels process (audio from tracks)
2. Sends are tapped (pre or post-fader)
3. Return channels process
4. Group buses sum their assigned channels
5. VCA calculations (fader control)
6. Master channel processes
7. Hardware output
```

---

## 3. Channel Strip

### 3.1. Channel Model

```cpp
class Channel {
public:
    ChannelID id() const;
    
    // Identity
    void set_name(const std::string& name);
    std::string name() const;
    void set_color(const Color& color);
    Color color() const;
    ChannelType type() const;
    
    // Level controls
    void set_volume(double db);            // -∞ to +24 dB
    double volume() const;
    void set_pan(double pan);               // -1.0 (L) to +1.0 (R)
    double pan() const;
    void set_mute(bool mute);
    bool is_muted() const;
    void set_solo(bool solo);
    bool is_soloed() const;
    
    // Automation
    void set_volume_automated(double db);  // Set by automation engine
    void set_pan_automated(double pan);
    double effective_volume() const;        // volume + automation + VCA
    
    // Phase
    void set_phase_invert(bool invert);
    bool phase_inverted() const;
    
    // Stereo width
    void set_width(double width);           // 0.0 (mono) to 2.0 (wide)
    double width() const;
    
    // Input routing
    void set_input(const AudioInput& input);
    AudioInput input() const;
    void set_output(const AudioOutput& output);
    AudioOutput output() const;
    
    // FX Chain
    FXChain& fx_chain();
    const FXChain& fx_chain() const;
    
    // Sends
    SendID add_send(const SendConfig& config);
    void remove_send(SendID id);
    void set_send_level(SendID id, double db);
    void set_send_prefader(SendID id, bool pre_fader);
    std::vector<SendSlot> sends() const;
    
    // EQ (built-in, zero-latency)
    BuiltInEQ& eq();
    
    // Metering
    const ChannelMetering& metering() const;
    
    // Group / VCA
    void set_group(BusID bus);
    BusID group() const;
    void set_vca(TrackID vca_track);
    TrackID vca() const;
    
    // Serialization
    void serialize(Serializer& s) const;
    static std::unique_ptr<Channel> deserialize(Deserializer& d);

protected:
    ChannelID id_;
    std::string name_;
    Color color_ = {0xCC, 0xCC, 0xCC};
    ChannelType type_ = ChannelType::Audio;
    
    // Level
    std::atomic<double> volume_db_{0.0};
    std::atomic<double> automated_volume_db_{0.0};
    std::atomic<double> pan_{0.0};
    std::atomic<bool> muted_{false};
    std::atomic<bool> soloed_{false};
    std::atomic<bool> phase_invert_{false};
    std::atomic<double> width_{1.0};
    
    // VCA contribution (set by VCA engine)
    std::atomic<double> vca_contribution_db_{0.0};
    
    // Routing
    AudioInput input_;
    AudioOutput output_;
    
    // DSP
    FXChain fx_chain_;
    BuiltInEQ eq_;
    
    // Sends
    std::vector<SendSlot> sends_;
    
    // Groups
    BusID group_bus_ = INVALID_BUS_ID;
    TrackID vca_track_ = INVALID_TRACK_ID;
    
    // Metering
    ChannelMetering metering_;
};

enum class ChannelType {
    Audio,      // Standard audio channel
    MIDI,       // MIDI channel (no audio path)
    Group,      // Group bus (sums assigned channels)
    Return,     // Return from send
    VCA,        // VCA control channel (no audio path)
    Master      // Master output
};
```

### 3.2. Channel Strip Components

```
Channel Strip (vertical layout):
┌──────────────┐
│  Track Name   │  ← Header (track/plugin icon)
├──────────────┤
│  ┌────────┐  │
│  │ -24    │  │  ← Meter (peak + RMS)
│  │ -18    │  │
│  │ -12 ██ │  │
│  │  -6 ██ │  │
│  │   0 ██ │  │
│  └────────┘  │
│  ┌────────┐  │
│  │  ████  │  │  ← Volume fader (motorized-style)
│  │  ████  │  │
│  │  ────  │  │
│  │        │  │
│  └────────┘  │
│  -∞    0 dB  │  ← Fader label
├──────────────┤
│   EQ ▼      │  ← EQ expand/collapse
├──────────────┤
│  FX Rack    │  ← Plugin slots
├──────────────┤
│  Sends      │  ← Send knobs (1-16)
│  ┌─┐ ┌─┐   │
│  │R│ │D│   │  ← Send A (Reverb), Send B (Delay)
│  └─┘ └─┘   │
├──────────────┤
│ ⏻ M S P W │  ← Mute, Solo, Phase, Width
│ 🔴          │  ← Arm button
└──────────────┘
```

---

## 4. Summing

### 4.1. Summing Architecture

```cpp
class SummingBus {
public:
    BusID id() const;
    void set_name(const std::string& name);
    std::string name() const;
    
    // Assigned channels
    void assign_channel(ChannelID channel);
    void unassign_channel(ChannelID channel);
    bool is_assigned(ChannelID channel) const;
    std::vector<ChannelID> assigned_channels() const;
    uint32_t channel_count() const;
    
    // Bus output level
    void set_volume(double db);
    double volume() const;
    
    // Bus FX
    FXChain& fx_chain();
    const FXChain& fx_chain() const;
    
    // Bus output routing
    void set_output(const AudioOutput& output);
    AudioOutput output() const;
    
    // Processing
    void process(const AudioBuffer* inputs, uint32_t input_count,
                 AudioBuffer* output, uint32_t frames);
    
    // Metering
    const ChannelMetering& metering() const;

private:
    BusID id_;
    std::string name_;
    std::vector<ChannelID> channels_;
    
    double volume_db_ = 0.0;
    FXChain fx_chain_;
    AudioOutput output_;
    ChannelMetering metering_;
};
```

### 4.2. Summing Math

```cpp
// Internal summing is 64-bit float:
// output[i] = Σ(channel[j].sample[i] * channel[j].gain) * bus_gain
//
// All channels are summed linearly with no intermediate truncation.
// Pan law: -3dB center, equal-power (sin/cos stereo):

void apply_pan_law(double sample_left, double sample_right,
                   double pan, double* out_left, double* out_right)
{
    // Equal-power pan law (constant perceived loudness)
    double angle = (pan + 1.0) * M_PI / 4.0;  // -1 → 0, +1 → π/2
    *out_left = sample_left * std::cos(angle);
    *out_right = sample_right * std::sin(angle);
}

// Alternative pan laws (configurable per channel):
enum class PanLaw {
    EqualPower_3dB,     // -3dB at center (default)
    Linear_0dB,         // 0dB at center (constant amplitude)
    Linear_6dB,         // -6dB at center (constant power for mono)
    StereoBalance       // Balance control (no pan, reduces opposite side)
};
```

### 4.3. Gain Staging

```
Track level (-18dB) ──► Channel Fader (+6dB) ──► Group (-3dB) ──► Master (-6dB)
     │                       │                       │                  │
     ▼                       ▼                       ▼                  ▼
   -18dB                   -12dB                  -15dB              -21dB ← output

Headroom: 6dB of digital headroom before clipping (target: -18dBFS average)
Internal: 64-bit float → no clipping until final output stage
```

---

## 5. Routing & Buses

### 5.1. Bus Types

| Bus Type | Purpose | Default Target |
|---|---|---|
| **Group** | Sum multiple channels | Master |
| **Return** | Return from send FX | Master |
| **Fold-down** | Downmix (e.g., 5.1 → stereo) | Master |
| **Aux** | Auxiliary bus (monitors, cues) | Hardware output |
| **External** | Hardware output routing | Physical output |

### 5.2. Route Target

```cpp
struct AudioOutput {
    enum class Type {
        Master,         // Send to master bus
        Bus,            // Send to specific bus
        Track,          // Send directly to a track
        External,       // Hardware output
        None            // No output (silent)
    };
    
    Type type = Type::Master;
    union {
        BusID bus_id;
        TrackID track_id;
        uint32_t external_channel;  // Hardware output channel
    };
    
    // Mono/Stereo routing (for fold-down)
    enum class Format { Stereo, Mono, DualMono };
    Format format = Format::Stereo;
};
```

### 5.3. Routing Examples

```
Simple:
    Track → Master

Grouped:
    Track 1 ──┐
    Track 2 ──┤── Group "Drums" ── Master
    Track 3 ──┘

With sends:
    Track ────────────────────── Master
        └── Reverb Send ── Return "Reverb" ──┘

Multi-bus:
    Kick ───────── Drum Group ──┐
    Snare ──────── Drum Group ──┤
    Bass ──────── Instrument ───┤── Master
    Synth ─────── Instrument ───┘
    Vocals ─────────── Vocal ───┘
```

---

## 6. FX Racks

### 6.1. FX Rack Architecture

```cpp
class FXChain {
public:
    SlotID insert_plugin(PluginID plugin, uint32_t position);
    void remove_plugin(SlotID id);
    void move_plugin(SlotID id, uint32_t new_position);
    void clear();
    
    // Processing
    void process(const AudioBuffer& input, AudioBuffer& output,
                 uint32_t frames, uint64_t sample_position);
    
    // Bypass
    void set_bypass(SlotID id, bool bypass);
    void set_chain_bypass(bool bypass);
    
    // Mix
    void set_mix(SlotID id, double wet);  // 0.0-1.0
    double mix(SlotID id) const;
    
    // Latency
    uint32_t total_latency() const;
    
private:
    struct Slot {
        PluginID plugin;
        bool bypassed = false;
        double mix = 1.0;
        uint32_t latency = 0;
        
        // Dry/wet buffers (for mix control)
        AudioBuffer dry_buffer;
        AudioBuffer wet_buffer;
    };
    
    std::vector<Slot> slots_;
    bool chain_bypassed_ = false;
};
```

### 6.2. Built-In EQ

```cpp
class BuiltInEQ {
public:
    // EQ bands (up to 8 bands)
    uint32_t add_band(BandType type, double frequency, double gain, double q);
    void remove_band(uint32_t index);
    void modify_band(uint32_t index, double frequency, double gain, double q);
    void clear();
    uint32_t band_count() const;
    
    // Processing
    void process(const AudioBuffer& input, AudioBuffer& output,
                 uint32_t frames);
    
    // Bypass
    void set_bypass(bool bypass);
    bool is_bypassed() const;
    
    // Zero-latency (always)
    uint32_t latency() const { return 0; }
    
    enum class BandType {
        LowShelf,
        HighShelf,
        Peak,
        LowCut,
        HighCut,
        BandPass,
        Notch
    };
    
    struct Band {
        BandType type;
        double frequency;       // 20-20000 Hz
        double gain_db = 0.0;  // -36 to +36 dB
        double q = 0.707;       // 0.1 - 10
        bool active = true;
    };
    
private:
    std::vector<Band> bands_;
    bool bypassed_ = false;
    
    // Pre-computed filter coefficients
    std::vector<BiquadFilter> filters_;
    void update_coefficients();
};
```

### 6.3. FX Rack UI

```
┌───────────────────────────────────────────────────────────────┐
│  Mixer: Channel 3 — Bass                                      │
├───────────────────────────────────────────────────────────────┤
│  EQ (Built-in) ──── [Bypass] [Reset]                          │
│  ┌───────────────────────────────────────────────────────────┐│
│  │  +12 ─────┐        ┌──────┐                              ││
│  │    0 ─────┼────────┼──────┼──────────────────────────    ││
│  │  -12 ─────┘        └──────┘                              ││
│  │    20    100    500    2k     10k     20k                 ││
│  └───────────────────────────────────────────────────────────┘│
│  │ LowCut: 40Hz     │ Bass: +3dB @ 100Hz Q:0.7              │
│  │ HighCut: 18kHz   │ Presence: +1.5dB @ 3kHz Q:1.2        │
├───────────────────────────────────────────────────────────────┤
│  FX Rack ────────────────────────────────────── [Add Plugin] │
│  ┌───────────────────────────────────────────────────────────┐│
│  │ 1. Compressor ⏻           Mix: 70% [──●──────────]       ││
│  │ 2. Saturator  ⏻           Mix: 40% [───●─────────]       ││
│  └───────────────────────────────────────────────────────────┘│
└───────────────────────────────────────────────────────────────┘
```

---

## 7. Sends & Returns

### 7.1. Send Architecture

```cpp
class SendManager {
public:
    SendID create_send(ChannelID source, const SendConfig& config);
    void destroy_send(SendID id);
    
    // Process sends (called before return processing)
    void process_sends(uint32_t frames);
    
    struct SendConfig {
        ChannelID source;
        RouteTarget destination;   // Bus or channel
        double level_db = -∞;      // Send level
        bool pre_fader = true;     // Pre or post fader
        bool pre_fx = false;       // Pre or post FX
        bool stereo = true;        // Mono or stereo send
    };
    
private:
    struct Send {
        SendConfig config;
        AudioBuffer buffer;        // Accumulated send audio
    };
    
    std::unordered_map<SendID, Send> sends_;
};
```

### 7.2. Send Configuration

```
Send routing options:
    ┌────────────────────────────────────────────┐
    │ Send Configuration                         │
    │                                            │
    │ Source: Channel 3 (Bass)                   │
    │ Destination: Return "Reverb"               │
    │                                            │
    │ Level:  ────●───────────────  -12 dB      │
    │                                            │
    │ Position:                                 │
    │ ○ Pre-FX (before EQ and FX chain)          │
    │ ● Post-FX / Pre-Fader (default)            │
    │ ○ Post-Fader                               │
    │                                            │
    │ Format: ○ Mono  ● Stereo                   │
    └────────────────────────────────────────────┘
```

### 7.3. Return Channels

```cpp
class ReturnChannel : public Channel {
public:
    ReturnChannel();
    
    // Returns receive from sends, process FX, output to master
    // Always post-fader (send level controls input)
    // Return fader controls level of processed signal
    
    // Common return uses:
    // - Reverb (100% wet)
    // - Delay (100% wet)
    // - Compressor (parallel compression)
    // - Distortion (parallel saturation)
    
    // Solo-safe: returns are NOT affected by track solo
    // (soloing a track does not mute reverb return)
    
    void set_mute_on_solo(bool mute);  // Whether to mute when no tracks soloed
};
```

---

## 8. Sidechain

### 8.1. Sidechain Architecture

```cpp
class SidechainManager {
public:
    // Configure sidechain
    void set_sidechain_source(ChannelID target, AudioInput source);
    void remove_sidechain(ChannelID target);
    
    // Process sidechain routing
    void process_sidechain(uint32_t frames);
    
    // Sidechain is simply routing audio from one channel
    // to another channel's sidechain input.
    // The target channel's compressor/plugin uses this
    // as the detection signal.
    
    // Example: Kick → Sidechain → Bass Compressor
    // Bass compressor ducks when kick hits
    
    struct SidechainConfig {
        ChannelID source_channel;
        ChannelID target_channel;
        bool listen = false;             // Hear sidechain signal
        uint32_t listen_channel = 0;     // Which channel to listen to
    };
    
private:
    std::vector<SidechainConfig> configs_;
    
    // Sidechain buffer storage
    std::unordered_map<ChannelID, AudioBuffer> sidechain_buffers_;
};
```

### 8.2. Sidechain Workflow

```
1. Route: Kick → Sidechain → Bass Compressor
2. Bass compressor detection mode: "Sidechain"
3. Sidechain signal: kick drum audio
4. Result: bass volume ducks on each kick hit

Visual feedback in mixer:
    ┌─────────────┐
    │ Bass        │
    │ ┌─────────┐ │  ← Volume fader
    │ │         │ │
    │ │ ████████│ │
    │ └─────────┘ │
    │ [SC] [Side] │  ← Sidechain indicator (flashing)
    └─────────────┘

    Sidechain source indicator:
    [Kick ◄╌╌╌╌ Bass]  ← Arrow showing signal flow
```

---

## 9. Metering

### 9.1. Meter Types

| Meter | Type | Update | Range | Purpose |
|---|---|---|---|---|
| **Peak** | Sample-level | Every buffer | -60 to 0 dBFS | Prevent clipping |
| **RMS** | Sliding window (300ms) | Every buffer | -60 to 0 dBFS | Perceived loudness |
| **LUFS-S** | Short-term (3s) | Every 100ms | -70 to 0 LUFS | Streaming loudness |
| **LUFS-I** | Integrated (full track) | Continuous | -70 to 0 LUFS | Broadcast compliance |
| **Phase Correlation** | Stereo phase | Every buffer | -1 to +1 | Mono compatibility |
| **Spectrum** | FFT-based | Every 512 samples | 20-20kHz | Frequency analysis |
| **Stereo Field** | Lissajous | Every buffer | — | Stereo imaging |
| **Dynamic Range** | Crest factor | Every buffer | 0-60 dB | Dynamic range |

### 9.2. Channel Metering

```cpp
class ChannelMetering {
public:
    // Called from audio thread
    void process_peak(const float* samples, uint32_t frames, uint32_t channels);
    void process_rms(const float* samples, uint32_t frames);
    void process_lufs(const float* samples, uint32_t frames);
    
    // Read from any thread
    struct Levels {
        float peak_left = -60.0f;
        float peak_right = -60.0f;
        float peak_hold = -60.0f;
        float rms_left = -60.0f;
        float rms_right = -60.0f;
        float crest_factor = 0.0f;     // Peak - RMS (dB)
        float phase_correlation = 1.0f;
        bool clipping = false;
    };
    Levels read_levels() const;
    
    // Hold peak (reset)
    void reset_hold_peak();
    
    // True peak (oversample 4x)
    float true_peak(const float* samples, uint32_t frames);
    
    // LUFS (loudness)
    struct LUFS {
        float momentary = -70.0f;  // 400ms window
        float short_term = -70.0f; // 3s window
        float integrated = -70.0f; // Full duration
        float range = 0.0f;        // Loudness range
        float true_peak = -70.0f;  // True peak max
    };
    LUFS read_lufs() const;
    
    // Spectrum
    struct Spectrum {
        float bins[2048];          // FFT magnitude bins
        float max_bin;             // Max magnitude
        uint32_t max_bin_index;    // Index of max
        float dominant_frequency;  // Hz
    };
    Spectrum read_spectrum() const;

private:
    // Peak detection
    std::atomic<float> peak_left_{-60.0f};
    std::atomic<float> peak_right_{-60.0f};
    std::atomic<float> peak_hold_left_{-60.0f};
    std::atomic<float> peak_hold_right_{-60.0f};
    std::atomic<float> rms_left_{-60.0f};
    std::atomic<float> rms_right_{-60.0f};
    std::atomic<float> crest_factor_{0.0f};
    std::atomic<float> phase_corr_{1.0f};
    std::atomic<bool> clipping_{false};
    
    // Decay filter for visual falloff
    struct DecayFilter {
        float current = -60.0f;
        float decay_rate;  // dB/second
        void apply(float input);
    };
    DecayFilter peak_decay_{40.0f};  // 40 dB/s falloff
    DecayFilter hold_decay_{2.0f};   // Very slow hold falloff
};
```

### 9.3. Meter Display

```
Channel Meter Display:
┌─────────────────┐
│     Peak         │
│  0 dB ───────── │
│                 │
│ -6  ████████    │  ← Peak bar (instant + hold)
│ -12 ████████    │
│ -18 ████████    │
│ -24 ████████    │
│ -30 ████████    │
│ -36 ████████    │
│ -42 ████████    │
│ -48 ████████    │
│ -54 ████████    │
│                 │
│     RMS         │
│  ██████░        │  ← RMS (shorter bar)
│                 │
│ [CLIP] [48]     │  ← Clip indicator + hold value
└─────────────────┘

Master Meter:
┌──────────────────────────────────────┐
│  Left  ████████████████████████░    │
│  Right ████████████████████████░    │
│  ────────────────────────────────── │
│  LUFS: -14.2 LUFS-I  │  TP: -8.1  │
│  Phase: +0.92        │  Dyn: 28dB │
└──────────────────────────────────────┘
```

---

## 10. Master Channel

### 10.1. Master Processing

```cpp
class MasterChannel : public Channel {
public:
    MasterChannel();
    
    // Master FX chain
    FXChain& master_fx();
    const FXChain& master_fx() const;
    
    // Output configuration
    void set_hardware_output(const AudioOutputConfig& config);
    AudioOutputConfig hardware_output() const;
    
    // Monitor output (separate from main output)
    void set_monitor_output(const AudioOutputConfig& config);
    AudioOutputConfig monitor_output() const;
    
    // Dithering
    enum class DitherType {
        None,
        Triangular,         // TPDF dither
        ShapedNoise,        // Noise-shaped (Spectral)
        Apodized            // Apodized noise (high-quality)
    };
    void set_dither(DitherType type, uint16_t target_bit_depth);
    DitherType dither() const;
    uint16_t target_bit_depth() const;
    
    // Output format
    void set_output_format(OutputFormat format);
    OutputFormat output_format() const;
    
    // Limiter (built-in, always on for protection)
    struct LimiterConfig {
        bool enabled = true;
        double threshold_db = -1.0;  // -0.1 to -6 dB
        double ceiling_db = -0.3;    // Output ceiling
        double release_ms = 100.0;   // Release time
    };
    void set_limiter(const LimiterConfig& config);
    LimiterConfig limiter() const;
    
    // Global metering
    const GlobalMetering& global_metering() const;

private:
    FXChain master_fx_;
    AudioOutputConfig hardware_output_;
    AudioOutputConfig monitor_output_;
    
    DitherType dither_ = DitherType::None;
    uint16_t target_bit_depth_ = 24;
    OutputFormat output_format_ = OutputFormat::Stereo;
    
    LimiterConfig limiter_;
    BrickwallLimiter limiter_dsp_;
    
    GlobalMetering global_metering_;
};

struct AudioOutputConfig {
    std::string device_id;
    uint32_t first_channel = 0;
    uint32_t channel_count = 2;     // 2 = stereo
    SampleRate sample_rate = SampleRate::SR_48000;
    
    // Monitor mix (cue mix)
    struct MonitorMix {
        std::vector<ChannelID> channels;  // Channels in monitor mix
        double volume = 0.0;              // dB
    };
    std::optional<MonitorMix> monitor_mix;
};
```

### 10.2. Master Channel Processing Order

```
1. Sum all routed channels/buses into master bus
2. Master FX chain (EQ, compressor, limiter)
3. Master volume fader
4. Bypass/trim for metering
5. Metering analysis
6. Dither (if exporting)
7. Hardware output mapping

In parallel:
    Monitor output (separate mix for headphones)
    Spectrum analyzer
    LUFS analysis
```

---

## 11. Mixer UI

### 11.1. Mixer Console

```
Mixer Console View:
┌────────────────────────────────────────────────────────────────────────────┐
│ Mixer  │  + Add Channel  │  [Reset Levels]  │  [Layout ▼]                 │
├────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┤
│    │    │    │    │    │    │    │    │    │    │    │    │    │    │    │
│ -6 │ -3 │  0 │  -6│ -9 │-12 │ -6 │  0 │  -3│ -6 │ -9 │-12 │ -6 │ -3 │  0 │
│ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │
│ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │
│ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │
│ ██ │ ██ │    │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │ ██ │
│ ██ │ ██ │    │ ██ │    │    │ ██ │    │    │ ██ │    │    │ ██ │ ██ │ ██ │
│ ██ │ ██ │    │ ██ │    │    │ ██ │    │    │ ██ │    │    │ ██ │ ██ │ ██ │
│    │    │    │    │    │    │    │    │    │    │    │    │    │    │    │
│ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │
│ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │  │
│ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │  │
│ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │ ││ │  │
│ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │ ── │
│ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │ ⏻  │
├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┤
│Kick │Snr │HH  │Bass│Syn │Pad │Vox │Gtr │Rvb │Dly │Bus1│Bus2│Ret1│Ret2│Mstr│
└────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
```

### 11.2. Layout Modes

```cpp
class MixerLayout {
public:
    enum class Mode {
        Console,        // Horizontal channel strips (Ableton-style)
        Track,          // Integrated with track view per-channel
        Floating,       // Detached mixer window
        Fullscreen      // Full screen mixer
    };
    
    enum class Visibility {
        All,            // All channels visible
        Grouped,        // Group headers visible, children collapsible
        SelectedOnly,   // Only selected/orange channels
        Custom          // User-defined visible set
    };
    
    void set_mode(Mode mode);
    void set_visibility(Visibility vis);
    void set_channel_width(ChannelWidth width);
    
    enum class ChannelWidth {
        Narrow,     // 48px (meters + fader only)
        Normal,     // 72px (full strip)
        Wide,       // 120px (with EQ preview)
        Full        // 200px (full detail)
    };
    
    // Show/hide sections
    void show_section(Section section, bool show);
    enum class Section {
        Meters, Inputs, EQ, FX, Sends, Routing
    };
};
```

---

## 12. API Reference

### 12.1. Public API

```cpp
class MixerAPI {
public:
    // Channel management
    ChannelID create_channel(const std::string& name, ChannelType type);
    bool delete_channel(ChannelID id);
    Channel* get_channel(ChannelID id);
    std::vector<ChannelID> all_channels() const;
    
    // Master
    MasterChannel* master();
    const MasterChannel* master() const;
    
    // Bus management
    BusID create_bus(const std::string& name);
    bool delete_bus(BusID id);
    std::vector<BusID> all_buses() const;
    
    // Sends
    SendID create_send(ChannelID source, ChannelID destination, bool pre_fader);
    void delete_send(SendID id);
    void set_send_level(SendID id, double db);
    
    // Sidechain
    void set_sidechain(ChannelID source, ChannelID target);
    void remove_sidechain(ChannelID target);
    
    // Metering (for UI)
    ChannelMetering::Levels channel_levels(ChannelID id) const;
    ChannelMetering::LUFS channel_lufs(ChannelID id) const;
    GlobalMetering master_metering() const;
    
    // Processing (called from audio engine)
    void process(uint32_t frames, uint64_t sample_position);
    
    // PDC
    void force_pdc_recalculation();
    uint32_t channel_latency(ChannelID id) const;
    
    // State
    MixerState save_state() const;
    void load_state(const MixerState& state);
};
```

---

## 13. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-MX-001 | Mixer Engine Architecture | 🔲 Pending |
| RFC-MX-002 | Channel Strip (fader, pan, mute, solo) | 🔲 Pending |
| RFC-MX-003 | 64-bit Float Summing Bus | 🔲 Pending |
| RFC-MX-004 | Group Buses & Aux Buses | 🔲 Pending |
| RFC-MX-005 | Built-in EQ (zero-latency) | 🔲 Pending |
| RFC-MX-006 | FX Rack (chain, bypass, mix, latency) | 🔲 Pending |
| RFC-MX-007 | Sends & Returns | 🔲 Pending |
| RFC-MX-008 | Sidechain Routing | 🔲 Pending |
| RFC-MX-009 | Peak/RMS/LUFS Metering | 🔲 Pending |
| RFC-MX-010 | Phase Correlation & Stereo Field | 🔲 Pending |
| RFC-MX-011 | Spectrum Analyzer | 🔲 Pending |
| RFC-MX-012 | Master Channel (limiter, dither) | 🔲 Pending |
| RFC-MX-013 | VCA Fader Control | 🔲 Pending |
| RFC-MX-014 | Monitor Output & Cue Mix | 🔲 Pending |
| RFC-MX-015 | Mixer Layout & UI | 🔲 Pending |
| RFC-MX-016 | Undo/Redo for Mixer Operations | 🔲 Pending |
| RFC-MX-017 | Mixer Automation | 🔲 Pending |
| RFC-MX-018 | Channel Presets & Templates | 🔲 Pending |

---

## Appendix A: Mixer Specifications

| Parameter | Value |
|---|---|
| Internal processing | 64-bit float |
| Channel count | 1024 max |
| Bus count | 128 max |
| Sends per channel | 16 max |
| EQ bands | 8 (built-in) |
| FX slots per channel | 16 max |
| Meter update rate | Per audio buffer |
| LUFS calculation | EBU R128 standard |
| Pan law | -3dB equal-power (default) |
| Fader range | -∞ to +24 dB |
| Fader resolution | 0.1 dB |
| Headroom (default) | 18 dB (at 0dB on meter) |
| Sample rate support | 44.1k - 192k |

## Appendix B: Gain Structure

```
Target levels for proper gain staging:
  ┌─────────────────────────────────────────────────────┐
  │  Individual tracks: -18 dBFS average, -6 dBFS peak  │
  │  Group buses:       -18 dBFS average, -6 dBFS peak  │
  │  Master bus:        -14 dBFS average (for LUFS -14) │
  │  Master output:     -1.0 dBTP max (true peak)       │
  └─────────────────────────────────────────────────────┘
  
  Clip indicator triggers at -0.1 dBFS (before true 0dB)
  Limiter ceiling at -0.3 dBFS (safety margin)
```
