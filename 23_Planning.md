# ARIA DAW — Development Roadmap

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27

---

## Table of Contents

1. [Overview](#1-overview)
2. [Phase 0: Foundation](#2-phase-0-foundation)
3. [Phase 1: Audio Engine](#3-phase-1-audio-engine)
4. [Phase 2: MIDI Engine](#4-phase-2-midi-engine)
5. [Phase 3: Timeline & Transport](#5-phase-3-timeline--transport)
6. [Phase 4: Piano Roll](#6-phase-4-piano-roll)
7. [Phase 5: Mixer](#7-phase-5-mixer)
8. [Phase 6: Plugin Host](#8-phase-6-plugin-host)
9. [Phase 7: Automation Engine](#9-phase-7-automation-engine)
10. [Phase 8: Arrangement & Session Views](#10-phase-8-arrangement--session-views)
11. [Phase 9: Browser & Database](#11-phase-9-browser--database)
12. [Phase 10: GPU UI Framework](#12-phase-10-gpu-ui-framework)
13. [Phase 11: Scripting API](#13-phase-11-scripting-api)
14. [Phase 12: AI Integration](#14-phase-12-ai-integration)
15. [Phase 13: Collaboration & Sync](#15-phase-13-collaboration--sync)
16. [Phase 14: Polish & Beta](#16-phase-14-polish--beta)
17. [Post-1.0 Roadmap](#17-post-10-roadmap)
18. [RFC Index](#18-rfc-index)

---

## 1. Overview

### 1.1. Development Philosophy

ARIA DAW is being built in **14 phases**, each producing a working, testable milestone. No phase depends on future phases — each one is complete and functional before moving on.

### 1.2. Timeline Estimate

| Phase | Duration | Effort | Team Size |
|---|---|---|---|
| P0: Foundation | 3-6 months | 800h | 2-3 devs |
| P1: Audio Engine | 6-9 months | 1500h | 3-4 devs |
| P2: MIDI Engine | 3-4 months | 600h | 2 devs |
| P3: Timeline | 3-4 months | 600h | 2 devs |
| P4: Piano Roll | 5-8 months | 1200h | 3 devs |
| P5: Mixer | 3-4 months | 600h | 2 devs |
| P6: Plugin Host | 5-7 months | 1000h | 3 devs |
| P7: Automation | 3-4 months | 600h | 2 devs |
| P8: Views | 4-6 months | 800h | 2-3 devs |
| P9: Browser | 2-3 months | 400h | 1-2 devs |
| P10: GPU UI | 6-9 months | 1500h | 3-4 devs |
| P11: Scripting | 3-4 months | 500h | 2 devs |
| P12: AI | 4-6 months | 800h | 2-3 devs |
| P13: Collab | 3-4 months | 500h | 2 devs |
| P14: Polish | 4-6 months | 1000h | Full team |
| **Total** | **4-6 years** | **~12,000h** | |

---

## 2. Phase 0: Foundation

**Duration:** 3-6 months
**Goal:** Build infrastructure, build system, and core scaffolding

### Deliverables

```
✅ Documentation (completed)
   - 23 specification documents
   - Full architecture documentation
   - API specification with 164 functions
   - 300+ planned RFCs

🔲 [ ] Repository setup
    - Git repository with branching strategy
    - CMake + Cargo + pnpm build system
    - CI/CD pipeline (GitHub Actions)
    - Code formatting, linting, static analysis

🔲 [ ] Project structure
    - Directory layout per architecture spec
    - Module stubs for all engines
    - Cross-language FFI skeleton (C++ ↔ Rust)
    - TypeScript UI scaffolding

🔲 [ ] Developer environment
    - Setup script (install dependencies)
    - Docker dev environment
    - VSCode/Cursor config (tasks, launch, intellisense)
    - Debug build configuration

🔲 [ ] Core kernel
    - Application lifecycle (init/run/shutdown)
    - Event bus (typed, thread-safe)
    - Service locator
    - Crash handler and diagnostics

🔲 [ ] Testing infrastructure
    - Catch2 test framework
    - Audio test harness
    - Test plugin suite
    - CI integration
```

**RFCs to create:** RFC-BLD-001 through RFC-BLD-016

---

## 3. Phase 1: Audio Engine

**Duration:** 6-9 months
**Goal:** Working audio playback and recording with low latency

### Deliverables

```
🔲 [ ] Audio device abstraction
    - ASIO driver (Windows)
    - WASAPI driver (Windows)
    - CoreAudio driver (macOS)
    - ALSA/PipeWire driver (Linux)
    - Device enumeration and selection

🔲 [ ] Audio buffer management
    - Lock-free buffer pool
    - AudioBlock (multi-channel, planar)
    - SIMD buffer operations (SSE2, AVX2, NEON)

🔲 [ ] Audio graph
    - DAG processing (topological sort)
    - Node types (input, output, gain, meter)
    - Lock-free parameter updates
    - Graph rebuild (atomic swap)

🔲 [ ] Transport
    - Play/stop/record state machine
    - Sample-accurate clock
    - Tempo map and time signature
    - Loop and punch ranges

🔲 [ ] DSP core
    - Biquad filter (all types)
    - State variable filter
    - Gain, pan, phase utilities
    - Metering (peak, RMS)

🔲 [ ] Export
    - Offline renderer (same graph, no real-time)
    - WAV, AIFF, FLAC support
    - Dithering (TPDF, noise-shaped)
```

**RFCs to create:** RFC-AE-001 through RFC-AE-020

---

## 4. Phase 2: MIDI Engine

**Duration:** 3-4 months
**Goal:** MIDI recording, playback, and device management

### Deliverables

```
🔲 [ ] MIDI device abstraction
    - Windows MIDI (WinMM, virtual ports)
    - CoreMIDI (macOS)
    - ALSA sequencer (Linux)
    - USB MIDI class driver
    - Virtual MIDI ports

🔲 [ ] MIDI data model
    - MidiEvent (all message types)
    - NoteTracker (active notes, sustain)
    - MidiClip (notes, CC, timing)

🔲 [ ] MIDI graph
    - MIDI processing nodes
    - MIDI thru/router
    - MIDI transform nodes

🔲 [ ] MIDI clock
    - Master/slave modes
    - 24 PPQN clock generation
    - MTC (future)

🔲 [ ] Recording
    - Timestamped event capture
    - Jitter compensation
    - MIDI quantize on record

🔲 [ ] MPE support
    - MPE decoder (zone → per-note)
    - MPE encoder (per-note → zone)
    - Piano roll MPE display
```

**RFCs to create:** RFC-ME-001 through RFC-ME-015

---

## 5. Phase 3: Timeline & Transport

**Duration:** 3-4 months
**Goal:** Working timeline with loops, markers, tempo map

### Deliverables

```
🔲 [ ] Timeline
    - PPQN timeline (960 PPQN)
    - Bar/beat/sixteenth display
    - Tempo track (multiple tempo events)
    - Time signature track
    - Loop points

🔲 [ ] Transport controls
    - Play, stop, record, pause, toggle
    - Position display and seeking
    - Playhead rendering
    - Metronome

🔲 [ ] Markers
    - Add/remove/move markers
    - Marker navigation
    - Marker colors and labels

🔲 [ ] Project model
    - Track container (all track types)
    - Clip container (audio, MIDI, automation)
    - Undo/redo stack
    - Project serialization stubs
```

**RFCs to create:** RFC-TR-001 through RFC-TR-019

---

## 6. Phase 4: Piano Roll

**Duration:** 5-8 months
**Goal:** The best piano roll in any DAW — vector-based MIDI editor

### Deliverables

```
🔲 [ ] Note data model
    - Note object (pitch, start, duration, velocity, color)
    - NoteCollection with spatial index
    - NoteID system (persistent, unique)

🔲 [ ] GPU rendering
    - Note instanced rendering (1 draw call)
    - Grid lines (bar, beat, sixteenth)
    - Keyboard display (textured from atlas)
    - Viewport culling (spatial grid)

🔲 [ ] Tools
    - Pencil (create, move, resize)
    - Select (click, marquee, shift-add)
    - Paint (continuous creation)
    - Erase (click, stroke)
    - Cut (split notes)
    - Glue (merge notes)
    - Ramp (velocity gradient)

🔲 [ ] Editing
    - Move, resize, transpose
    - Selection (single, multiple, range)
    - Copy/paste/duplicate
    - Undo/redo

🔲 [ ] Velocity editing
    - Velocity bars
    - Ramp tool
    - Randomize, compress, expand

🔲 [ ] Grid & snap
    - Snap to grid (all levels)
    - Snap to scale
    - Adaptive snap (zoom-based)

🔲 [ ] Expression lanes
    - Pitch bend lane
    - CC lanes (any CC)
    - Channel pressure
    - Per-note MPE expressions

🔲 [ ] Ghost notes
    - Adjacent clip ghosting
    - Reference track ghosting
    - Scale degree ghosting

🔲 [ ] Scale & chord tools
    - Scale highlighting (14 scales)
    - Snap-to-scale
    - Chord generator (20+ types)
    - Arpeggiator (9 patterns)

🔲 [ ] Quantize & humanize
    - Grid quantize (strength, swing)
    - Humanize (timing, velocity)
    - Groove templates (swing, latin, funk, hip-hop)

🔲 [ ] MIDI step input
    - Real-time step recording
    - Step sequencer mode
    - Replace mode

🔲 [ ] Multi-monitor
    - Independent window mode
    - Floating panel mode
    - Layout presets
```

**RFCs to create:** RFC-PR-001 through RFC-PR-024

---

## 7. Phase 5: Mixer

**Duration:** 3-4 months
**Goal:** Working mixer with faders, metering, routing

### Deliverables

```
🔲 [ ] Channel strip
    - Volume fader (-∞ to +24 dB)
    - Pan (equal-power, 4 laws)
    - Mute, solo, phase invert
    - Width/stereo balance

🔲 [ ] Summing
    - 64-bit float summing
    - Group buses
    - VCA faders

🔲 [ ] Routing
    - Track → bus → master
    - Sends (pre/post fader, pre/post FX)
    - Returns (FX buses)
    - Sidechain routing

🔲 [ ] Metering
    - Peak (sample-level, with hold)
    - RMS (300ms window)
    - LUFS (momentary, short-term, integrated)
    - Phase correlation
    - Spectrum analyzer (FFT-based)

🔲 [ ] Built-in EQ
    - 8-band parametric EQ
    - Zero-latency (biquad cascade)
    - Low/high cut filters

🔲 [ ] FX rack
    - Plugin chain with drag reorder
    - Per-plugin bypass, wet/dry mix
    - Plugin delay compensation

🔲 [ ] Master channel
    - Master fader
    - Master FX chain
    - Brickwall limiter
    - Dithering
    - Hardware output configuration

🔲 [ ] Mixer UI
    - Console layout (horizontal strips)
    - Track-integrated layout
    - Floating window
    - Channel width presets
```

**RFCs to create:** RFC-MX-001 through RFC-MX-018

---

## 8. Phase 6: Plugin Host

**Duration:** 5-7 months
**Goal:** Load and process third-party plugins (CLAP, VST3, AU)

### Deliverables

```
🔲 [ ] CLAP host
    - Full CLAP spec support
    - All extensions (audio, params, state, GUI)
    - Native CLAP plugin factory for built-ins

🔲 [ ] VST3 host
    - Component + controller separation
    - Parameter management
    - Bus configuration
    - Program/list management

🔲 [ ] AU host (macOS)
    - Audio Unit v2/v3 support
    - Parameter mapping
    - Cocoa view embedding

🔲 [ ] LV2 host (Linux)
    - Lilv-based implementation
    - Port discovery
    - Worker/schedule support

🔲 [ ] Plugin sandboxing
    - Level 0: In-process (trusted)
    - Level 1: Sandboxed thread
    - Level 2: Out-of-process (IPC)
    - Level 3: Out-of-process + restricted

🔲 [ ] Plugin scanning
    - Incremental scan (file modification time)
    - Full rescan on demand
    - Blacklist (auto + manual)
    - Plugin cache (JSON)

🔲 [ ] Presets
    - CLAP preset format (.clap-preset)
    - VST3 preset (fxp/fxb) support
    - Preset browser and search
    - Factory and user presets

🔲 [ ] Plugin GUI
    - Embedded editor in ARIA window
    - Floating editor window
    - HiDPI scaling
    - Editor state persistence
```

**RFCs to create:** RFC-PH-001 through RFC-PH-019

---

## 9. Phase 7: Automation Engine

**Duration:** 3-4 months
**Goal:** Complete automation and modulation system

### Deliverables

```
🔲 [ ] Automation clips
    - Point-based automation (10 interpolation types)
    - Bezier curve editing
    - Step automation clips
    - LFO automation clips

🔲 [ ] Envelope system
    - ADSR, AHDSR, DADSR
    - Gated envelope evaluation
    - Envelope curve shapes

🔲 [ ] Modulation sources
    - LFO (10 waveforms, key-sync, tempo-sync)
    - Envelope follower (sidechain)
    - Step sequencer
    - Macro knobs
    - Random/S&H
    - Note property (velocity, pitch)

🔲 [ ] Modulation matrix
    - Source → Target routing
    - Amount per connection
    - Bipolar/unipolar
    - Nested modulation (8 levels deep)

🔲 [ ] Automation lanes
    - Lane per parameter
    - Arm/disarm for recording
    - Lane visibility toggling
    - Lane height customization

🔲 [ ] Automation recording
    - Real-time capture (with smoothing)
    - Douglas-Peucker point reduction
    - Post-recording editing

🔲 [ ] Real-time parameter cache
    - Lock-free double-buffered
    - Sample-accurate automation
    - Audio thread reads (atomic)
```

**RFCs to create:** RFC-AU-001 through RFC-AU-020

---

## 10. Phase 8: Arrangement & Session Views

**Duration:** 4-6 months
**Goal:** Complete project arrangement with dual-view workflow

### Deliverables

```
🔲 [ ] Arrangement view
    - Linear timeline with tracks
    - Clip rendering (audio waveforms, MIDI)
    - Track headers (name, color, controls)
    - Drag-and-drop clip placement
    - Clip fades and crossfades
    - Time stretcher/pitch shifter per clip

🔲 [ ] Session view
    - Clip slot grid (tracks × scenes)
    - Scene launch (trigger, stop)
    - Clip launch modes (trigger, toggle, gate)
    - Follow actions
    - Crossfader

🔲 [ ] Dual-view sync
    - Session clips → arrangement recording
    - Arrangement → session clip extraction
    - Transport sync between views

🔲 [ ] Track types
    - Audio track (with clips)
    - MIDI track (with instrument)
    - Group track (audio summing)
    - VCA track (fader control)
    - Return track (FX bus)
    - Master track

🔲 [ ] Freeze & bounce
    - Track freeze (render FX → audio)
    - Track flatten (permanent freeze)
    - Stem bounce (selected tracks)
    - Master bounce

🔲 [ ] Track groups & VCAs
    - Group folder (fold/unfold)
    - Group summing
    - VCA fader (proportional slave control)
```

**RFCs to create:** RFC-TR-011, RFC-TR-014, RFC-TR-016, RFC-TR-018, RFC-TR-019

---

## 11. Phase 9: Browser & Database

**Duration:** 2-3 months
**Goal:** Sample indexing, search, and file browser

### Deliverables

```
🔲 [ ] SQLite database
    - Full schema (samples, plugins, projects, presets, tags)
    - FTS5 full-text search
    - Migration system
    - WAL mode performance

🔲 [ ] File scanner/indexer
    - Recursive folder scanning
    - File type detection
    - Metadata extraction (duration, sample rate)
    - BPM/key/loudness analysis
    - Category classification

🔲 [ ] File watcher
    - Real-time folder monitoring
    - Platform-specific (inotify, FSEvents, ReadDirectoryChanges)
    - Debounced updates

🔲 [ ] Search engine
    - Full-text search (FTS5)
    - Filtered search (category, BPM, key, tags)
    - Semantic search (via AI)
    - Sort by relevance, name, date, BPM

🔲 [ ] File browser UI
    - File tree (by folder)
    - Category tree (by type)
    - Search results
    - Preview panel (waveform, metadata)
    - Audio preview player
    - Drag-and-drop to tracks

🔲 [ ] Waveform cache
    - Peak/minima generation
    - Disk cache (per-file)
    - Memory cache (LRU)
```

**RFCs to create:** RFC-DB-001 through RFC-DB-015, RFC-FS-001 through RFC-FS-018

---

## 12. Phase 10: GPU UI Framework

**Duration:** 6-9 months
**Goal:** Complete GPU-rendered user interface

### Deliverables

```
🔲 [ ] WebGPU backend
    - Device initialization
    - Swap chain management
    - Buffer, texture, shader management
    - Multi-window support

🔲 [ ] Skia integration
    - Canvas abstraction (2D drawing)
    - Text rendering (glyph atlas)
    - SVG icon rendering
    - Shadow/blur post-processing

🔲 [ ] Component system
    - Virtual component tree
    - Diff/patch cycle (VDOM)
    - State management (reactive)
    - Component lifecycle

🔲 [ ] Widget library (GPU-rendered)
    - Button, ToggleButton, IconButton
    - TextInput, Dropdown, SearchField
    - Slider (H/V), Knob, RotaryEncoder
    - MeterBar, WaveformDisplay
    - ScrollView, TabPanel, Accordion
    - Dialog, Modal, ContextMenu, Tooltip

🔲 [ ] DAW-specific widgets
    - ChannelStrip (mixer channel)
    - Fader (touch-sensitive)
    - MeterBar (peak/RMS)
    - Timeline ruler
    - PianoRoll canvas
    - Automation curve canvas

🔲 [ ] Layout engine
    - Flexbox layout (GPU-computed)
    - Docking system (split, tab, float)
    - Multi-window management
    - HiDPI support

🔲 [ ] Animation engine
    - GPU-side interpolation
    - Easing curves (30+ types)
    - Animation composer
    - Transition system

🔲 [ ] Theme system
    - JSON theme file format
    - Complete color palette
    - Typography scale
    - Component style overrides
    - Live reload
    - Built-in themes (dark, light, high-contrast)

🔲 [ ] Input system
    - Mouse, keyboard, touch, pen
    - Keyboard shortcut manager
    - Focus management
    - Accessibility (tab navigation, screen reader)
```

**RFCs to create:** RFC-UI-001 through RFC-UI-020, RFC-GFX-001 through RFC-GFX-020, RFC-REN-001 through RFC-REN-018, RFC-TH-001 through RFC-TH-016

---

## 13. Phase 11: Scripting API

**Duration:** 3-4 months
**Goal:** Lua scripting and macro system

### Deliverables

```
🔲 [ ] Lua runtime
    - Lua 5.4 VM management
    - Multiple script instances
    - Memory and time limits
    - Sandboxed environment

🔲 [ ] Public API
    - 164 C API functions across 13 modules
    - Lua bindings (via C API)
    - Python bindings (future)
    - API documentation

🔲 [ ] Script types
    - MIDI effect script
    - MIDI generator script
    - Audio effect script
    - Audio generator script
    - UI script (custom panels)
    - Automation script
    - Macro script

🔲 [ ] Script manager
    - Install/uninstall scripts
    - Enable/disable
    - Parameter configuration
    - Hot reload

🔲 [ ] Macro system
    - Action recording
    - Macro → Lua code generation
    - Playback
    
🔲 [ ] Script security
    - Sandbox restrictions (no io, os, load)
    - Memory limit (50MB)
    - Instruction limit (10k/frame)
    - Timeout protection
```

**RFCs to create:** RFC-SC-001 through RFC-SC-018, RFC-API-001 through RFC-API-020

---

## 14. Phase 12: AI Integration

**Duration:** 4-6 months
**Goal:** AI-powered workflow features

### Deliverables

```
🔲 [ ] AI infrastructure
    - Python subprocess bridge
    - ONNX Runtime integration
    - Model management (download, update)
    - GPU acceleration for inference
    - C++ → Python JSON protocol

🔲 [ ] Semantic sample search
    - CLAP-based audio embedding model
    - Natural language search ("dark kick")
    - Similarity search (find similar sounds)
    - Real-time embedding during indexing

🔲 [ ] Stem separation
    - Demucs-based model integration
    - Voice, drums, bass, other separation
    - GPU-accelerated processing
    - Progress reporting

🔲 [ ] Chord & progression tools
    - Key detection (Krumhansl-Schmuckler)
    - Chord recognition (from audio or MIDI)
    - Chord progression suggestion
    - Melody harmonization

🔲 [ ] Mix assistant
    - Genre-based mix presets
    - Level balance suggestion
    - EQ curve suggestion
    - Dynamic range analysis

🔲 [ ] MIDI generation
    - Drum pattern generation
    - Bass line generation
    - Chord progression generation
    - Style-conditional generation
```

**RFCs to create:** RFC-AI-001 through RFC-AI-010

---

## 15. Phase 13: Collaboration & Sync

**Duration:** 3-4 months
**Goal:** Project sharing and real-time collaboration

### Deliverables

```
🔲 [ ] Project sync
    - Project snapshot/restore
    - Incremental sync (delta-based)
    - Conflict resolution
    - Cloud storage integration (optional)

🔲 [ ] Collaboration
    - Shared project model (edit + merge)
    - Real-time cursor presence
    - Chat/comments within project
    - Permission management

🔲 [ ] ARIA Link
    - Network protocol (WebRTC-based)
    - Audio streaming (low-latency)
    - MIDI sharing
    - Session sharing
```

**RFCs to create:** RFC-CL-001 through RFC-CL-008

---

## 16. Phase 14: Polish & Beta

**Duration:** 4-6 months
**Goal:** Production-ready release

### Deliverables

```
🔲 [ ] Performance optimization
    - CPU profiling and optimization
    - GPU profiling and optimization
    - Memory usage optimization
    - Startup time optimization
    - Large project stress testing

🔲 [ ] Stability
    - 24-hour stability test
    - Plugin compatibility matrix (500+ plugins)
    - Crash reporting system
    - Memory leak detection
    - Thread sanitizer testing

🔲 [ ] User testing
    - Internal alpha (20 users)
    - Closed beta (200 users)
    - Open beta (2000 users)
    - Feedback collection system

🔲 [ ] Documentation
    - User manual
    - API reference
    - Scripting guide
    - Plugin developer guide
    - Video tutorials

🔲 [ ] Packaging
    - Windows installer (NSIS)
    - macOS app bundle (notarized)
    - Linux AppImage
    - Auto-update system
    - License management

🔲 [ ] Release
    - Version 1.0.0
    - Release notes
    - Marketing website
    - Community forums
```

---

## 17. Post-1.0 Roadmap

### 17.1. v1.1 - v1.9 (Feature Additions)

| Version | Focus |
|---|---|
| v1.1 | Workflow improvements (keyboard shortcuts, custom layouts) |
| v1.2 | MIDI 2.0 support |
| v1.3 | Advanced time-stretching (Elastique-like) |
| v1.4 | Video import and scoring |
| v1.5 | ARA 2 integration (Melodyne-style) |
| v1.6 | CLAP 1.2+ new features |
| v1.7 | External hardware integration (MIDI CC learn enhancements) |
| v1.8 | Advanced routing (multichannel, surround, Atmos) |
| v1.9 | Plugin sandboxing v2 (performance improvements) |

### 17.2. v2.0 (Major)

| Feature | Description |
|---|---|
| **New DSP engine** | Full modular DSP rack (Reaktor-style) |
| **Notation editor** | Score view with engraving |
| **Advanced AI** | Stem separation v2, mastering assistant |
| **Collaboration v2** | Real-time multi-user editing |
| **Plugin SDK** | C++ API for third-party plugins |
| **Asset store** | Presets, scripts, themes marketplace |

### 17.3. v3.0 (Future)

| Feature | Description |
|---|---|
| **Cloud DAW** | Browser-based ARIA Lite |
| **Mobile companion** | iOS/Android remote control |
| **Hardware integration** | Control surface SDK |
| **Game audio** | FMOD/Wwise integration |
| **Modular scripting** | Visual scripting (Max-like) |

---

## 18. RFC Index

| RFC | Phase | Component | Status |
|---|---|---|---|
| RFC-BLD-001-016 | P0 | Foundation/Build | 🔲 |
| RFC-AE-001-020 | P1 | Audio Engine | 🔲 |
| RFC-ME-001-015 | P2 | MIDI Engine | 🔲 |
| RFC-TR-001-019 | P3, P8 | Track/Timeline/Views | 🔲 |
| RFC-PR-001-024 | P4 | Piano Roll | 🔲 |
| RFC-MX-001-018 | P5 | Mixer | 🔲 |
| RFC-PH-001-019 | P6 | Plugin Host | 🔲 |
| RFC-AU-001-020 | P7 | Automation | 🔲 |
| RFC-FS-001-018 | P9 | File System | 🔲 |
| RFC-DB-001-015 | P9 | Database | 🔲 |
| RFC-UI-001-020 | P10 | UI Framework | 🔲 |
| RFC-GFX-001-020 | P10 | Graphics | 🔲 |
| RFC-REN-001-018 | P10 | Rendering | 🔲 |
| RFC-TH-001-016 | P10 | Theme System | 🔲 |
| RFC-SC-001-018 | P11 | Scripting | 🔲 |
| RFC-API-001-020 | P11 | Public API | 🔲 |
| RFC-AI-001-010 | P12 | AI Integration | 🔲 |
| RFC-CL-001-008 | P13 | Collaboration | 🔲 |
| **Total** | | | **~300 RFCs** |

---

## Appendix A: Key Milestones

```
  P0 Complete: [ ]  ─── Repository ready, CI green, build works
  P1 Complete: [ ]  ─── Audio plays through speakers
  P2 Complete: [ ]  ─── MIDI records and plays back
  P3 Complete: [ ]  ─── Timeline seeks and loops
  P4 Complete: [ ]  ─── Piano Roll creates and edits notes
  P5 Complete: [ ]  ─── Mixer controls volume and routing
  P6 Complete: [ ]  ─── Third-party plugins load and process
  P7 Complete: [ ]  ─── Automation records and plays
  P8 Complete: [ ]  ─── Full arrangement + session workflow
  P9 Complete: [ ]  ─── Browser searches and previews samples
  P10 Complete: [ ]  ─── GPU UI renders at 144 FPS
  P11 Complete: [ ]  ─── Lua scripts extend the DAW
  P12 Complete: [ ]  ─── AI searches samples semantically
  P13 Complete: [ ]  ─── Projects sync across devices
  
  v1.0.0 Release: [ ]
```

## Appendix B: Dependencies Between Phases

```
P0 ──── P1 ──── P3 ──── P4 ──── P8 ──── P10 ──── P11 ──── P14
  │      │               │                            │
  │      └─── P2 ──── P6 ──── P7 ──── P5             │
  │                                                    │
  └────────────────────── P9 ───────────────────── P12 ┘
                                                        │
                                                    P13 ┘

P0 (Foundation): No dependencies
P1 (Audio):      P0
P2 (MIDI):       P0
P3 (Timeline):   P1, P2
P4 (Piano Roll): P2, P3, P10
P5 (Mixer):      P1
P6 (Plugin):     P1, P2
P7 (Automation): P1, P3
P8 (Views):      P3, P4, P5, P6, P7
P9 (Browser):    P0
P10 (GPU UI):    P0
P11 (Scripting): P8, P10
P12 (AI):        P9
P13 (Collab):    P8, P12
P14 (Polish):    All
```
