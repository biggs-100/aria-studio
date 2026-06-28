# ARIA DAW — System Architecture

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Scope**: Overall system architecture, module boundaries, interfaces, data flow

---

## Table of Contents

1. [Architectural Philosophy](#1-architectural-philosophy)
2. [System Context](#2-system-context)
3. [Module Map](#3-module-map)
4. [Layer Architecture](#4-layer-architecture)
5. [Inter-Module Communication](#5-inter-module-communication)
6. [Threading Model](#6-threading-model)
7. [Memory Model](#7-memory-model)
8. [Data Flow](#8-data-flow)
9. [Build System Architecture](#9-build-system-architecture)
10. [Multi-Language Integration](#10-multi-language-integration)
11. [Plugin Architecture](#11-plugin-architecture)
12. [UI Architecture](#12-ui-architecture)
13. [Audio Pipeline](#13-audio-pipeline)
14. [Project File Architecture](#14-project-file-architecture)
15. [Error Handling & Recovery](#15-error-handling--recovery)
16. [Security Architecture](#16-security-architecture)
17. [Testing Strategy](#17-testing-strategy)
18. [Deployment & Distribution](#18-deployment--distribution)
19. [Glossary](#19-glossary)

---

## 1. Architectural Philosophy

### 1.1. Core Principles

ARIA DAW is built on four architectural pillars:

1. **Modular Monolith with Clear Boundaries** — Every engine is independently compilable and testable. Modules communicate through defined interfaces, not shared state. The system is distributed as a single application binary, but internally it behaves as a federated system.

2. **Layered Responsibility** — No circular dependencies. The kernel layer knows nothing about the UI. The audio engine knows nothing about file indexing. Each layer has a strict dependency direction.

3. **Real-Time Safety First** — The audio thread is sacred. No allocations, no locks, no I/O on the audio thread. All real-time paths are lock-free and wait-free.

4. **Right Tool for the Right Job** — C++ for real-time and SIMD. Rust for auxiliary services where memory safety matters. TypeScript for declarative UI. Lua for user scripting. Python for AI inference.

### 1.2. Architectural Patterns

| Pattern | Application |
|---|---|
| **Hexagonal Architecture** | Each engine has inbound and outbound ports. Adapters implement platform-specific I/O. |
| **Event-Driven** | Inter-module communication via a typed event bus. Modules publish and subscribe without direct coupling. |
| **Command Query Separation** | Mutations are commands (fire-and-forget or awaitable). Reads are queries (synchronous snapshot). |
| **Data-Oriented Design** | Hot paths (audio, MIDI) process cache-friendly arrays of structs, not polymorphic objects. |
| **Plugin Architecture** | Core engines are themselves replaceable — the audio engine, MIDI engine, etc. have the same plugin-like interface as third-party plugins. |

---

## 2. System Context

### 2.1. High-Level Context Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ARIA DAW Application                        │
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                    UI Layer (TypeScript)                       │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐   │  │
│  │  │Widgets   │ │Panels    │ │Piano Roll│ │Mixer Console   │   │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └────────────────┘   │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                              │ (IPC / FFI)                           │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                    Core Layer (C++)                             │  │
│  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌────────┐             │  │
│  │  │Audio │ │ MIDI │ │ DSP  │ │Plugin│ │Autom.  │             │  │
│  │  │Engine│ │Engine│ │Engine│ │Host  │ │Engine  │             │  │
│  │  └──────┘ └──────┘ └──────┘ └──────┘ └────────┘             │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                              │ (IPC / API)                           │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                 Service Layer (Rust)                            │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐   │  │
│  │  │Database  │ │File Sys  │ │Networking│ │Scripting (Lua) │   │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └────────────────┘   │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                              │ (Process-level)                       │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │              AI Layer (Python subprocess)                       │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐                     │  │
│  │  │Stem Sep  │ │Semantic  │ │Chord Gen │                     │  │
│  │  │          │ │Search    │ │          │                     │  │
│  │  └──────────┘ └──────────┘ └──────────┘                     │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
         │              │              │              │
    ┌────▼────┐   ┌─────▼─────┐  ┌────▼────┐   ┌────▼────┐
    │  Audio  │   │  MIDI     │  │  GPU    │   │  File   │
    │  Device │   │  Device   │  │         │   │  System │
    └─────────┘   └───────────┘  └─────────┘   └─────────┘
```

### 2.2. External Interfaces

| Interface | Direction | Protocol | Description |
|---|---|---|---|
| Audio Device I/O | Bidirectional | ASIO / WASAPI / CoreAudio / ALSA / PipeWire | Real-time audio stream |
| MIDI Device I/O | Bidirectional | MIDI 1.0 / USB MIDI | MIDI input/output |
| GPU | Output | WebGPU (Vulkan/Metal/DX12) | Graphics rendering |
| Plugin API | Bidirectional | CLAP / VST3 / AU / LV2 | Third-party audio plugins |
| File System | Bidirectional | OS-native | Project files, samples, presets |
| Network | Bidirectional | TCP/TLS | Collaboration, updates, cloud sync |
| AI Models | Input | ONNX Runtime / Python subprocess | ML model inference |

---

## 3. Module Map

### 3.1. Complete Module Inventory

```
aria-daw/
│
├── kernel/                          # Kernel — core services & lifecycle
│   ├── application.cc               # Application entry point, lifecycle
│   ├── service_locator.cc           # Service locator (not a DI container — explicit)
│   ├── event_bus.cc                 # Typed event bus
│   ├── command_queue.cc             # Command dispatch (cross-thread safe)
│   ├── undo_stack.cc                # Undo/redo system
│   └── crash_handler.cc             # Crash recovery & diagnostics
│
├── audio/                           # Audio Engine
│   ├── driver/                      # Audio device abstraction
│   │   ├── asio_driver.cc
│   │   ├── wasapi_driver.cc
│   │   ├── coreaudio_driver.cc
│   │   ├── alsa_driver.cc
│   │   └── pipewire_driver.cc
│   ├── graph/                       # Audio processing graph
│   │   ├── audio_graph.cc           # Sample-accurate graph
│   │   ├── node.cc                  # Base audio node
│   │   ├── io_node.cc               # Audio input/output node
│   │   └── fan_node.cc              # Splitter/merger node
│   ├── buffer/                      # Buffer management
│   │   ├── audio_buffer.cc          # Multi-channel audio buffer
│   │   ├── buffer_pool.cc           # Lock-free buffer pool
│   │   └── sample_converter.cc      # Format conversion
│   ├── clock/                       # Audio clock & sync
│   │   ├── audio_clock.cc           # Sample-accurate clock
│   │   ├── tempo_map.cc             # Tempo & time signature
│   │   └── transport.cc             # Play/stop/record state machine
│   └── export/                      # Export engine
│       ├── offline_renderer.cc
│       ├── file_writer.cc
│       └── dither.cc
│
├── midi/                            # MIDI Engine
│   ├── driver/                      # MIDI device abstraction
│   │   ├── midi_input.cc
│   │   └── midi_output.cc
│   ├── graph/                       # MIDI processing graph
│   │   ├── midi_graph.cc
│   │   └── midi_node.cc
│   ├── clock/                       # MIDI clock
│   │   ├── midi_clock.cc
│   │   └── midi_sync.cc
│   └── mpe/                         # MPE support
│       ├── mpe_decoder.cc
│       └── mpe_encoder.cc
│
├── dsp/                             # DSP Engine
│   ├── core/                        # Core DSP primitives
│   │   ├── simd_ops.cc              # SIMD-accelerated operations
│   │   ├── filter.cc                # IIR/FIR filters
│   │   ├── eq.cc                    # Parametric EQ
│   │   ├── compressor.cc            # Dynamics processor
│   │   ├── limiter.cc               # Brickwall limiter
│   │   ├── reverb.cc                # Convolution/algo reverb
│   │   ├── delay.cc                 # Delay/echo
│   │   ├── distortion.cc            # Saturation/distortion
│   │   └── modulation.cc            # Chorus/flanger/phaser
│   ├── analysis/                    # Audio analysis
│   │   ├── fft.cc                   # FFT implementation
│   │   ├── spectrogram.cc           # Spectrogram generation
│   │   ├── pitch_detection.cc       # Pitch detection
│   │   ├── transient_detection.cc   # Transient detection
│   │   └── loudness.cc              # LUFS metering
│   ├── time_stretch/                # Time/pitch manipulation
│   │   ├── rubber_band.cc           # Elastique-like stretching
│   │   └── formant.cc               # Formant preservation
│   └── mixer/                       # Mixer DSP
│       ├── channel_strip.cc
│       ├── summing_bus.cc
│       ├── pan_law.cc
│       └── metering.cc
│
├── plugin/                          # Plugin Host
│   ├── clap/                        # CLAP host implementation
│   │   ├── clap_host.cc
│   │   ├── clap_plugin.cc
│   │   └── clap_extensions.cc       # CLAP extensions support
│   ├── vst3/                        # VST3 host implementation
│   │   ├── vst3_host.cc
│   │   ├── vst3_plugin.cc
│   │   └── vst3_parameters.cc
│   ├── au/                          # Audio Unit host (macOS)
│   │   └── au_host.mm
│   ├── lv2/                         # LV2 host (Linux)
│   │   ├── lv2_host.cc
│   │   └── lv2_features.cc
│   ├── sandbox/                     # Plugin sandboxing
│   │   ├── sandbox_manager.cc
│   │   ├── sandbox_process.cc       # Out-of-process (optional)
│   │   └── crash_isolation.cc
│   ├── native/                      # ARIA's own CLAP-native plugins
│   │   ├── clap_factory.cc          # CLAP plugin factory
│   │   ├── synths/                  # Built-in synthesizers
│   │   ├── effects/                 # Built-in audio effects
│   │   └── utilities/               # Utility plugins
│   └── preset/                      # Preset management
│       ├── preset_db.cc
│       └── preset_format.cc
│
├── automation/                      # Automation Engine
│   ├── envelope.cc                  # Envelope / automation curve
│   ├── bezier.cc                    # Bezier curve math
│   ├── modulation.cc                # Modulation mapping
│   ├── modulation_source.cc         # LFO, envelope, step generator
│   ├── modulation_target.cc         # Parameter binding
│   └── automation_recorder.cc       # Real-time automation recording
│
├── timeline/                        # Timeline & Transport
│   ├── timeline.cc                  # Master timeline
│   ├── transport.cc                 # Transport state (play/stop/record)
│   ├── tempo_map.cc                 # Tempo & time signature track
│   ├── time_signature.cc
│   ├── bar_beat.cc                  # Bar/beat/16th conversion
│   └── loop.cc                      # Loop points
│
├── project/                         # Project Engine
│   ├── project.cc                   # Project document model
│   ├── project_serializer.cc        # Serialization/deserialization
│   ├── project_format.cc            # File format definition
│   ├── project_migration.cc         # Version migration
│   ├── undo_stack.cc                # Undo/redo
│   └── template_manager.cc          # Templates
│
├── model/                           # Data Model
│   ├── track.cc                     # Track model
│   ├── clip.cc                      # Clip model (audio, MIDI, automation)
│   ├── clip_slot.cc                  # Session view clip slot
│   ├── scene.cc                     # Session view scene
│   ├── arrangement.cc               # Arrangement data
│   └── session.cc                   # Session data
│
├── ui/                              # UI Layer (TypeScript)
│   ├── components/                  # Reusable UI components
│   │   ├── Button/
│   │   ├── Slider/
│   │   ├── Knob/
│   │   ├── Meter/
│   │   ├── Track/
│   │   ├── Clip/
│   │   └── Menu/
│   ├── views/                       # Top-level views
│   │   ├── ArrangementView/
│   │   ├── SessionView/
│   │   ├── MixerView/
│   │   ├── PianoRoll/
│   │   └── BrowserView/
│   ├── layout/                      # Layout & docking
│   │   ├── DockManager/
│   │   ├── SplitPanel/
│   │   └── TabPanel/
│   ├── theme/                       # Theming
│   │   ├── ThemeProvider/
│   │   ├── colors/
│   │   └── typography/
│   └── animations/                  # Animation system
│       ├── AnimationEngine/
│       ├── easing/
│       └── transitions/
│
├── graphics/                        # Graphics Engine (C++ + Skia + WebGPU)
│   ├── webgpu/                      # WebGPU abstraction
│   │   ├── device.cc
│   │   ├── swapchain.cc
│   │   ├── shader.cc
│   │   └── pipeline.cc
│   ├── skia/                        # Skia integration
│   │   ├── skia_canvas.cc
│   │   ├── skia_text.cc
│   │   └── skia_path.cc
│   ├── widgets/                     # GPU-rendered widgets
│   │   ├── button.cc
│   │   ├── slider.cc
│   │   ├── text.cc
│   │   └── waveform.cc
│   ├── layout/                      # Layout engine
│   │   ├── layout_tree.cc
│   │   └── flex_layout.cc
│   └── effects/                     # GPU effects
│       ├── blur.cc
│       ├── shadow.cc
│       └── glow.cc
│
├── browser/                         # File Browser & Sample Database
│   ├── file_tree.cc                 # File system tree
│   ├── file_preview.cc              # File preview (waveform, metadata)
│   ├── sample_db.cc                 # SQLite sample database
│   ├── indexer.cc                   # Background indexer
│   ├── tag_manager.cc               # Tag system
│   └── search_engine.cc             # Full-text search
│
├── scripting/                       # Lua Scripting
│   ├── lua_runtime.cc               # Lua VM wrapper
│   ├── lua_api.cc                   # Public API bindings
│   ├── lua_midi.cc                  # MIDI-specific API
│   ├── lua_audio.cc                 # Audio-specific API
│   ├── lua_ui.cc                    # UI extension API
│   ├── macro_recorder.cc            # Macro recording
│   └── script_manager.cc            # Script loading/hot-reload
│
├── ai/                              # AI Engine (Python)
│   ├── ai_bridge.cc                 # C++ ↔ Python bridge
│   ├── ai_subprocess.cc             # Python subprocess manager
│   ├── models/                      # ML models (ONNX, etc.)
│   ├── semantic_search/             # Semantic sample search
│   ├── stem_separation/             # Source separation
│   ├── chord_generation/            # Chord/scale suggestions
│   └── mix_assistant/               # Mix suggestions
│
├── networking/                      # Networking (Rust)
│   ├── sync/                        # Project sync
│   ├── collaboration/               # Real-time collaboration
│   ├── updates/                     # Update checking
│   └── cloud/                       # Cloud storage
│
├── database/                        # Database (Rust)
│   ├── schema.sql                   # SQLite schema
│   ├── connection.cc                # DB connection
│   ├── migrations/                  # Schema migrations
│   └── queries/                     # Query modules
│
├── platform/                        # Platform-specific code
│   ├── windows/
│   ├── macos/
│   └── linux/
│
├── crash/                           # Crash handling & diagnostics
│   ├── crash_reporter.cc
│   ├── minidump.cc
│   └── diagnostics.cc
│
└── tests/                           # Tests
    ├── unit/
    ├── integration/
    ├── performance/
    └── regression/
```

### 3.2. Module Dependency Graph

```
                    ┌──────────┐
                    │ Platform │
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │  kernel  │
                    └────┬─────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
     ┌────▼───┐   ┌─────▼─────┐   ┌────▼────┐
     │ audio  │   │   midi    │   │project  │
     └────┬───┘   └─────┬─────┘   └────┬────┘
          │              │              │
     ┌────▼───┐   ┌─────▼─────┐        │
     │  dsp   │   │ timeline  │◄───────┘
     └────┬───┘   └─────┬─────┘
          │              │
     ┌────▼───┐   ┌─────▼─────┐
     │ plugin │   │automation │
     └────┬───┘   └─────┬─────┘
          │              │
          └──────┬───────┘
                 │
            ┌────▼────┐
            │  model  │
            └────┬────┘
                 │
          ┌──────┴────────┐
          │               │
    ┌─────▼─────┐   ┌────▼──────┐
    │ graphics  │   │  browser  │
    └─────┬─────┘   └────┬──────┘
          │               │
     ┌────▼────┐    ┌────▼──────┐
     │   ui    │    │ database  │
     └─────────┘    └───────────┘

    ┌───────────┐    ┌────────────┐
    │scripting  │    │networking  │
    └───────────┘    └────────────┘
         │                │
         └────────┬───────┘
                  │
             ┌────▼────┐
             │   ai    │
             └─────────┘
```

Rule: **No circular dependencies**. A module at a lower level never imports from a higher level.

---

## 4. Layer Architecture

### 4.1. Layer Definitions

ARIA is organized into four logical layers:

```
Layer 4: Application ─────────────────────────────────────────────────
    UI Framework (TypeScript)
    Views, Widgets, Panels, Piano Roll, Mixer Console
──────────────────────────────────────────────────────────────────────
Layer 3: Core ────────────────────────────────────────────────────────
    Audio Engine, MIDI Engine, DSP Engine, Plugin Host
    Automation Engine, Timeline, Project Engine
    Written in C++. Real-time safe.
──────────────────────────────────────────────────────────────────────
Layer 2: Services ────────────────────────────────────────────────────
    Database (SQLite), File System, Networking, Scripting (Lua)
    Written in Rust. Async, memory-safe, no real-time constraints.
──────────────────────────────────────────────────────────────────────
Layer 1: Intelligence ────────────────────────────────────────────────
    AI Models, Semantic Search, Stem Separation
    Written in Python. Runs as subprocess. Not real-time.
──────────────────────────────────────────────────────────────────────
Layer 0: Platform ────────────────────────────────────────────────────
    OS abstractions: threads, memory, file I/O, windowing
    Platform-specific: ASIO, CoreAudio, DirectX, Metal
```

### 4.2. Layer Communication Rules

| From | To | Mechanism | Latency |
|---|---|---|---|
| UI (Layer 4) | Core (Layer 3) | FFI (C ABI) via TypeScript bindings | Microseconds |
| UI (Layer 4) | Services (Layer 2) | IPC or FFI via Rust C ABI | Microseconds |
| Core (Layer 3) | Services (Layer 2) | C ABI function calls, callback-based | Microseconds |
| Core (Layer 3) | AI (Layer 1) | Subprocess stdin/stdout (JSON/msgpack) | Milliseconds |
| Services (Layer 2) | AI (Layer 1) | Subprocess stdin/stdout | Milliseconds |
| Core (Layer 3) | Plugins | CLAP/VST3/AU C ABI | Real-time safe |

### 4.3. The Kernel

The kernel is the central coordinator. It does NOT implement business logic — it provides the infrastructure for modules to communicate.

**Kernel Responsibilities:**
- Application lifecycle (init, run, shutdown)
- Module registration and discovery
- Thread pool management
- Event bus distribution
- Command dispatch
- Undo stack coordination
- Crash detection and recovery

**Kernel Non-Responsibilities:**
- Audio processing
- MIDI processing
- UI rendering
- File I/O

---

## 5. Inter-Module Communication

### 5.1. Event Bus

The event bus is the primary communication channel between modules. It is **typed**, **thread-safe**, and **lock-free** for the common case.

```
┌─────────────────────────────────────────────────────────────┐
│                        Event Bus                             │
│                                                              │
│  publish<T>(event)  →  [subscriber_1, subscriber_2, ...]     │
│                                                              │
│  subscribe<T>(handler) → subscription_id                     │
│                                                              │
│  Events are value types. Immutable after publication.        │
│  Subscribers run on the publisher's thread by default.       │
│  Async subscribers can be dispatched to a thread pool.       │
└─────────────────────────────────────────────────────────────┘
```

**Event Categories:**

| Category | Examples | Thread |
|---|---|---|
| **Transport** | PlayRequested, Stopped, PositionChanged, LoopChanged | Main thread |
| **Project** | TrackAdded, TrackRemoved, ClipModified, Saved | Main thread |
| **Audio** | DeviceOpened, BufferSizeChanged, SampleRateChanged | Audio thread |
| **MIDI** | NoteOn, NoteOff, CCChanged, ClockTick | MIDI thread |
| **UI** | ThemeChanged, LayoutChanged, ViewChanged | UI thread |
| **Plugin** | PluginLoaded, PluginCrashed, PresetChanged | Main thread |
| **Automation** | EnvelopeModified, ModulationAdded | Main thread |
| **File** | IndexingStarted, IndexingComplete, FileDiscovered | Background |
| **Error** | AudioXRun, PluginCrashReport, DiskFull | Any |

### 5.2. Command Queue

For operations that need a result (not fire-and-forget), modules use the command queue:

```cpp
// Example: Saving a project
auto result = command_queue.dispatch<SaveProject>({
    .path = "/projects/my-track.aria"
});
// result is a future<SaveProjectResult>
```

### 5.3. Audio Thread Communication

The audio thread communicates with the main thread through **lock-free queues**:

```cpp
// Main thread → Audio thread (control messages)
lock_free_queue<AudioControlMessage, 256> audio_control_queue;

// Audio thread → Main thread (status/parameters)
lock_free_queue<AudioStatusMessage, 256> audio_status_queue;
```

### 5.4. Service Layer Interface (Rust C ABI)

The Rust service layer exposes a C-compatible API:

```c
// C ABI exported from the Rust crate
typedef struct DbConnection DbConnection;

DbConnection* db_open(const char* path);
void db_close(DbConnection* conn);
bool db_query_samples(DbConnection* conn,
                      const char* search_query,
                      SampleResult* out_results,
                      size_t* out_count);
```

These are called directly from C++ without serialization overhead.

### 5.5. AI Subprocess Protocol

The AI engine runs as a Python subprocess. Communication is via JSON/msgpack over stdin/stdout:

```json
// Request (C++ → Python)
{
    "id": "req-001",
    "method": "semantic_search",
    "params": {
        "query": "dark kick with sub-bass",
        "limit": 20
    }
}

// Response (Python → C++)
{
    "id": "req-001",
    "result": [
        {"path": "/samples/kicks/dark_kick_01.wav", "score": 0.95},
        {"path": "/samples/kicks/sub_kick_03.wav", "score": 0.87}
    ]
}
```

---

## 6. Threading Model

### 6.1. Thread Map

| Thread | Priority | Responsibility | Language |
|---|---|---|---|
| **Audio Thread** | Highest (real-time) | Audio callback, buffer processing, DSP | C++ |
| **MIDI Thread** | High | MIDI input/output, clock | C++ |
| **UI Thread** | Normal | UI rendering, event dispatch | TypeScript (main) |
| **Main Thread** | Normal | Application logic, project management | C++ |
| **Worker Pool** | Normal-low | Background tasks, file I/O, indexing | C++ / Rust |
| **AI Process** | Low | ML inference | Python |

### 6.2. Audio Thread (Real-Time)

**Rules:**
- No memory allocation after initialization
- No locks (all synchronization is lock-free)
- No I/O (no file access, no network)
- No syscalls (except known-safe ones)
- Deterministic execution time
- Must never block

**Architecture:**
```
Audio Callback (hardware IRQ)
    │
    ▼
AudioGraph::process(buffer_size)
    │
    ├──► Process audio nodes (topological order)
    │       ├── AudioInputNode → buffer
    │       ├── TrackNode → process clip data → buffer
    │       ├── PluginNode → process with plugin
    │       ├── MixerNode → sum all tracks
    │       └── AudioOutputNode → deliver to hardware
    │
    ├──► Check AudioControlQueue for pending commands
    │       └── Apply parameter changes (lock-free)
    │
    └──► Push AudioStatusMessage to status queue
```

### 6.3. Lock-Free Primitives

| Primitive | Usage | Implementation |
|---|---|---|
| `SPSCQueue` | Audio → Main, Main → Audio | Single-producer, single-consumer ring buffer |
| `MPMCQueue` | Worker pool dispatch | Multi-producer, multi-consumer ring buffer |
| `AtomicParameter` | Real-time safe parameter changes | `std::atomic<double>` with double-buffering |
| `ReadWriteFlag` | Module state signaling | `std::atomic<uint32_t>` |
| `LockFreePool` | Audio buffer allocation | Pre-allocated pool with free-list |

---

## 7. Memory Model

### 7.1. Ownership Strategy

ARIA uses a hybrid ownership model:

| Region | Ownership Model | Rationale |
|---|---|---|
| Audio engine | `std::unique_ptr` + raw observers | Single owner, no sharing on audio thread |
| Project model | `std::shared_ptr` (immutable after commit) | Copy-on-write for undo support |
| UI state | Borrowed references via kernel | UI reads, does not own core state |
| Plugin state | Opaque pointers (CLAP/VST3 contract) | Plugins own their own memory |
| GPU resources | RAII wrappers | Resource lifetime tied to render passes |
| Rust services | Rust ownership model | Guaranteed by the compiler |

### 7.2. Audio Memory

```
┌─────────────────────────────────────────────┐
│                Buffer Pool                    │
│                                              │
│  [Pre-allocated at init]                     │
│  [Fixed size: max_buffer_size * num_channels]│
│  [Lock-free allocation/free]                 │
│  [Zero-copy when possible]                   │
└─────────────────────────────────────────────┘
```

- All audio buffers are pre-allocated at initialization
- No heap allocation on the audio thread
- Buffer sizes are powers of two (32, 64, 128, 256, 512, 1024, 2048, 4096)
- Sample format: 32-bit float for processing, 64-bit float for mixing

### 7.3. Undo Memory

Undo uses a **copy-on-write** model:

1. Before a mutation, the affected state is snapshotted
2. The snapshot is stored as a serialized delta
3. Undo restores the delta, redo re-applies it
4. Memory limit: configurable (default: 1GB)
5. Old entries are evicted LRU when the limit is reached

---

## 8. Data Flow

### 8.1. Audio Playback Flow

```
User clicks "Play"
    │
    ▼
Main Thread: Transport::play()
    │
    ▼
Event Bus: publish(PlayRequested)
    │
    ├──► Timeline starts counting
    ├──► Audio Engine activates processing
    └──► UI updates to "playing" state

Audio Callback:
    │
    ▼
AudioGraph::process(128 samples)
    │
    ├──► Timeline::get_position() → current bar/beat
    │
    ├──► For each track:
    │       ├── Get clip at current position
    │       ├── Read clip audio data (lock-free cache)
    │       ├── Process through track FX chain
    │       ├── Apply automation (current parameter values)
    │       └── Send to mixer input
    │
    ├──► Mixer: sum all channels → master
    │
    └──► AudioOutput: deliver to hardware buffer
```

### 8.2. MIDI Recording Flow

```
MIDI Device sends Note On
    │
    ▼
MIDI Input Driver
    │
    ▼
MIDI Engine: timestamp & route
    │
    ├──► MIDI Graph: apply MIDI FX
    │
    ├──► If recording:
    │       ├── MIDI Recorder → Clip buffer
    │       └── Real-time echo to armed track
    │
    └──► If not recording:
            └── Route to selected instrument/device

After recording stops:
    ├──► Clip buffer → Clip model
    ├──► Piano Roll updates
    └──► Undo stack entry created
```

### 8.3. Plugin Processing Flow

```
Track has CLAP synth on its FX chain
    │
    ▼
Audio Graph reaches TrackNode
    │
    ▼
PluginNode::process(buffer)
    │
    ├──► Check sandbox (in-process or out-of-process)
    │
    ├──► Convert buffer to plugin format (if needed)
    │
    ├──► Call clap_plugin.process()
    │       ├── Plugin reads MIDI events from event list
    │       ├── Plugin processes audio
    │       └── Plugin writes output to buffer
    │
    ├──► Convert buffer back to ARIA format
    │
    └──► Continue to next node in chain
```

### 8.4. Project Save Flow

```
User presses Ctrl+S (or auto-save triggers)
    │
    ▼
Command Queue: dispatch<SaveProject>(path)
    │
    ▼
Project::serialize()
    │
    ├──► Snapshot model state
    │
    ├──► Begin transaction in project file
    │
    ├──► Write project header + version
    │
    ├──► For each track:
    │       ├── Track metadata
    │       ├── Clip data (references to audio files, MIDI data inline)
    │       ├── Automation data
    │       ├── Plugin state (preset references)
    │       └── Routing configuration
    │
    ├──► Write mixer state
    │
    ├──► Write arrangement/session data
    │
    ├──► Write CRC32 checksum
    │
    ├──► Commit transaction
    │
    └──► Event Bus: publish(ProjectSaved, path)
```

---

## 9. Build System Architecture

### 9.1. Build Toolchain

| Language | Build System | Compiler | Notes |
|---|---|---|---|
| C++20/23 | CMake 3.28+ | MSVC 2022, Clang 17+, GCC 13+ | `-march=native` for SIMD |
| Rust | Cargo 1.75+ | rustc | `cdylib` crate for C ABI |
| TypeScript | pnpm + esbuild | tsc (type-check only) | Bundled to native via Node/V8 or QuickJS |
| Lua | — | — | Embedded `lua.h` from Lua 5.4 |
| Python | — | — | Subprocess, no build-time dependency |
| SQLite | — | — | Amalgamation, vendored |

### 9.2. Dependency Graph (Build)

```
CMake (C++ Core)
  │
  ├──► Cargo (Rust Services) ──► cdylib (.dll/.so/.dylib)
  │       │
  │       └──► External crates (rusqlite, reqwest, etc.)
  │
  ├──► WebGPU headers
  ├──► Skia (prebuilt or built from source)
  ├──► Lua 5.4 (vended)
  ├──► SQLite (vended)
  └──► pnpm (TypeScript UI)
          │
          └──► Bundled UI assembly (.so/.dll)
```

### 9.3. Build Variants

| Variant | Optimization | Debug Info | LTO | ASAN |
|---|---|---|---|---|
| Debug | `-O0 -g` | Full | No | Optional |
| RelWithDebInfo | `-O2 -g` | Full | Yes | No |
| Release | `-O3 -DNDEBUG` | None | Full | No |
| Distribution | `-O3 -DNDEBUG` | Strip | Full + PGO | No |
| Sanitized | `-O1 -g` | Full | No | Yes |

### 9.4. CI/CD Pipeline

```
┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
│  Commit  │──►│   Lint   │──►│   Build  │──►│   Test   │
└──────────┘   └──────────┘   └──────────┘   └──────────┘
                                                  │
                                          ┌───────┴────────┐
                                          │                 │
                                     ┌────▼────┐     ┌─────▼────┐
                                     │  Unit   │     │Integra-  │
                                     │  Tests  │     │tion Tests│
                                     └─────────┘     └──────────┘
                                          │
                                     ┌────▼────┐
                                     │Perf.    │
                                     │Tests    │
                                     └─────────┘
                                          │
                                     ┌────▼────┐
                                     │ Package │
                                     │ & Sign  │
                                     └─────────┘
```

---

## 10. Multi-Language Integration

### 10.1. C++ ↔ Rust Boundary

```
C++ (caller)                     Rust (callee)
    │                                │
    │  extern "C" {                  │
    │    void* db_open(char* path);  │
    │  }                             │
    │                                │
    │──────► FFI call ──────────►    │
    │                                │
    │◄────── return opaque ptr ──────│
    │                                │
    │  // C++ uses the pointer       │
    │  // Rust owns the memory       │
```

**Rules:**
- All cross-language calls go through `extern "C"` ABI
- No C++ name mangling crosses the boundary
- No exceptions cross the boundary
- Memory allocated by Rust is freed by Rust
- All pointer lifetimes are documented in the API

### 10.2. C++ ↔ TypeScript Boundary

The TypeScript UI runs in an embedded JavaScript runtime (V8, QuickJS, or a custom lightweight runtime). Communication is via a C ABI bridge:

```
TypeScript                         C++
    │                                │
    │  // Call native function       │
    │  const result =                │
    │    bridge.call('nativeFunc',   │
    │      {param: 42});             │
    │                                │
    │──────► serialize args ────────►│
    │       (flatbuffers / custom)   │
    │                                │
    │◄────── deserialize result ─────│
    │       (flatbuffers / custom)   │
```

### 10.3. C++ ↔ Lua Boundary

Lua is embedded directly via `lua.h`. The Lua API is a set of registered C functions:

```c
// C++ registers Lua-callable functions
lua_register(L, "aria_get_track_count", lua_get_track_count);
lua_register(L, "aria_create_midi_clip", lua_create_midi_clip);
lua_register(L, "aria_add_note", lua_add_note);

// Lua script calls them
-- user_script.lua
local tracks = aria_get_track_count()
local clip = aria_create_midi_clip(1, 4)
aria_add_note(clip, 60, 0, 1.0, 100)  -- C5, beat 0, 1 bar, vel 100
```

### 10.4. C++ ↔ Python Boundary

Python runs as a subprocess. Communication is text-based (JSON/msgpack):

```
C++                                     Python Subprocess
    │                                        │
    │  Start subprocess with stdin/stdout    │
    │──────────────────────────────────────► │
    │                                        │
    │  { "method": "semantic_search",        │
    │    "params": { "query": "..." } }      │
    │──────────────────────────────────────► │
    │                                        │
    │  { "result": [...] }                   │
    │◄───────────────────────────────────────│
```

---

## 11. Plugin Architecture

### 11.1. Plugin Host Design

```
┌──────────────────────────────────────────────────────────────────┐
│                         Plugin Host                               │
│                                                                   │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐ │
│  │  CLAP Host │  │  VST3 Host │  │  AU Host   │  │  LV2 Host  │ │
│  └──────┬─────┘  └──────┬─────┘  └──────┬─────┘  └──────┬─────┘ │
│         │               │               │               │        │
│         └───────────────┼───────────────┼───────────────┘        │
│                         │               │                        │
│                    ┌────▼───────────────▼────┐                   │
│                    │   Plugin Abstraction    │                   │
│                    │   (Unified Plugin API)  │                   │
│                    └────────────┬────────────┘                   │
│                                 │                                │
│                    ┌────────────▼────────────┐                   │
│                    │    Native Plugin API    │                   │
│                    │   (CLAP-native path)    │                   │
│                    └────────────┬────────────┘                   │
│                                 │                                │
│                    ┌────────────▼────────────┐                   │
│                    │   Plugin Graph Node     │                   │
│                    └────────────┬────────────┘                   │
│                                 │                                │
│                    ┌────────────▼────────────┐                   │
│                    │   Audio Graph Node      │                   │
│                    └─────────────────────────┘                   │
└──────────────────────────────────────────────────────────────────┘
```

### 11.2. Native CLAP Plugin Pipeline

ARIA's built-in plugins are CLAP plugins that load through the same pipeline as third-party CLAP plugins. There is no "fast path" or internal API.

```
CLAP Plugin Factory (C++)
    │
    ├──► Register plugin descriptors (CLAP extension)
    │
    ├──► When instantiated:
    │       ├── Standard clap_plugin.create()
    │       ├── Standard clap_plugin.init()
    │       ├── Standard clap_plugin.activate()
    │       └── Standard clap_plugin.process()
    │
    └──► When destroyed:
            ├── Standard clap_plugin.deactivate()
            └── Standard clap_plugin.destroy()
```

This means:
- Built-in plugins can be loaded in any other CLAP-compatible DAW
- There is no performance penalty for internal plugins (zero-copy buffer sharing through clap_audio_buffer_t)
- Plugin crashes are handled by the same sandboxing mechanism
- Plugin development and third-party development use identical APIs and tools

### 11.3. Plugin Sandboxing

| Mode | Isolation | Latency Impact | Default |
|---|---|---|---|
| **In-process** | None (process crash = host crash) | None | Trusted plugins (native, whitelisted) |
| **Out-of-process** | Full (plugin crash ≠ host crash) | +50-200μs per buffer | Untrusted plugins |
| **Sandboxed thread** | Partial (crash isolated to thread) | +10-50μs | Semi-trusted plugins |

---

## 12. UI Architecture

### 12.1. Render Pipeline

```
TypeScript Component Tree
    │
    ▼
Layout Engine (Flex/Grid)
    │
    ▼
Paint Tree (GPU Commands)
    │
    ▼
Skia Canvas (2D rendering)
    │
    ▼
WebGPU (Vulkan / Metal / DirectX 12)
    │
    ▼
GPU
```

### 12.2. Frame Budget

| Phase | Budget (at 144 FPS) | Budget (at 60 FPS) |
|---|---|---|
| TypeScript event handling | 1ms | 4ms |
| Layout calculation | 1ms | 4ms |
| Paint tree generation | 2ms | 5ms |
| Skia rendering | 2ms | 4ms |
| WebGPU submission | 1ms | 2ms |
| **Total budget** | **~7ms** | **~16ms** |

### 12.3. UI Architecture Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                   UI Framework (TypeScript)                    │
│                                                               │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────────────┐ │
│  │ Components   │  │   Views     │  │      Layout           │ │
│  │ (Widget Tree)│  │ (Panels)    │  │ (Dock, Split, Tab)   │ │
│  └──────┬───────┘  └──────┬──────┘  └──────────┬───────────┘ │
│         │                 │                     │              │
│         └─────────────────┼─────────────────────┘              │
│                           │                                    │
│                    ┌──────▼──────┐                             │
│                    │ Theme Engine │                             │
│                    └──────┬──────┘                             │
│                           │                                    │
│                    ┌──────▼──────┐                             │
│                    │ Animation   │                             │
│                    │ Engine      │                             │
│                    └──────┬──────┘                             │
│                           │                                    │
│                    ┌──────▼──────┐                             │
│                    │  Bridge FFI │                             │
│                    └──────┬──────┘                             │
└───────────────────────────┼───────────────────────────────────┘
                            │ (C ABI)
                    ┌───────▼────────┐
                    │ Graphics Engine │
                    │  (C++ + Skia + │
                    │    WebGPU)      │
                    └────────────────┘
```

### 12.4. Component Model

Components in ARIA follow a unidirectional data flow:

```
State (C++) ──► FFI Bridge ──► UI State Store ──► Component ──► GPU
                    ▲                                      │
                    │                                      │
                    └──── User Input (events/actions) ─────┘
```

---

## 13. Audio Pipeline

### 13.1. Signal Chain

```
Audio Input         MIDI Input
    │                   │
    ▼                   ▼
Input Node ───┐   MIDI Router ──► Instrument Track ──► Audio
              │        │                                    │
              │   ┌────┴────┐                               │
              │   │ MIDI FX │                               │
              │   └─────────┘                               │
              │                                             ▼
              ├──► Audio Track ──► Track FX ──► Mixer Input
              │                        │             │
              │                   ┌────┴────┐  ┌─────┴──────┐
              │                   │Sidechain│  │ Send/Return │
              │                   └─────────┘  └─────┬──────┘
              │                                      │
              └──────────────────────────────────────┘
                                                     │
                                              ┌──────▼──────┐
                                              │   Mixer     │
                                              │  Summing    │
                                              └──────┬──────┘
                                                     │
                                              ┌──────▼──────┐
                                              │   Master    │
                                              │  Channel    │
                                              └──────┬──────┘
                                                     │
                                              ┌──────▼──────┐
                                              │   Output    │
                                              │    Node     │
                                              └─────────────┘
```

### 13.2. Audio Graph Processing Order

1. **Input nodes** process first (audio interfaces, resampling)
2. **Instrument tracks** receive MIDI → produce audio
3. **Audio tracks** receive clip data → apply clip gain/fades → output audio
4. **Track FX** process in order (pre-fader, post-fader)
5. **Sends** are tapped post-fader or pre-fader
6. **Mixer inputs** are summed into their assigned buses
7. **Returns** process and sum into the mix
8. **Master channel** applies final processing
9. **Output node** delivers to hardware

### 13.3. Latency Management

| Component | Typical Latency |
|---|---|
| Audio buffer | 1.5ms - 10ms (depends on buffer size) |
| Track FX | 0 - 10ms (plugin-specific) |
| Plugin PDC | Automatic (plugin reports latency) |
| Master bus | 0ms |
| Output | 0.5ms (driver-specific) |
| **Total** | **2ms - 20ms** |

---

## 14. Project File Architecture

### 14.1. File Format Overview

The ARIA project format is a custom binary format designed for:
- Fast loading (memory-mappable)
- Backwards compatibility
- Crash recovery
- Non-destructive editing

```
┌──────────────────────────────────────────────┐
│              ARIA Project File                │
│                                                │
│  ┌──────────────────────────────────────────┐ │
│  │            File Header (64 bytes)         │ │
│  │  - Magic: "ARIA\0"                       │ │
│  │  - Version: major.minor.patch             │ │
│  │  - Checksum: CRC32                        │ │
│  │  - Created: timestamp                     │ │
│  │  - Modified: timestamp                    │ │
│  └──────────────────────────────────────────┘ │
│                                                │
│  ┌──────────────────────────────────────────┐ │
│  │            Block Table                     │ │
│  │  - Block count                            │ │
│  │  - Block[index]: type, offset, size, CRC  │ │
│  └──────────────────────────────────────────┘ │
│                                                │
│  ┌──────────────────────────────────────────┐ │
│  │            Block 1: Project Metadata       │ │
│  └──────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────┐ │
│  │            Block 2: Track List             │ │
│  └──────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────┐ │
│  │            Block 3: Mixer State            │ │
│  └──────────────────────────────────────────┘ │
│  ...                                          │
│                                                │
│  ┌──────────────────────────────────────────┐ │
│  │            Footer Block                    │ │
│  │  - CRC32 of entire file (excluding header)│ │
│  └──────────────────────────────────────────┘ │
└──────────────────────────────────────────────┘
```

### 14.2. Version Compatibility

| Strategy | Mechanism |
|---|---|
| Forward compatibility | New blocks are ignored by older versions |
| Backward compatibility | Old blocks must be readable by new versions |
| Migration | Project has a migration path from v(N) to v(N+1) |
| Default values | New fields default to safe values in older files |

---

## 15. Error Handling & Recovery

### 15.1. Error Categories

| Category | Handling | Example |
|---|---|---|
| **Recoverable** | Log + continue | Audio buffer underrun |
| **Degradable** | Fallback + notify | Plugin fails to load |
| **Fatal** | Save state + exit | Out of memory |
| **Crash** | Auto-save + restart | Access violation |

### 15.2. Crash Recovery Flow

```
Application starts
    │
    ├──► Check for crash flag from previous run
    │
    ├──► If crash detected:
    │       ├── Load last auto-save
    │       ├── Restore window state
    │       ├── Show "Recovery" banner
    │       └── Generate crash report file
    │
    └──► Normal startup
```

### 15.3. Auto-Save Strategy

| Trigger | Action |
|---|---|
| Every 60 seconds | Save project to `.aria.auto` |
| On project switch | Save current project |
| Before plugin load | Save checkpoint |
| On idle (5s no input) | Save if dirty |
| On crash | Save emergency file to temp |

---

## 16. Security Architecture

### 16.1. Threat Model

| Threat | Mitigation |
|---|---|
| Malicious VST3/CLAP plugin | Out-of-process sandboxing |
| Malicious Lua script | Sandboxed Lua environment (limited API) |
| Malicious project file | Block CRC verification, sanitized deserialization |
| Network attack | TLS, signed updates, no open ports |
| Data exfiltration | No telemetry without consent, file access scoped |

### 16.2. Plugin Security

```
Plugin loading sequence:
    │
    ├──► Validate plugin binary signature (if signed)
    ├──► Scan with antivirus heuristic
    ├──► Decide sandbox mode:
    │       ├── Native CLAP plugin → in-process (trusted)
    │       ├── Signed third-party plugin → sandboxed thread
    │       └── Unsigned third-party → out-of-process
    ├──► Load plugin in selected sandbox
    └──► Monitor for crash / timeout
```

---

## 17. Testing Strategy

### 17.1. Test Pyramid

```
         ╱╲
        ╱  ╲         UI Tests (TypeScript)
       ╱    ╲        (Puppeteer-like, GPU screenshot)
      ╱──────╲
     ╱        ╲      Integration Tests (C++/Rust)
    ╱          ╲     (Module boundary tests, project load/save)
   ╱────────────╲
  ╱              ╲   Unit Tests (C++/Rust)
 ╱                ╲  (Individual module tests, audio buffer tests)
╱──────────────────╲
```

### 17.2. Module Testability

| Module | Isolation | Mock Strategy |
|---|---|---|
| Audio Engine | Audio graph testable without audio device | Mock `AudioDriver` interface |
| MIDI Engine | MIDI graph testable without MIDI hardware | Mock `MidiDevice` interface |
| Plugin Host | Plugin loading testable with test plugins | Minimal CLAP/VST3 test plugins |
| Project Engine | Serialization roundtrip tests | In-memory file system |
| Automation | Curve math is pure functions | No mocking needed |
| DSP | Pure signal processing | No mocking needed |

---

## 18. Deployment & Distribution

### 18.1. Packaging

| Platform | Package Format | Installer |
|---|---|---|
| Windows | `.exe` + `.dll` | MSI / NSIS installer |
| macOS | `.app` bundle | DMG + notarized |
| Linux | AppImage / Flatpak | AppImage / Flatpak |

### 18.2. Update Strategy

- **Semi-automatic**: Check for updates on startup (opt-in)
- **Delta updates**: Binary diff for faster downloads
- **Rollback**: Previous version preserved on update
- **Channel**: Stable / Beta / Nightly

---

## 19. Glossary

| Term | Definition |
|---|---|
| **Audio Graph** | Directed acyclic graph of audio processing nodes |
| **Audio Thread** | Real-time thread that processes audio buffers |
| **CLAP** | CLever Audio Plugin — open plugin format, native to ARIA |
| **Command Queue** | Thread-safe queue for dispatching mutating operations |
| **Event Bus** | Publish/subscribe system for inter-module communication |
| **FFI** | Foreign Function Interface — C ABI cross-language calls |
| **Lock-Free** | Concurrency mechanism where progress is guaranteed without locks |
| **PDC** | Plugin Delay Compensation — automatic latency correction |
| **Sandboxing** | Isolating plugins to prevent crashes from affecting the host |
| **SIMD** | Single Instruction Multiple Data — parallel CPU processing |
| **Skia** | Open-source 2D graphics library (Chrome, Android) |
| **WebGPU** | Cross-platform GPU API abstraction (Vulkan/Metal/DirectX) |
