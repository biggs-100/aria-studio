# P1 - Audio Engine: Tasks

> **Status**: In Progress — Slice 1 Complete
> **Priority**: P0 (Required for v1)

---

## T-101: Audio buffer types and lock-free pool

**Status**: ✅ Complete
**Effort**: Medium
**Dependencies**: P0 (kernel, build system)
**Reference**: `04_Audio_Engine.md` §4

### Description
Implement AudioBuffer (multi-channel float), MixBuffer (64-bit double), and LockFreeBufferPool (pre-allocated ring buffer). SIMD-accelerated clear, copy, add, gain operations.

### Acceptance Criteria
- [x] `AudioBuffer` with per-channel float* access, up to 32 channels
- [x] `MixBuffer` with 64-bit double internal precision
- [x] `LockFreeBufferPool` with acquire/release (lock-free)
- [x] SIMD buffer ops: clear, copy, add, multiply_gain, add_with_gain
- [x] SSE2 and scalar implementations
- [x] Unit tests for all buffer operations
- [x] No allocations on audio thread path

---

## T-102: AudioDriver abstraction + ASIO driver

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-101
**Reference**: `04_Audio_Engine.md` §2

### Description
Implement `AudioDriver` abstract class and ASIO driver for Windows. Device enumeration, open/start/stop/close lifecycle.

### Acceptance Criteria
- [ ] `AudioDriver` abstract base with pure virtual interface
- [ ] Device discovery: `enumerate_devices()` returns `AudioDeviceInfo[]`
- [ ] ASIO driver: loads DLL, queries channels/sample-rates/buffers
- [ ] ASIO callback invokes `on_process(input, output, frames)`
- [ ] Device selection and configuration

---

## T-103: WASAPI driver (Windows)

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-101
**Reference**: `04_Audio_Engine.md` §2

### Description
Implement WASAPI driver with exclusive and shared modes for Windows fallback.

### Acceptance Criteria
- [ ] WASAPI exclusive mode (low-latency)
- [ ] WASAPI shared mode (system sound mixing)
- [ ] Device enumeration via MMDevice API
- [ ] Sample rate and buffer size configuration
- [ ] Callback-based processing (event-driven)

---

## T-104: CoreAudio driver (macOS)

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-101
**Reference**: `04_Audio_Engine.md` §2

### Description
Implement CoreAudio driver with AudioUnit I/O for macOS.

### Acceptance Criteria
- [ ] AudioUnit I/O setup with RemoteIO unit
- [ ] Device enumeration via AudioObjectGetPropertyData
- [ ] Configurable sample rate and buffer size
- [ ] Callback invokes `on_process`
- [ ] Aggregate device support

---

## T-105: ALSA/PipeWire driver (Linux)

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-101
**Reference**: `04_Audio_Engine.md` §2

### Description
Implement ALSA and PipeWire drivers for Linux.

### Acceptance Criteria
- [ ] ALSA: device enumeration, hw/sw params, mmap or read/write
- [ ] PipeWire: main-loop based processing
- [ ] Configurable sample rate and buffer size
- [ ] Fallback chain: PipeWire → ALSA

---

## T-106: AudioGraph DAG

**Status**: ✅ Complete
**Effort**: Large
**Dependencies**: T-101
**Reference**: `04_Audio_Engine.md` §3

### Description
Implement the audio processing graph as a DAG with topological sort, node types, and lock-free parameter updates. Atomic graph swap on structural changes.

### Acceptance Criteria
- [x] `AudioGraph` class with `add_node`, `remove_node`, `connect`, `disconnect`
- [x] Topological sort on rebuild
- [x] Cycle detection
- [x] `AudioNode` base class: virtual `process(ProcessContext&)`
- [x] Node types: `AudioInputNode`, `AudioOutputNode`, `GainNode`, `MeterNode`, `SilenceNode`
- [x] Lock-free parameter updates via `AtomicParameter` double-buffer
- [ ] Atomic graph swap: old graph kept alive for 2 callbacks *(not implemented — deferred to Slice 3)*
- [x] Graph rebuild triggers on structural change
- [x] Unit tests for graph topology

---

## T-107: Audio clock and transport

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-106
**Reference**: `04_Audio_Engine.md` §5

### Description
Implement sample-accurate audio clock and transport state machine (play/stop/record/pause). Tempo map with binary search, loop/punch ranges.

### Acceptance Criteria
- [ ] `AudioClock` with `sample_position()`, `current_bpm()`, `current_bar_beat()`
- [ ] Transport state machine: Stop, Play, Record, Pause
- [ ] `TempoMap` with ordered `TempoEvent[]` and binary search
- [ ] Loop range (start/end)
- [ ] Punch range for recording
- [ ] Unit tests for tempo map queries

---

## T-108: Audio graph processing pipeline

**Status**: 🔲 Pending
**Effort**: Large
**Dependencies**: T-106, T-107
**Reference**: `04_Audio_Engine.md` §6

### Description
Wire up the full audio callback: transport check → graph processing → clock update → control messages → metering → x-run detection.

### Acceptance Criteria
- [ ] Full callback flow implemented per spec §6.1
- [ ] Track processing: clip reading, gain, output
- [ ] Recording pipeline: input → ring buffer → clip
- [ ] Control message processing (lock-free queue)
- [ ] Meter update (peak, RMS)
- [ ] X-run detection and counter
- [ ] Integration test: process 1000 buffers without glitch

---

## T-109: PDC (Plugin Delay Compensation)

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-106
**Reference**: `04_Audio_Engine.md` §7

### Description
Implement plugin delay compensation: track latency tracking, maximum latency calculation, delay buffers for early tracks.

