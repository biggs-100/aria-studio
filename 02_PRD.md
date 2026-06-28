# ARIA DAW — Product Requirements Document

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Style**: Modular master document — each subsystem has its own detailed specification.

---

## Table of Contents

1. [Product Overview](#1-product-overview)
2. [User Personas](#2-user-personas)
3. [Functional Requirements Overview](#3-functional-requirements-overview)
4. [Non-Functional Requirements](#4-non-functional-requirements)
5. [Module Specifications Index](#5-module-specifications-index)
6. [User Stories](#6-user-stories)
7. [Constraints & Assumptions](#7-constraints--assumptions)
8. [Glossary](#8-glossary)

---

## 1. Product Overview

### 1.1. Product Statement

ARIA is a **digital audio workstation** for music production, sound design, and performance. It combines the piano roll excellence of FL Studio, the session workflow of Ableton Live, and the modular flexibility of Bitwig — all built on a modern, GPU-accelerated, multi-language architecture designed for longevity.

### 1.2. Key Value Propositions

- **Best-in-class Piano Roll**: Vector-based MIDI editing with expression, MPE, and embedded automation
- **GPU-Accelerated Everything**: 144 FPS UI, smooth animations, hardware-accelerated waveform and spectrogram rendering
- **Modular by Design**: Independent engines that can be developed, tested, and replaced independently
- **AI-Integrated Workflow**: Semantic search, chord assistance, stem separation — built into the fabric of the DAW
- **Deeply Customizable**: Full theming, Lua scripting, customizable layouts and shortcuts
- **Multi-Language Architecture**: C++ for real-time, Rust for safety, TypeScript for UI, Lua for scripting, Python for AI
- **CLAP-Native Plugins**: All built-in synths, effects, and utilities are CLAP format — open, portable, and usable in other CLAP-compatible hosts

### 1.3. Target Platforms

| Platform | Support | Notes |
|---|---|---|
| Windows 10/11 | Primary target | ASIO, WASAPI |
| macOS 13+ | Primary target | CoreAudio, AU |
| Linux (Ubuntu 22.04+, Arch) | Secondary target | ALSA, PipeWire, LV2 |

### 1.4. Business Model

- **Perpetual license** with optional upgrade pricing
- No subscription requirement
- No telemetry without explicit consent
- Open-source core (engine layer) under a fair license
- Commercial license for the complete application

---

## 2. User Personas

### 2.1. The Producer (Primary)

**Name**: Alex
**Age**: 24
**Role**: Beatmaker / Electronic Music Producer
**DAW History**: FL Studio since version 11

**Needs**:
- Fast workflow from idea to beat
- Excellent piano roll with pattern-based composition
- Drum sequencing and sample chopping
- Quick export for social media previews

**Pain Points**:
- FL Studio's playlist/arrangement workflow feels dated
- Wants better stock plugins and effects
- Wants AI assistance for chord progressions and sample selection

**Success Metric**: Can produce a complete track from scratch in under 2 hours.

### 2.2. The Sound Designer

**Name**: Maya
**Age**: 31
**Role**: Sound Designer for Games
**DAW History**: Ableton Live, Bitwig

**Needs**:
- Deep modulation and automation
- Flexible routing for complex signal chains
- High-quality time-stretching and pitch-shifting
- Scripting for custom tools

**Pain Points**:
- Wants modulation that goes beyond LFO/Envelope
- Wants to build custom MIDI tools without Max4Live's overhead
- GPU-accelerated waveform editing

**Success Metric**: Can create a complex sound design patch with 20+ modulation routings without performance degradation.

### 2.3. The Composer

**Name**: James
**Age**: 38
**Role**: Film/TV Composer
**DAW History**: Cubase, Logic Pro

**Needs**:
- Robust arrangement view
- Time signature and tempo track
- Video import and scoring
- Professional export formats

**Pain Points**:
- Wants faster arrangement workflow
- Needs reliable VST3 compatibility for orchestral libraries
- Expressiveness in MIDI editing (velocity, CC, MPE)

**Success Metric**: Can score a 5-minute cue with tempo changes, orchestral template, and video sync.

### 2.4. The Performer

**Name**: Kai
**Age**: 29
**Role**: Live Electronic Musician
**DAW History**: Ableton Live

**Needs**:
- Rock-solid session view
- MIDI mapping and controller integration
- Low latency, glitch-free playback
- Scene launching and clip manipulation

**Pain Points**:
- Needs absolute stability for live performances
- Wants better visual feedback during performance
- Session view improvements over Ableton Live

**Success Metric**: Can perform a 1-hour live set with zero crashes or audio glitches.

### 2.5. The Mix Engineer

**Name**: Sara
**Age**: 42
**Role**: Mixing & Mastering Engineer
**DAW History**: Pro Tools, Reaper, Studio One

**Needs**:
- Professional mixer with flexible routing
- High-precision metering
- Export with dithering and sample rate conversion
- Compatibility with hardware controllers

**Pain Points**:
- Accuracy and reliability above all else
- Needs precise automation editing
- Wants better organization tools (groups, VCAs, buses)

**Success Metric**: Can mix a 40-track project with complex routing, automation, and external hardware inserts.

### 2.6. The Developer

**Name**: Robin
**Age**: 27
**Role**: Music Tech Developer
**DAW History**: Various, uses Reaper for scripting

**Needs**:
- Full scripting API (Lua)
- Public API for third-party extensions
- Console/logging for development
- Hot-reload for scripts

**Pain Points**:
- Wants a clean, documented API
- Lua as first-class citizen, not an afterthought
- Wants to build and share extensions easily

**Success Metric**: Can build a custom MIDI effect in Lua in under 30 minutes.

---

## 3. Functional Requirements Overview

This section defines the high-level functional requirements for ARIA DAW. Detailed specifications for each module are maintained in their respective documents.

### 3.1. Audio Engine

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| AE-001 | Real-time multi-track audio recording and playback | P0 | `specs/audio-engine/` |
| AE-002 | Support for 32-bit float and 64-bit float audio processing | P0 | `specs/audio-engine/` |
| AE-003 | Sample rates: 44.1k, 48k, 88.2k, 96k, 176.4k, 192k | P0 | `specs/audio-engine/` |
| AE-004 | Configurable buffer sizes (32-4096 samples) | P0 | `specs/audio-engine/` |
| AE-005 | Multi-client audio I/O (ASIO, WASAPI, CoreAudio, ALSA, PipeWire) | P0 | `specs/audio-engine/` |
| AE-006 | Sample-accurate audio graph with zero-latency monitoring | P0 | `specs/audio-engine/` |
| AE-007 | Internal mixing at 64-bit float precision | P0 | `specs/audio-engine/` |
| AE-008 | SIMD-accelerated (SSE, AVX, AVX2, AVX-512, ARM NEON) processing | P0 | `specs/audio-engine/` |
| AE-009 | Multi-core audio processing distribution | P1 | `specs/audio-engine/` |
| AE-010 | Offline bounce and stem export | P1 | `specs/audio-engine/` |

### 3.2. MIDI Engine

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| ME-001 | MIDI input/output via standard MIDI, USB MIDI, and virtual ports | P0 | `specs/midi-engine/` |
| ME-002 | MIDI clock send/receive with jitter-free timing | P0 | `specs/midi-engine/` |
| ME-003 | MPE (MIDI Polyphonic Expression) support | P0 | `specs/midi-engine/` |
| ME-004 | MIDI CC, NRPN, and SysEx handling | P1 | `specs/midi-engine/` |
| ME-005 | MIDI learn mapping system | P1 | `specs/midi-engine/` |
| ME-006 | MIDI quantize and humanize | P1 | `specs/midi-engine/` |

### 3.3. Piano Roll

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| PR-001 | Vector-based note rendering (GPU-accelerated) | P0 | `specs/piano-roll/` |
| PR-002 | Note object properties: pos, duration, velocity, color, channel | P0 | `specs/piano-roll/` |
| PR-003 | Expression editing (per-note CC, pitch bend, pressure) | P0 | `specs/piano-roll/` |
| PR-004 | MPE editing support | P0 | `specs/piano-roll/` |
| PR-005 | Tools: pencil, select, paint, erase, cut, glue | P0 | `specs/piano-roll/` |
| PR-006 | Scale highlighting and snap-to-scale | P0 | `specs/piano-roll/` |
| PR-007 | Chord generator and arpeggiator built-in | P1 | `specs/piano-roll/` |
| PR-008 | Ghost notes from other clips/tracks | P1 | `specs/piano-roll/` |
| PR-009 | Multi-monitor support (separate piano roll window) | P1 | `specs/piano-roll/` |
| PR-010 | Handle 100k+ notes with smooth scrolling/zooming | P1 | `specs/piano-roll/` |

### 3.4. Automation

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| AU-001 | Automation clips with Bezier curve editing | P0 | `specs/automation/` |
| AU-002 | Automation lanes per track and per parameter | P0 | `specs/automation/` |
| AU-003 | Modulation mapping (LFO, Envelope, Random, Step) | P0 | `specs/automation/` |
| AU-004 | Parameter modulation with nested modulation | P1 | `specs/automation/` |
| AU-005 | Automation recording from hardware controls | P1 | `specs/automation/` |
| AU-006 | Automation clip editing within piano roll | P1 | `specs/automation/` |

### 3.5. Mixer

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| MX-001 | Channel strip with volume, pan, mute, solo, phase | P0 | `specs/mixer/` |
| MX-002 | Flexible routing: sends, returns, sidechain, buses | P0 | `specs/mixer/` |
| MX-003 | VCA groups and track grouping | P0 | `specs/mixer/` |
| MX-004 | FX racks with drag-and-drop ordering | P0 | `specs/mixer/` |
| MX-005 | Professional metering (peak, RMS, LUFS, phase correlation) | P0 | `specs/mixer/` |
| MX-006 | Support for 1000+ tracks without performance loss | P1 | `specs/mixer/` |

### 3.6. Plugin Host

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| PH-001 | **CLAP is the native plugin format**. All ARIA built-in plugins (synths, effects, utilities) are CLAP. Native CLAP plugins load through the same pipeline as third-party CLAP plugins. | P0 | `specs/plugin-host/` |
| PH-002 | VST3 support (full spec compliance) for third-party plugins | P0 | `specs/plugin-host/` |
| PH-003 | CLAP support for third-party plugins (full spec compliance) | P0 | `specs/plugin-host/` |
| PH-004 | Audio Unit support (macOS) | P0 | `specs/plugin-host/` |
| PH-005 | LV2 support (Linux) | P1 | `specs/plugin-host/` |
| PH-006 | Plugin sandboxing (crash isolation) | P0 | `specs/plugin-host/` |
| PH-007 | Plugin delay compensation (full PDC) | P0 | `specs/plugin-host/` |
| PH-008 | Plugin preset browsing and management | P1 | `specs/plugin-host/` |
| PH-009 | Native ARIA plugins are discoverable and loadable by other CLAP-compatible DAWs | P1 | `specs/plugin-host/` |

### 3.7. Project Engine

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| PJ-001 | Project file format with backwards compatibility guarantee | P0 | `specs/project-format/` |
| PJ-002 | Non-destructive editing model | P0 | `specs/project-format/` |
| PJ-003 | Undo/redo with unlimited history (configurable) | P0 | `specs/project-format/` |
| PJ-004 | Auto-save and crash recovery | P0 | `specs/project-format/` |
| PJ-005 | Template system | P1 | `specs/project-format/` |

### 3.8. Views

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| VW-001 | Arrangement View: linear timeline with tracks and clips | P0 | `specs/views/` |
| VW-002 | Session View: clip-launching grid (Ableton-inspired) | P0 | `specs/views/` |
| VW-003 | Playlist View: pattern-based arrangement (FL-inspired) | P1 | `specs/views/` |
| VW-004 | Synchronized views: edits in one view reflect in others | P0 | `specs/views/` |

### 3.9. File System & Browser

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| FB-001 | SQLite-powered sample database with full-text search | P0 | `specs/file-system/` |
| FB-002 | File browser with favorites, tags, collections | P0 | `specs/file-system/` |
| FB-003 | Auto-indexing of watched folders | P1 | `specs/file-system/` |
| FB-004 | Waveform preview and audio preview in browser | P0 | `specs/file-system/` |
| FB-005 | Plugin database with parameter search | P1 | `specs/file-system/` |

### 3.10. User Interface

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| UI-001 | GPU-accelerated rendering at 144+ FPS | P0 | `specs/ui-framework/` |
| UI-002 | Declarative component system (TypeScript) | P0 | `specs/ui-framework/` |
| UI-003 | Complete theming: colors, spacing, typography, animations | P0 | `specs/theme-system/` |
| UI-004 | Docking system with flexible layout | P0 | `specs/ui-framework/` |
| UI-005 | Touch and pen input support | P1 | `specs/ui-framework/` |
| UI-006 | High-DPI / Retina display support | P0 | `specs/ui-framework/` |
| UI-007 | Multi-window and multi-monitor support | P1 | `specs/ui-framework/` |
| UI-008 | Full keyboard navigation and accessibility | P1 | `specs/ui-framework/` |

### 3.11. Scripting

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| SC-001 | Lua scripting runtime embedded in the application | P0 | `specs/scripting/` |
| SC-002 | Public API for plugin-like script extensions | P0 | `specs/scripting/` |
| SC-003 | Macro recording and playback | P1 | `specs/scripting/` |
| SC-004 | Script hot-reload for development | P1 | `specs/scripting/` |
| SC-005 | Community script marketplace (future) | P2 | `specs/scripting/` |

### 3.12. AI Features

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| AI-001 | Semantic sample search ("dark kick", "bright pad") | P1 | `specs/ai-engine/` |
| AI-002 | Key and tempo detection | P1 | `specs/ai-engine/` |
| AI-003 | Chord progression generation and suggestion | P1 | `specs/ai-engine/` |
| AI-004 | Stem separation (voice, drums, bass, other) | P2 | `specs/ai-engine/` |
| AI-005 | Mix suggestion based on genre analysis | P2 | `specs/ai-engine/` |
| AI-006 | Intelligent MIDI editing assistance | P1 | `specs/ai-engine/` |

### 3.13. Collaboration

| ID | Requirement | Priority | Module Spec |
|---|---|---|---|
| CL-001 | Project sharing with version history | P2 | `specs/collaboration/` |
| CL-002 | Real-time collaborative editing | P3 | `specs/collaboration/` |
| CL-003 | Cloud sync of user settings and templates | P2 | `specs/collaboration/` |

---

## 4. Non-Functional Requirements

### 4.1. Performance

| ID | Requirement | Target |
|---|---|---|
| NFR-001 | Audio latency | < 10ms round-trip at 64 sample buffer |
| NFR-002 | UI frame rate | 144 FPS on mid-range GPU (GTX 1660 / RX 580) |
| NFR-003 | Project load time | < 5 seconds for 100-track project |
| NFR-004 | Memory usage | < 2GB idle, < 8GB with 100-track project + plugins |
| NFR-005 | Plugin scan on startup | < 3 seconds (incremental) |
| NFR-006 | Audio engine CPU usage | < 5% at idle with empty project |
| NFR-007 | Export speed | Real-time or faster for 44.1k/24-bit |

### 4.2. Reliability

| ID | Requirement | Target |
|---|---|---|
| NFR-008 | Crash rate | < 0.1% of sessions |
| NFR-009 | Plugin crash isolation | Plugin crash never takes down the host |
| NFR-010 | Project recovery | Auto-save every 60 seconds, recoverable on next launch |
| NFR-011 | File format integrity | CRC32 checksums on project blocks |
| NFR-012 | Uptime stability | 24+ hours of continuous use without degradation |

### 4.3. Compatibility

| ID | Requirement | Target |
|---|---|---|
| NFR-013 | VST3 compliance | 100% of spec |
| NFR-014 | CLAP compliance | 100% of spec |
| NFR-015 | AU compliance (macOS) | 100% of spec |
| NFR-016 | LV2 compliance (Linux) | Core spec |
| NFR-017 | MIDI 2.0 | P2 target, not required for v1 |
| NFR-018 | ARA (Audio Random Access) | P2 target for v2 |

### 4.4. Portability

| ID | Requirement | Target |
|---|---|---|
| NFR-019 | Windows support | Windows 10 22H2+, Windows 11 |
| NFR-020 | macOS support | macOS 13 Ventura+ (Arm64 + x64) |
| NFR-021 | Linux support | Ubuntu 22.04+, Arch Linux (x64) |
| NFR-022 | ARM (Apple Silicon) | Native support from v1 |
| NFR-023 | ARM (Windows/Linux) | P2 target |

### 4.5. Security

| ID | Requirement | Target |
|---|---|---|
| NFR-024 | Plugin sandboxing | Separate process or isolated memory space |
| NFR-025 | File access | Project files only access their own directories |
| NFR-026 | Network | All networking must be opt-in, with clear user consent |
| NFR-027 | Crash reporting | No PII included in crash dumps without consent |

### 4.6. Maintainability

| ID | Requirement | Target |
|---|---|---|
| NFR-028 | Module independence | Each engine must be testable in isolation |
| NFR-029 | Interface contracts | Any module can be replaced if interface is preserved |
| NFR-030 | Documentation | Every public API must have docs before release |
| NFR-031 | Code coverage | > 80% for core engines, > 60% for UI |

---

## 5. Module Specifications Index

Each module has its own detailed specification document:

| Module | Spec Document | Status |
|---|---|---|
| Audio Engine | `specs/audio-engine/01-spec.md` | 🔲 Pending |
| MIDI Engine | `specs/midi-engine/01-spec.md` | 🔲 Pending |
| Piano Roll | `specs/piano-roll/01-spec.md` | 🔲 Pending |
| Automation | `specs/automation/01-spec.md` | 🔲 Pending |
| Mixer | `specs/mixer/01-spec.md` | 🔲 Pending |
| Plugin Host (CLAP-native) | `specs/plugin-host/01-spec.md` | 🔲 Pending |
| Project Format | `specs/project-format/01-spec.md` | 🔲 Pending |
| Views (Arrangement/Session) | `specs/views/01-spec.md` | 🔲 Pending |
| File System & Browser | `specs/file-system/01-spec.md` | 🔲 Pending |
| UI Framework | `specs/ui-framework/01-spec.md` | 🔲 Pending |
| Theme System | `specs/theme-system/01-spec.md` | 🔲 Pending |
| Graphics Engine | `specs/graphics/01-spec.md` | 🔲 Pending |
| Scripting (Lua) | `specs/scripting/01-spec.md` | 🔲 Pending |
| AI Engine | `specs/ai-engine/01-spec.md` | 🔲 Pending |
| Collaboration | `specs/collaboration/01-spec.md` | 🔲 Pending |
| Testing Strategy | `specs/testing/01-spec.md` | 🔲 Pending |
| Build System | `specs/build/01-spec.md` | 🔲 Pending |

---

## 6. User Stories

### 6.1. Core Workflow

```
As a producer
I want to create a new project, add audio and MIDI tracks,
record and edit them, add effects, and export a finished mix
So that I can produce music from start to finish in one application
```

### 6.2. Pattern-Based Production

```
As a beatmaker
I want to create patterns in the piano roll, arrange them in the playlist,
and quickly audition different arrangements
So that I can iterate on musical ideas rapidly
```

### 6.3. Live Performance

```
As a performer
I want to launch clips in session view, trigger effects,
and have everything stay sample-accurate and stable
So that I can perform live without technical anxiety
```

### 6.4. Sound Design

```
As a sound designer
I want to route audio through complex modulation chains,
automate any parameter with curves, and visualize signal flow
So that I can create evolving, expressive sounds
```

### 6.5. Custom Tooling

```
As a power user
I want to write Lua scripts that automate repetitive tasks,
create custom MIDI generators, and extend the DAW's functionality
So that I can build my ideal workflow
```

### 6.6. Sample Management

```
As a producer with a large sample library
I want to search samples by description, tag them, preview them,
and drag them into my project
So that I can find the right sound quickly without breaking flow
```

### 6.7. AI Assistance

```
As a producer stuck on a creative block
I want the DAW to suggest chord progressions based on my melody,
or find samples that match the mood of my track
So that I can overcome creative blocks faster
```

---

## 7. Constraints & Assumptions

### 7.1. Technical Constraints

1. **C++20/23 required**: Audio engine must use modern C++ for real-time safety and SIMD
2. **WebGPU required**: GPU abstraction layer must support Vulkan, Metal, and DirectX 12
3. **No Qt dependency**: UI is custom-built on WebGPU + Skia
4. **SQLite embedded**: No external database server required
5. **Offline-first**: All core features work without internet
6. **CLAP-native plugins**: All built-in ARIA plugins (synths, effects, utilities) use CLAP format. No proprietary plugin format.
7. **Plugin sandboxing**: Must prevent plugin crashes from taking down the host

### 7.2. Design Constraints

1. **Backwards compatibility**: Project files from v1 must open in v10+
2. **Non-destructive editing**: Source files are never modified
3. **Undo for everything**: Every user action must be undoable
4. **Real-time safety**: No memory allocation on the audio thread
5. **Deterministic playback**: Same project = same output, every time

### 7.3. Business Constraints

1. **Perpetual license option**: Users can buy once and own forever
2. **No mandatory subscription**: All features available as perpetual license
3. **No user tracking**: No analytics, no telemetry without consent
4. **Fair pricing**: Competitive with existing DAWs at similar feature levels

### 7.4. Assumptions

1. Users have a GPU that supports Vulkan 1.2+/Metal 3+/DirectX 12
2. Users have at least 8GB RAM, 16GB recommended
3. An ASIO driver is available on Windows for low-latency audio
4. Users are familiar with basic DAW concepts (tracks, clips, mixer, automation)
5. CLAP adoption will continue to grow as the open plugin standard. ARIA contributes to this by shipping CLAP-native plugins that work in any CLAP host.

---

## 8. Glossary

| Term | Definition |
|---|---|
| **Arrangement View** | Linear timeline view for composing and arranging the full song |
| **ASIO** | Audio Stream Input/Output — low-latency audio driver protocol |
| **Automation Clip** | A clip that contains parameter automation data (envelopes, curves) |
| **AU** | Audio Unit — Apple's plugin format |
| **Bezier Curve** | Parametric curve used for smooth automation shapes |
| **CLAP** | CLever Audio Plugin — open, modern plugin format |
| **Clip** | A segment of audio or MIDI data on a track |
| **DAW** | Digital Audio Workstation |
| **DSP** | Digital Signal Processing |
| **FX Rack** | Chain of effects on a mixer channel |
| **GPU Rendering** | Using the graphics card to render UI instead of the CPU |
| **LFO** | Low-Frequency Oscillator — used for modulation |
| **LV2** | Open plugin standard for Linux audio |
| **MIDI** | Musical Instrument Digital Interface |
| **MPE** | MIDI Polyphonic Expression — per-note pitch, timbre, pressure |
| **PDC** | Plugin Delay Compensation — automatic latency correction |
| **Piano Roll** | MIDI note editor in grid format |
| **Sample Rate** | Number of audio samples per second (e.g., 44100 Hz) |
| **Session View** | Clip-launching grid for improvisation and performance |
| **Skia** | Open-source 2D graphics library (used by Chrome, Android, Flutter) |
| **VCA** | Voltage-Controlled Amplifier — group fader control |
| **VST3** | Virtual Studio Technology — Steinberg's plugin format |
| **WebGPU** | Modern cross-platform GPU API abstraction (Vulkan/Metal/DirectX) |

---

## Appendix A: Priority Definitions

| Priority | Meaning |
|---|---|
| **P0** | Required for v1 release. Blocking if missing. |
| **P1** | Strongly desired for v1. Non-blocking but high value. |
| **P2** | Planned for v1.x or v2. Documented for architecture awareness. |
| **P3** | Future. Not yet designed or scheduled. |
