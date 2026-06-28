# ARIA DAW — Vision Document

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27

---

## 1. Purpose

ARIA is a next-generation digital audio workstation designed from the ground up as a **modular, GPU-accelerated, multi-language platform** for music production, sound design, and performance. It is not a clone of FL Studio, Ableton Live, or any existing DAW. It is a reimagination of what a DAW can be when built with modern engineering practices and a clear architectural philosophy.

ARIA exists to bridge the gap between professional-grade audio production and modern software design — bringing the performance of C++, the safety of Rust, the expressiveness of TypeScript, the flexibility of Lua, and the intelligence of Python into a single, coherent platform.

---

## 2. Philosophy

### 2.1. Architecture over Monolith

Traditional DAWs grew organically over decades, accumulating layers of technical debt. ARIA is designed as a collection of independent engines, each with a clear boundary, interface, and purpose. This modularity ensures that components can be developed, tested, and replaced independently without destabilizing the system.

```
ARIA DAW Architecture (Conceptual)
┌─────────────────────────────────────────────┐
│               UI Engine (TypeScript)          │
│  ┌─────────────────────────────────────────┐ │
│  │        Graphics Engine (WebGPU + Skia)   │ │
│  └─────────────────────────────────────────┘ │
├─────────────────────────────────────────────┤
│               Core Services                   │
│  ┌──────┐ ┌──────┐ ┌────────┐ ┌──────────┐ │
│  │Audio │ │ MIDI │ │ Plugin │ │Automation│ │
│  │Engine│ │Engine│ │ Host   │ │ Engine   │ │
│  └──────┘ └──────┘ └────────┘ └──────────┘ │
│  ┌──────┐ ┌──────┐ ┌────────┐ ┌──────────┐ │
│  │ DSP  │ │Project│ │ File   │ │   AI     │ │
│  │Engine│ │Engine│ │System  │ │ Engine   │ │
│  └──────┘ └──────┘ └────────┘ └──────────┘ │
├─────────────────────────────────────────────┤
│           Infrastructure (Rust)              │
│  ┌──────────┐ ┌──────────┐ ┌─────────────┐ │
│  │Database  │ │Networking│ │Scripting API│ │
│  │& Indexing│ │  & Sync  │ │   (Lua)     │ │
│  └──────────┘ └──────────┘ └─────────────┘ │
└─────────────────────────────────────────────┘
```

### 2.2. Right Language for the Right Job

ARIA rejects the "C++ for everything" approach.

| Domain | Language | Rationale |
|---|---|---|
| Audio Engine, DSP, MIDI | **C++20/23** | Real-time guarantees, SIMD/AVX/NEON, low-latency audio callbacks |
| Plugin Host (CLAP-native + VST3, AU) | **C++20/23** | CLAP is the native plugin format. All built-in plugins use CLAP. Binary compatibility with VST3 and AU for third-party. |
| Database, Indexing, Networking | **Rust** | Memory safety, fearless concurrency, zero-cost abstractions for auxiliary services |
| UI Framework, Rendering | **C++20/23 + TypeScript** | GPU-accelerated rendering (WebGPU + Skia) with declarative component model |
| Automation, Macros, Scripting | **Lua** | Embeddable, fast, proven in audio/creative tools |
| AI/ML Integration | **Python** | Ecosystem richness (transformers, ONNX, tensor libraries) |

### 2.3. GPU-First Rendering

Unlike traditional DAWs that render UI via CPU-bound frameworks (Qt, GDI, WinForms), ARIA renders everything on the GPU. Every pixel — waveforms, piano roll notes, knobs, meters, animations — is drawn through WebGPU (Vulkan/Metal/DirectX 12) with Skia as the 2D renderer. This enables:

- Silky smooth 144+ FPS animations
- Complex vector-based editors (piano roll as a vector canvas)
- Hardware-accelerated waveform and spectrogram rendering
- Theming that is performant and dynamic
- No distinction between "UI" and "canvas" — everything is a canvas

### 2.4. Intelligence as Infrastructure

