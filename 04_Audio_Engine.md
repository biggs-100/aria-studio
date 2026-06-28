# ARIA DAW — Audio Engine Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23
> **Real-Time Safety**: Strict — no allocations, no locks, no I/O on audio thread

---

## Table of Contents

1. [Overview](#1-overview)
2. [Audio Device Abstraction](#2-audio-device-abstraction)
3. [Audio Graph](#3-audio-graph)
4. [Buffer Management](#4-buffer-management)
5. [Audio Clock & Transport](#5-audio-clock--transport)
6. [Processing Pipeline](#6-processing-pipeline)
7. [Latency Management](#7-latency-management)
8. [Multi-Core Distribution](#8-multi-core-distribution)
9. [SIMD & Optimization](#9-simd--optimization)
10. [Sample Rate Conversion](#10-sample-rate-conversion)
11. [Offline Rendering & Export](#11-offline-rendering--export)
12. [Metering & Analysis](#12-metering--analysis)
13. [Error Recovery](#13-error-recovery)
14. [API Reference](#14-api-reference)
15. [RFC Index](#15-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Audio Engine is the heart of ARIA DAW. It is responsible for:

- Real-time multi-track audio recording and playback
- Sample-accurate audio graph processing
- Low-latency audio I/O through multiple driver APIs
- Internal mixing at 64-bit float precision
- SIMD-accelerated DSP operations
- Offline bounce and stem export

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Round-trip latency | < 10ms at 64-sample buffer |
| Max tracks | 1000+ simultaneous audio tracks |
| Internal precision | 64-bit float throughout the mix |
| Sample rates | 44.1k, 48k, 88.2k, 96k, 176.4k, 192k |
| Bit depths | 16, 24, 32-float, 64-float |
| CPU idle | < 5% with empty project |
| Audio thread safety | Strict real-time (no allocations, no locks) |

### 1.3. Real-Time Contract

The audio engine operates under **strict real-time constraints**:

```
The audio callback is invoked by the hardware driver on a high-priority thread.
The callback MUST complete within one buffer period.
Any violation causes an audio glitch (x-run).

Rules:
  ✓ Lock-free synchronization only
  ✓ Pre-allocated memory only
  ✓ Deterministic execution paths
  ✓ No syscalls (except known-safe)
  ✓ No file I/O
  ✓ No network I/O
  ✓ No dynamic memory allocation
  ✓ No mutexes or blocking primitives
```

---

## 2. Audio Device Abstraction

### 2.1. Driver Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    AudioEngine                            │
│                                                           │
│  ┌────────────────────────────────────────────────────┐  │
│  │               AudioDriver (Abstract)                │  │
│  │  +open(sample_rate, buffer_size) → Result           │  │
│  │  +start() → Result                                  │  │
│  │  +stop() → Result                                   │  │
│  │  +close() → Result                                  │  │
│  │  +current_sample_rate() → uint32_t                  │  │
│  │  +current_buffer_size() → uint32_t                  │  │
│  │  +input_channels() → uint32_t                       │  │
│  │  +output_channels() → uint32_t                      │  │
│  │                                                     │  │
│  │  Callback (set by AudioEngine):                     │  │
│  │  on_process(input_buffer, output_buffer, frames)    │  │
│  └────────────────────────────────────────────────────┘  │
│                   │            │            │             │
│         ┌─────────┘            │            └─────────┐   │
│         ▼                      ▼                      ▼   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │  ASIO    │  │ WASAPI   │  │ CoreAudio│  │  ALSA    │ │
│  │  Driver  │  │ Driver   │  │  Driver  │  │  Driver  │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │
│  ┌──────────┐                                            │
│  │ PipeWire │                                            │
│  │  Driver  │                                            │
│  └──────────┘                                            │
└──────────────────────────────────────────────────────────┘
```

### 2.2. Driver Matrix

| Driver | Platform | Latency | Channels | Features |
|---|---|---|---|---|
| **ASIO** | Windows | Very low (2-10ms) | Up to 256 | Multi-client, direct monitoring |
| **WASAPI Exclusive** | Windows | Low (5-15ms) | Up to 32 | Shared/exclusive modes |
| **WASAPI Shared** | Windows | Moderate (20-50ms) | Up to 32 | System sound mixing |
| **CoreAudio** | macOS | Very low (3-10ms) | Up to 256 | Aggregate devices, AU |
| **ALSA** | Linux | Low (5-15ms) | Up to 128 | Direct hardware access |
| **PipeWire** | Linux | Low (5-15ms) | Up to 128 | JACK compatibility |

### 2.3. Driver Selection Strategy

```
1. User-configured driver (saved in preferences)
2. Auto-detect:
   a. Windows: ASIO → WASAPI Exclusive → WASAPI Shared
   b. macOS: CoreAudio
   c. Linux: PipeWire → ALSA
3. Fallback: system default (WASAPI Shared / ALSA default)
```

### 2.4. Device Discovery

```cpp
struct AudioDeviceInfo {
    std::string id;           // Unique device identifier
    std::string name;         // Human-readable name
    uint32_t input_channels;
    uint32_t output_channels;
    std::vector<uint32_t> sample_rates;  // Supported sample rates
    std::vector<uint32_t> buffer_sizes;  // Supported buffer sizes
    bool is_default;
    bool is_usb;              // USB audio device flag
};

class AudioDriver {
    virtual std::vector<AudioDeviceInfo> enumerate_devices() = 0;
    virtual Result open(const AudioDeviceInfo& device,
                        uint32_t sample_rate,
                        uint32_t buffer_size) = 0;
    // ...
};
```

---

## 3. Audio Graph

### 3.1. Graph Architecture

The audio graph is a **directed acyclic graph (DAG)** of audio processing nodes. It is processed in **topological order** every audio callback.

```
┌──────────────────────────────────────────────┐
│                 AudioGraph                    │
│                                               │
│  Properties:                                  │
│    - DAG of processing nodes                  │
│    - Topologically sorted                     │
│    - Sample-accurate timing                   │
│    - Lock-free parameter updates              │
│    - Rebuilds on structural change            │
│                                               │
│  Methods:                                     │
│    process(frames) → void                     │
│    add_node(node) → node_id                   │
│    remove_node(node_id) → void                │
│    connect(source, target, channel_map)       │
│    disconnect(source, target)                 │
│    rebuild() → void (re-sort topology)        │
└──────────────────────────────────────────────┘
```

### 3.2. Node Types

| Node | Purpose | Inputs | Outputs |
|---|---|---|---|
| `AudioInputNode` | Audio interface input | 0 | 1+ (per channel) |
| `AudioOutputNode` | Audio interface output | 1+ | 0 |
| `TrackAudioNode` | Playback audio clip data | 1 | 1 |
| `TrackInstrumentNode` | MIDI → audio via instrument | 1 (MIDI) | 1 (Audio) |
| `PluginNode` | Audio plugin processing | 1+ | 1+ |
| `MixerNode` | Audio summing | 1+ | 1 |
| `FaderNode` | Volume/pan/gain | 1 | 1 |
| `SendNode` | Tap audio to bus/return | 1 | 2 (dry + send) |
| `ReturnNode` | Process return audio | 1 | 1 |
| `SidechainNode` | Sidechain input | 2 (main + side) | 1 |
| `MeterNode` | Level/analysis metering | 1 | 1 (pass-through) |
| `SilenceNode` | Generate silence | 0 | 1 |

### 3.3. Node Interface

```cpp
class AudioNode {
public:
    struct ProcessContext {
        uint32_t frames;                    // Buffer size (samples)
        double sample_rate;                 // Current sample rate
        uint64_t sample_position;           // Global sample position
        const AudioBuffer** inputs;         // Input buffer array
        AudioBuffer** outputs;              // Output buffer array
        uint32_t input_count;
        uint32_t output_count;
        const AutomationState* automation;  // Current automation values
    };

    virtual void process(ProcessContext& ctx) = 0;
    virtual void reset() {}                 // Reset state (on transport stop)
    virtual uint32_t latency() const { return 0; }

    // Parameter access (atomic, real-time safe)
    virtual void set_parameter(uint32_t index, double value);
    virtual double get_parameter(uint32_t index) const;

protected:
    AudioNodeSpec spec;                     // Node type, name, I/O config
    AtomicParameters params;                // Lock-free parameter storage
};
```

### 3.4. Graph Rebuilding

The audio graph must be rebuilt when:

1. A track is added or removed
2. A plugin is inserted or removed
3. Routing changes (sends, sidechains)
4. Track order changes

**Rebuild strategy:**

```
Main Thread:                    Audio Thread:
    │                               │
    ├── Lock graph mutex            │
    ├── Create new graph            │
    ├── Topologically sort          │
    ├── Validate (no cycles)        │
    ├── Install new graph atomically│
    ├── Unlock graph mutex          │
    │                               ├── Next callback: pick up new graph
    │                               ├── Old graph: drain references
    │                               └── Old graph: delete (after 2 callbacks)
```

The old graph is kept alive for 2 audio callbacks after replacement to ensure no audio thread is still referencing it.

---

## 4. Buffer Management

### 4.1. Buffer Types

```cpp
class AudioBuffer {
public:
    uint32_t frames;          // Number of samples per channel
    uint32_t channels;        // Number of interleaved channels
    float* data[32];          // Per-channel pointers (max 32 channels)
    uint32_t capacity;        // Allocated capacity (frames)

    // Access
    float* channel(uint32_t ch);
    const float* channel(uint32_t ch) const;

    // Operations
    void clear();
    void copy_from(const AudioBuffer& src);
    void add_from(const AudioBuffer& src, float gain);
    void apply_gain(float gain);
};

// Internal mixing buffer (64-bit)
class MixBuffer {
public:
    double* data[32];
    uint32_t frames;
    uint32_t channels;
};
```

### 4.2. Buffer Pool

All audio buffers are allocated from a **lock-free buffer pool** initialized at startup:

```cpp
class LockFreeBufferPool {
public:
    // Initialize pool with max capacity
    void init(uint32_t max_buffer_size, uint32_t max_channels, uint32_t pool_size);

    // Lock-free acquire / release
    AudioBuffer* acquire();
    void release(AudioBuffer* buffer);

private:
    // Pre-allocated ring buffer of AudioBuffer objects
    std::array<AudioBuffer, MAX_POOL_SIZE> pool_;
    std::atomic<uint32_t> head_;
    std::atomic<uint32_t> tail_;
};
```

**Pool size:** Configurable, default 1024 buffers. At 48kHz / 128-sample buffer, this allows ~340ms of audio data in flight.

### 4.3. Zero-Copy Paths

```
Audio Input Driver → Direct buffer → Audio Graph → Direct buffer → Audio Output Driver
                        │
                        └──► Record buffer (lock-free queue)
                        └──► Monitoring path (zero-copy)
```

Where possible, buffers are passed as pointers without copying. Copy-on-write is used for recording and processing.

---

## 5. Audio Clock & Transport

### 5.1. Transport State Machine

```
                  ┌─────────┐
                  │  Stop   │
                  └────┬────┘
                       │
            ┌──────────┼──────────┐
            ▼          ▼          ▼
        ┌──────┐  ┌────────┐  ┌────────┐
        │ Play │  │ Record │  │Pause   │
        └──┬───┘  └───┬────┘  └───┬────┘
           │          │           │
           └──────────┼───────────┘
                      ▼
                 ┌────────┐
                 │  Stop  │
                 └────────┘
```

### 5.2. Audio Clock

```cpp
class AudioClock {
public:
    // Called from audio thread every callback
    void process(uint32_t frames);

    // State (atomics, readable from any thread)
    uint64_t sample_position() const;      // Total samples played
    double   current_bpm() const;          // Current tempo (from tempo map)
    double   current_time() const;         // Seconds
    BarBeat  current_bar_beat() const;     // Bars:Beats:Sixteenths

    // Tempo map access (double-buffered for real-time safety)
    void set_tempo_map(const TempoMap& map);
    TempoMap tempo_map() const;

private:
    // Double-buffered for lock-free reads
    struct ClockState {
        uint64_t sample_position;
        double current_time;
        BarBeat bar_beat;
    };
    std::atomic<ClockState> state_;
    DoubleBuffer<TempoMap> tempo_map_;
};
```

### 5.3. Sample-Accurate Timing

The audio clock is sample-accurate. Position is measured in samples since the beginning of the project:

```
sample_position = (bar - 1) * samples_per_bar + (beat - 1) * samples_per_beat + offset_samples
```

Time signature changes are handled by the tempo map, which is an ordered list of tempo/time-sig events:

```cpp
struct TempoEvent {
    uint64_t sample_position;
    double bpm;
    TimeSignature time_signature;
};

class TempoMap {
    std::vector<TempoEvent> events_;
    // Binary search to find tempo at any sample position
    double bpm_at(uint64_t sample_position) const;
};
```

---

## 6. Processing Pipeline

### 6.1. Audio Callback Flow (Detailed)

```
AudioDriver::on_process(input, output, frames)
    │
    ├──► 1. Begin timing measurement
    │
    ├──► 2. Check transport state (atomic)
    │         │
    │         ├──► Playing: process audio
    │         ├──► Recording: input → record buffer + process
    │         └──► Stopped: output silence, process offline queues
    │
    ├──► 3. Process AudioGraph
    │         │
    │         ├──► For each node in topological order:
    │         │       ├── Read input buffers (from previous node)
    │         │       ├── Apply automation parameters (atomic read)
    │         │       ├── node->process(ctx)
    │         │       └── Write output buffers (for next node)
    │         │
    │         └──► Master output → output buffer
    │
    ├──► 4. Update clock
    │         sample_position += frames
    │
    ├──► 5. Process control messages (lock-free queue)
    │         ├── Apply pending parameter changes
    │         ├── Apply pending graph changes (if swapped)
    │         └── Execute pending commands (start/stop, etc.)
    │
    ├──► 6. Update metering (peak hold, RMS)
    │
    ├──► 7. Detect x-run (if processing time > buffer period)
    │
    └──► 8. End timing — report to diagnostics
```

### 6.2. Track Processing Detail

```
TrackAudioNode::process(ctx)
    │
    ├──► Get clip at current position from timeline
    │
    ├──► If no clip: output silence, return
    │
    ├──► Read audio data from clip cache (lock-free)
    │       │
    │       ├──► Clip is in memory cache → direct pointer
    │       └──► Clip is on disk → trigger async load, play silence until ready
    │
    ├──► Apply clip gain and fades
    │
    ├──► Apply time-stretch if clip tempo ≠ project tempo
    │
    ├──► Apply pitch shift if transposed
    │
    ├──► Apply track volume and pan
    │
    └──► Output to next node (track FX or mixer)
```

### 6.3. Recording Pipeline

```
AudioInputNode receives buffer
    │
    ├──► Pass to monitoring path (zero-copy)
    │
    ├──► If armed track is recording:
    │       ├── Copy buffer to recording ring buffer
    │       ├── Append to active clip in memory
    │       ├── Update recording meter (peak)
    │       └── After recording stops:
    │               ├── Finalize clip
    │               ├── Write to disk (async)
    │               └── Notify main thread via event bus
    │
    └──► Pass to audio graph
```

---

## 7. Latency Management

### 7.1. Latency Sources

| Source | Typical Range | Mitigation |
|---|---|---|
| Audio buffer | 0.7ms - 10ms | User-selectable buffer size |
| Driver latency | 0 - 2ms | Driver-specific optimizations |
| Plugin processing | 0 - 10ms | Plugin Delay Compensation (PDC) |
| Plugin PDC | Automatic | Calculated from plugin reported latency |
| Monitoring path | 0ms (direct) | Zero-copy hardware monitoring when available |
| **Total round-trip** | **2ms - 20ms** | |

### 7.2. Buffer Size Selection

```
User preference:
    "Low Latency" → 32 or 64 samples  (recording, live performance)
    "Balanced"    → 128 or 256 samples (general production)
    "High"        → 512 or 1024 samples (mixing, heavy plugin use)
    "Maximum"     → 2048 or 4096 samples (mastering, final mix)
```

### 7.3. Plugin Delay Compensation

PDC ensures all tracks remain sample-aligned even when plugins introduce latency:

```cpp
class PluginDelayCompensation {
public:
    // Called when graph changes
    void recalculate(const AudioGraph& graph);

    // Per-track total latency (sum of all plugins on chain)
    uint32_t track_latency(track_id_t track) const;

    // Maximum latency across all tracks
    uint32_t max_latency() const;

    // Delay buffer for early tracks (compensate to max)
    void apply_compensation(AudioBuffer* buffer, uint32_t track_latency);

private:
    std::unordered_map<track_id_t, uint32_t> track_latencies_;
    uint32_t max_latency_ = 0;
};
```

Early tracks are delayed by `max_latency - track_latency` samples to align with the latest track.

---

## 8. Multi-Core Distribution

### 8.1. Distribution Model

The audio graph can be distributed across multiple CPU cores for parallel processing.

```
Audio Callback (core 0)
    │
    ├──► Phase 1 (serial): Input nodes + output nodes + mixer
    │       Core 0 handles the critical path
    │
    ├──► Phase 2 (parallel): Track processing
    │       Core 0: tracks 0-24
    │       Core 1: tracks 25-49
    │       Core 2: tracks 50-74
    │       Core 3: tracks 75-99
    │
    └──► Phase 3 (serial): Master summing
            Core 0 sums all track outputs
```

### 8.2. Distribution Rules

- Tracks with heavy plugins get their own core if available
- Tracks are grouped by processing time to balance load
- Distribution is recomputed when graph topology changes
- On single-core systems: all processing is serial (no overhead penalty)
- Latency-sensitive paths (monitoring) stay on the main audio thread

---

## 9. SIMD & Optimization

### 9.1. SIMD Operations

All hot-path DSP operations use SIMD acceleration when available:

| Operation | Intrinsic | Latency Gain |
|---|---|---|
| Audio summation (mix) | `_mm256_add_ps` / `_mm512_add_ps` | 4-8x |
| Gain multiplication | `_mm256_mul_ps` | 4-8x |
| Pan law (equal power) | `_mm256_sqrt_ps` + `_mm256_mul_ps` | 4x |
| Sample conversion (int↔float) | `_mm256_cvtepi32_ps` | 4-8x |
| Peak detection | `_mm256_max_ps` | 4x |
| RMS calculation | `_mm256_mul_ps` + `_mm256_hadd_ps` | 4x |
| FIR filtering | `_mm256_fmadd_ps` | 4-8x |
| FFT | SIMD accelerated | 2-4x |

### 9.2. SIMD Dispatch

```cpp
// Runtime CPU feature detection
enum class SimdLevel {
    SCALAR,
    SSE2,
    SSE4_1,
    AVX,
    AVX2,
    AVX512,
    ARM_NEON,
    ARM_NEON64
};

SimdLevel detect_simd_level();

// Dispatching
template<typename Func>
auto dispatch_simd(Func scalar, Func sse, Func avx, Func avx512)
    -> decltype(scalar());
```

### 9.3. Compiler Optimizations

```cmake
# CMake configuration
target_compile_options(audio_engine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2 /fp:fast /O2>
    $<$<CXX_COMPILER_ID:Clang>:-march=native -ffast-math -O3>
    $<$<CXX_COMPILER_ID:GNU>:-march=native -ffast-math -O3>
)
```

---

## 10. Sample Rate Conversion

### 10.1. SRC Strategy

When the project sample rate differs from the audio device sample rate, or when clips are at a different rate, sample rate conversion is needed.

| Scenario | Method | Quality |
|---|---|---|
| Project ≠ Device | SRC at graph input/output | High (multi-stage) |
| Clip ≠ Project | SRC at clip read time | High (multi-stage) |
| Offline export | SRC at final stage | Best (high-quality sinc) |

### 10.2. SRC Implementation

```cpp
class SampleRateConverter {
public:
    enum class Quality {
        Fast,       // Linear interpolation
        Medium,     // 4-point cubic
        High,       // 8-point windowed sinc
        Best        // 64-point windowed sinc (offline only)
    };

    // Process a block of samples
    void process(const float* input, uint32_t input_frames,
                 float* output, uint32_t& output_frames,
                 double ratio);

    // Reset internal state
    void reset();
};
```

Uses librubberband or a custom implementation with SSE/AVX acceleration.

---

## 11. Offline Rendering & Export

### 11.1. Export Pipeline

```
User selects export range and format
    │
    ▼
Configure export settings:
    ├── Range: loop / selection / entire project
    ├── Format: WAV, AIFF, FLAC, MP3, OGG
    ├── Sample rate: 44.1k - 192k
    ├── Bit depth: 16, 24, 32-float, 64-float
    ├── Dither: None / Triangular / Shaped
    └── Normalize: None / Peak / LUFS
    │
    ▼
Create offline audio graph
    │
    ▼
Process in bulk (not real-time):
    ├── Read all clips (no disk streaming latency)
    ├── Process through full graph (including plugins)
    ├── Apply master channel FX
    ├── Apply dithering
    └── Write to file
    │
    ▼
Report: file path, duration, sample rate, bit depth
```

### 11.2. Offline vs Real-Time Processing

| Aspect | Real-Time | Offline |
|---|---|---|
| Speed | 1x (must keep up with clock) | Up to 100x faster |
| SRC quality | High | Best (64-point sinc) |
| Plugin processing | Live | Bypass: cannot use hardware |
| Memory | Constrained | Generous (full clips in RAM) |
| Error handling | Must not miss deadline | Can retry on failure |

### 11.3. Export Formats

| Format | Bit Depth | Compression | Use Case |
|---|---|---|---|
| WAV | 16, 24, 32f, 64f | None | Master export, archival |
| AIFF | 16, 24, 32f | None | macOS compatibility |
| FLAC | 16, 24 | Lossless | Distribution, archival |
| MP3 | — | Lossy | Preview, sharing |
| OGG Vorbis | — | Lossy | Game audio, distribution |

---

## 12. Metering & Analysis

### 12.1. Meter Types

| Meter | Type | Update Rate | Purpose |
|---|---|---|---|
| **Peak** | Sample-level | Every buffer | Prevent clipping |
| **RMS** | Sliding window | Every buffer | Perceived loudness |
| **LUFS** | Gated (K-weighted) | Every 100ms | Broadcast loudness |
| **Phase Correlation** | Sample-level | Every buffer | Mono compatibility |
| **Spectrum** | FFT-based | Every 512 samples | Frequency analysis |
| **Stereo Field** | Lissajous | Every buffer | Stereo imaging |

### 12.2. Peak Meter Implementation

```cpp
class PeakMeter {
public:
    void process(const float* samples, uint32_t frames, uint32_t channel);

    // Get current levels (atomic - read from any thread)
    float current_peak() const;
    float hold_peak() const;      // Max peak since last reset
    float current_rms() const;

    // Reset hold peak
    void reset_hold();

private:
    // Decay time for visual falloff
    static constexpr double DECAY_MS = 300.0;

    std::atomic<float> current_peak_;
    std::atomic<float> hold_peak_;
    std::atomic<float> current_rms_;
    double decay_factor_;
};
```

---

## 13. Error Recovery

### 13.1. X-Run Detection

```cpp
class AudioDiagnostics {
public:
    void begin_callback();      // Called at start of audio callback
    void end_callback();        // Called at end of callback

    uint64_t x_run_count() const;       // Total x-runs this session
    double   max_callback_time() const;  // Worst-case callback duration
    double   avg_callback_time() const;  // Average callback duration
    bool     is_performing() const;      // Currently x-running?

private:
    using Clock = std::chrono::high_resolution_clock;

    std::atomic<uint64_t> x_run_count_;
    std::atomic<double> max_callback_time_;
    Clock::time_point callback_start_;
};
```

### 13.2. Recovery Actions

| Scenario | Immediate Action | User Notification |
|---|---|---|
| Single x-run | Continue, increment counter | None (counter in diagnostics) |
| Burst x-runs (5+ in 1s) | Increase buffer size by 2x | Toast notification: "Audio buffer increased" |
| Device disconnection | Stop transport, close driver | Dialog: "Audio device disconnected" |
| Buffer underflow (playback) | Fill with silence | Invisible |
| Buffer overflow (recording) | Discard overflow samples | Warning in transport bar |

---

## 14. API Reference

### 14.1. Public API (C++)

```cpp
// Audio Engine public interface
class AudioEngine {
public:
    // Lifecycle
    Result init(const AudioEngineConfig& config);
    void shutdown();
    bool is_initialized() const;

    // Device management
    std::vector<AudioDeviceInfo> enumerate_devices();
    Result open_device(const AudioDeviceInfo& device);
    Result close_device();
    AudioDeviceInfo current_device() const;

    // Transport
    void play();
    void stop();
    void record();
    bool is_playing() const;
    bool is_recording() const;

    // Position
    void set_position(uint64_t sample);
    uint64_t sample_position() const;
    double current_time() const;
    BarBeat current_bar_beat() const;

    // Graph
    AudioGraph& graph();
    const AudioGraph& graph() const;

    // Latency
    uint32_t current_latency() const;    // Round-trip in samples
    uint32_t buffer_size() const;
    uint32_t sample_rate() const;

    // Diagnostics
    const AudioDiagnostics& diagnostics() const;
};
```

### 14.2. Configuration

```cpp
struct AudioEngineConfig {
    uint32_t buffer_size = 128;           // Default buffer
    uint32_t sample_rate = 48000;         // Default sample rate
    SimdLevel min_simd = SimdLevel::SSE2; // Minimum SIMD target
    uint32_t buffer_pool_size = 1024;     // Lock-free buffer pool
    bool enable_multicore = true;         // Multi-core distribution
    bool enable_analysis = true;          // Spectrum/metering
    LogLevel log_level = LogLevel::Warn;
};
```

---

## 15. RFC Index

The following RFCs describe individual components of the Audio Engine in detail:

| RFC | Component | Status |
|---|---|---|
| RFC-AE-001 | Audio Callback Architecture | 🔲 Pending |
| RFC-AE-002 | ASIO Driver Implementation | 🔲 Pending |
| RFC-AE-003 | WASAPI Driver Implementation | 🔲 Pending |
| RFC-AE-004 | CoreAudio Driver Implementation | 🔲 Pending |
| RFC-AE-005 | ALSA / PipeWire Driver Implementation | 🔲 Pending |
| RFC-AE-006 | Lock-Free Buffer Pool | 🔲 Pending |
| RFC-AE-007 | Audio Graph Topology & Processing | 🔲 Pending |
| RFC-AE-008 | Sample-Accurate Transport | 🔲 Pending |
| RFC-AE-009 | Tempo Map & Time Signature | 🔲 Pending |
| RFC-AE-010 | Plugin Delay Compensation | 🔲 Pending |
| RFC-AE-011 | Multi-Core Audio Distribution | 🔲 Pending |
| RFC-AE-012 | SIMD Dispatch & Optimization | 🔲 Pending |
| RFC-AE-013 | Sample Rate Conversion | 🔲 Pending |
| RFC-AE-014 | Offline Renderer & Export | 🔲 Pending |
| RFC-AE-015 | Audio Metering & Analysis | 🔲 Pending |
| RFC-AE-016 | X-Run Detection & Recovery | 🔲 Pending |
| RFC-AE-017 | Zero-Copy Monitoring Path | 🔲 Pending |
| RFC-AE-018 | Recording Pipeline | 🔲 Pending |
| RFC-AE-019 | Audio Clip Cache | 🔲 Pending |
| RFC-AE-020 | Multi-Client Device Support | 🔲 Pending |

---

## Appendix A: Performance Benchmarks

| Test | Target | Measurement Method |
|---|---|---|
| Empty project | < 5% CPU at 48kHz/128 buffer | 60-second capture of max CPU |
| 100 audio tracks | < 30% CPU | All tracks playing clips |
| 50 VST3 instances | < 60% CPU | Heavy synth plugins |
| Export 5min project | < 30s (10x real-time) | Full project export |
| X-run rate | 0 in 8 hours | Stress test at 32-sample buffer |
| Project load | < 3s for 100 tracks | Cold load from SSD |

## Appendix B: Error Codes

| Code | Description |
|---|---|
| `AE_OK` | No error |
| `AE_ERR_DEVICE_NOT_FOUND` | Audio device not found |
| `AE_ERR_DEVICE_BUSY` | Device already in use |
| `AE_ERR_UNSUPPORTED_SAMPLE_RATE` | Sample rate not supported by device |
| `AE_ERR_UNSUPPORTED_BUFFER_SIZE` | Buffer size not supported |
| `AE_ERR_XRUN` | Buffer underrun/overrun detected |
| `AE_ERR_GRAPH_CYCLE` | Audio graph contains a cycle |
| `AE_ERR_GRAPH_TOO_DEEP` | Audio graph exceeds max depth |
| `AE_ERR_BUFFER_POOL_EXHAUSTED` | No available buffers in pool |
| `AE_ERR_NOT_INITIALIZED` | Engine not initialized |