### Acceptance Criteria
- [ ] `PluginDelayCompensation` calculates per-track latency
- [ ] Delay buffer for early tracks (align to max latency)
- [ ] PDC recalculation on graph change
- [ ] Unit tests for alignment

---

## T-110: Multi-core audio distribution

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-108
**Reference**: `04_Audio_Engine.md` §8

### Description
Distribute track processing across CPU cores for parallel execution. Balance load by track processing time.

### Acceptance Criteria
- [ ] Phase 1 (serial): input + output + mixer on main audio thread
- [ ] Phase 2 (parallel): track processing distributed across cores
- [ ] Phase 3 (serial): master summing
- [ ] Load balancing: group tracks by processing time
- [ ] Single-core fallback (serial only, no overhead)

---

## T-111: SIMD dispatch and optimization

**Status**: ✅ Complete
**Effort**: Medium
**Dependencies**: T-101
**Reference**: `04_Audio_Engine.md` §9, `10_DSP.md` §2

### Description
Implement runtime CPU feature detection and SIMD dispatch for all hot-path operations. SSE2, AVX, AVX2, AVX512, NEON.

### Acceptance Criteria
- [x] `SimdLevel` enum with runtime detection
- [x] `dispatch_simd()` via function pointer dispatch table
- [x] SIMD ops: add, multiply, copy, clear, peak, RMS
- [ ] AVX2 FMA for fused multiply-add *(not implemented — deferred to Slice 3)*
- [x] Unit tests: SIMD output matches scalar exactly

---

## T-112: Export / offline renderer

**Status**: 🔲 Pending
**Effort**: Large
**Dependencies**: T-108
**Reference**: `04_Audio_Engine.md` §11

### Description
Implement offline rendering (non-real-time, up to 100x speed) and file export. WAV, AIFF, FLAC, MP3, OGG. Dithering.

### Acceptance Criteria
- [ ] `OfflineRenderer` processes same graph as real-time, no clock constraint
- [ ] WAV export (16, 24, 32f, 64f)
- [ ] AIFF export
- [ ] FLAC export (lossless compression)
- [ ] MP3 export (via libmp3lame)
- [ ] OGG Vorbis export
- [ ] Dither: None, Triangular, Shaped
- [ ] Normalize: None, Peak, LUFS
- [ ] Export range: loop, selection, entire project
- [ ] Integration test: export → reimport → sample-exact match

---

## T-113: Audio metering and analysis

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-106
**Reference**: `04_Audio_Engine.md` §12

### Description
Implement meter types: peak, RMS, LUFS (EBU R128), phase correlation, spectrum analyzer.

### Acceptance Criteria
- [ ] `PeakMeter` with instant peak + hold + decay (300ms)
- [ ] `RMSMeter` with 300ms sliding window
- [ ] `LUFSMeter` (momentary, short-term, integrated) per EBU R128
- [ ] Phase correlation (-1 to +1)
- [ ] Spectrum analyzer (FFT-based, configurable FFT size)
- [ ] Thread-safe reads (atomic)
- [ ] Unit tests against known signals

---

## T-114: SRC and time stretching

**Status**: 🔲 Pending
**Effort**: Medium
**Dependencies**: T-106
**Reference**: `04_Audio_Engine.md` §10

### Description
Implement sample rate conversion (project ≠ device, clip ≠ project) and basic time stretching.

### Acceptance Criteria
- [ ] `SampleRateConverter` with 4 quality levels (Fast→Best)
- [ ] SRC at graph input/output when project SR ≠ device SR
- [ ] Clip SRC at read time
- [ ] 64-point sinc for best quality (offline)

---

## T-115: Error recovery (x-run detection)

**Status**: 🔲 Pending
**Effort**: Small
**Dependencies**: T-108
**Reference**: `04_Audio_Engine.md` §13

### Description
Implement x-run detection, counter, and automatic buffer size increase on burst x-runs. Device disconnection handling.

### Acceptance Criteria
- [ ] `AudioDiagnostics` with begin/end callback timing
- [ ] x-run counter
- [ ] Auto buffer size increase (2x) on burst (5+ in 1s)
- [ ] Device disconnection → stop transport, show notification
- [ ] Buffer underflow → silence fill (invisible)

---

## Task Dependencies

```
T-101 (Buffer pool)
  ├── T-102 (ASIO driver)
  ├── T-103 (WASAPI driver)     ← parallel
  ├── T-104 (CoreAudio driver)
  ├── T-105 (ALSA/PipeWire driver)
  ├── T-106 (AudioGraph)
  │   ├── T-107 (Transport)
  │   │   └── T-108 (Pipeline)
  │   │       ├── T-110 (Multi-core)
  │   │       ├── T-112 (Export)
  │   │       └── T-115 (X-run)
  │   ├── T-109 (PDC)
  │   ├── T-113 (Metering)
  │   └── T-114 (SRC)
  └── T-111 (SIMD dispatch)
```

---

## Review Workload Forecast

- **Estimated changed lines**: ~8,000
- **Files changed**: ~60
- **400-line budget risk**: High — exceeds 800-line limit
- **Decision needed before apply**: Yes — SP1 exceeds review budget

Chained PRs recommended: Yes — split into 3-4 slices:
- Slice 1: Buffer pool + SIMD + AudioGraph core (T-101, T-111, T-106)
- Slice 2: Drivers + Transport (T-102, T-103, T-104, T-105, T-107)
- Slice 3: Pipeline + PDC + Multi-core (T-108, T-109, T-110)
- Slice 4: Export + Metering + SRC + X-run (T-112, T-113, T-114, T-115)