AI is not a feature tab. It is a layer of the platform that enhances every interaction:

- Semantic sample search ("dark kick", "bright snare")
- Chord and progression suggestions
- Stem separation and source isolation
- Mix suggestions based on genre analysis
- Intelligent automation and MIDI editing assistance
- Key and tempo detection

---

## 3. Design Principles

### 3.1. Uncompromising Audio Quality

Everything starts with the audio engine. Zero compromises on latency, sample accuracy, or sound quality. The audio graph must be sample-accurate, the mixing engine must be 64-bit float end-to-end, and the DSP must support SIMD-accelerated processing.

### 3.2. Animations Are Not Decoration

What makes a DAW feel premium is not its color palette — it is **how it moves**. Every transition, hover, click, and state change has purposefully designed easing curves, timing, and feedback. Animations communicate state, guide attention, and make the tool feel alive.

### 3.3. The Piano Roll Is Not a Grid

The piano roll is the most important editor in a DAW. ARIA treats it as a **vector illustration canvas** where each note is a rich object with position, duration, velocity, color, channel, expression data, MPE, and embedded automation. Editing should feel as fluid as working in a vector design tool.

### 3.4. Modularity Is Non-Negotiable

Every engine must be independently compilable, testable, and replaceable. The audio engine does not depend on the UI engine. The project engine does not depend on the graphics engine. Contracts between modules are defined by interfaces, not implementations.

### 3.5. The User Owns Their Workflow

ARIA must be deeply customizable: themes, layouts, scripting, macros, keyboard shortcuts, and automation. If a user wants to build a custom tool, ARIA should not get in their way. The Lua scripting system is a first-class citizen, not an afterthought.

---

## 4. Target Audience

| Persona | Description | Key Needs |
|---|---|---|
| **Producer** | Beatmaker, electronic music producer | Piano roll, pattern-based workflow, fast iteration |
| **Sound Designer** | Creating sounds, samples, presets | Deep DSP, modulation, audio editing |
| **Composer** | Film/game/media composer | Arrangement view, score, orchestration, export |
| **Performer** | Live electronic musician | Session view, low latency, stability |
| **Mix Engineer** | Mixing and mastering | Console workflow, routing, metering, precision |
| **Developer** | Extending ARIA via scripting | Open API, Lua scripting, plugin SDK |

Primary target: **producers and sound designers** who want a modern, fast, and deeply integrated creative tool.

---

## 5. What ARIA WILL Do (Scope)

- Real-time multi-track audio recording and playback
- MIDI sequencing with industry-leading piano roll
- Advanced automation with Bezier curves and modulation
- Professional mixing console with flexible routing
- CLAP-native built-in plugins (synths, effects, utilities) — open and portable to any CLAP host
- VST3, CLAP, AU (macOS), LV2 (Linux) third-party plugin support
- Session View (clip-launching, Ableton-inspired) and Arrangement View
- GPU-accelerated UI with full theming system
- Integrated sample browser with SQLite-powered indexing
- Lua scripting for macro automation and custom tools
- AI-powered features: semantic search, chord generation, stem separation
- Collaboration through project sync and cloud infrastructure
- Lossless project format with backwards compatibility guarantees
- Full offline mode with no telemetry requirements

---

## 6. What ARIA Will NOT Do (First Release)

- Not a replacement for Pro Tools in post-production (v1 focus is music creation)
- Not a notation/score editor (v1 focus is piano roll and audio)
- Not a video editor or video scoring platform
- Not a cloud-only SaaS product (offline-first, sync is optional)
- Not subscription-only (fair business model, perpetual license option)
- Not a DJ tool (focus is production, not live DJ mixing)
- Not a hardware-dependent platform (runs on standard consumer hardware)

---

## 7. Competitive Differentiation

| Aspect | FL Studio | Ableton Live | Bitwig | ARIA |
|---|---|---|---|---|---|
| Piano Roll | ★★★★★ | ★★★☆☆ | ★★★★☆ | ★★★★★ + Vector |
| Session View | ★☆☆☆☆ | ★★★★★ | ★★★★☆ | ★★★★★ |
| Automation | ★★★☆☆ | ★★★★☆ | ★★★★★ | ★★★★★ + Bezier |
| GPU UI | ★★★★☆ | ★★★☆☆ | ★★★★☆ | ★★★★★ |
| GPU Rendering | Partial | No | Partial | Full (WebGPU) |
| Plugin Format | VST-only | VST+AU | VST+CLAP | **CLAP-native** + VST3, AU |
| Scripting | ★★★☆☆ | ★★★★☆ (Max) | ★★★★★ | ★★★★★ (Lua native) |
| AI Integration | ★★☆☆☆ | ★☆☆☆☆ | ★★★☆☆ | ★★★★★ (Built-in) |
| Modular Architecture | ★★☆☆☆ | ★★☆☆☆ | ★★★★☆ | ★★★★★ |
| Theme System | ★★★★☆ | ★☆☆☆☆ | ★★★☆☆ | ★★★★★ |
| Cross-Platform | Win/Mac | Win/Mac | Win/Mac/Linux | Win/Mac/Linux |

---

## 8. Technology Pillars

1. **C++20/23** — Audio engine, DSP, plugin host, real-time components
2. **Rust** — Database, indexing, networking, auxiliary services
3. **TypeScript** — UI layer (declarative components, themes, layout)
4. **WebGPU + Skia** — GPU-accelerated 2D rendering (Vulkan/Metal/DirectX 12)
5. **Lua** — Scripting, macros, automation extensions
6. **Python** — AI/ML model inference and tooling
7. **SQLite** — Metadata database, sample indexing, project cache
8. **CMake + Cargo + pnpm** — Multi-language build system

---

## 9. Development Phases (Macro Roadmap)

| Phase | Focus | Duration (est.) |
|---|---|---|
| **P0** | Documentation & Architecture | Design docs, RFCs, specs |
| **P1** | Audio Engine | Real-time audio graph, device I/O, buffer management |
| **P2** | MIDI Engine | MIDI I/O, timing, recording, playback |
| **P3** | Timeline & Transport | Playhead, looping, time signatures, tempo map |
| **P4** | Piano Roll | Vector-based MIDI editor, tools, quantization |
| **P5** | Mixer | Channel strip, routing, FX racks, metering |
| **P6** | Plugin Host | VST3, CLAP, AU, LV2 sandboxing |
| **P7** | Automation Engine | Envelopes, Bezier curves, modulation |
| **P8** | Arrangement & Session Views | Clip launching, arrangement compositing |
| **P9** | Browser & Database | Sample indexing, tag system, search |
| **P10** | GPU UI Framework | Widget system, themes, animations |
| **P11** | Scripting API | Lua runtime, macro system, public API |
| **P12** | AI Integration | Semantic search, chord tools, stem separation |
| **P13** | Collaboration & Sync | Project sharing, real-time sync |
| **P14** | Polish & Beta | Performance, stability, user testing |

---

## 10. Success Criteria

ARIA DAW is successful when:

1. A producer can go from idea to finished track entirely within ARIA
2. The piano roll is considered best-in-class by experienced users
3. The UI runs at 144 FPS on standard consumer hardware
4. The automation system enables creative modulation beyond what competitors offer
5. The Lua scripting API has a community creating and sharing extensions
6. The modular architecture allows third-party contributions without deep core knowledge
7. AI features genuinely improve workflow (not gimmicks)
8. The project format is stable and backwards-compatible

---

## 11. Non-Negotiables

- **Real-time safety**: No allocation in audio thread. Ever.
- **Data integrity**: Project files must be recoverable from corruption.
- **Backwards compatibility**: Projects saved in v1 must open in v10.
- **Offline-first**: Core features never require internet.
- **User privacy**: No analytics, no telemetry without explicit consent.
- **Deterministic playback**: Same project, same output, every time.

---

*"A DAW should feel like an instrument, not an application."*
