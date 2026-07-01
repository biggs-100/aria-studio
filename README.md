# ARIA DAW

> Estación de trabajo de audio digital modular, acelerada por GPU y multilenguaje.

ARIA es una DAW (Digital Audio Workstation) moderna construida desde cero con C++23 para el núcleo de audio en tiempo real, Rust para servicios de base de datos/red, TypeScript para la UI declarativa y Lua para scripting de usuario. Renderizado GPU via WebGPU (Dawn) + Skia Graphite.

## Estado

| Fase | Estado | Descripción |
|------|--------|-------------|
| P0–P11 | ✅ Implementado | Foundation, Audio, MIDI, Timeline, Piano Roll, Mixer, Plugin Host, Automation, Views, Browser, GPU UI, Scripting |
| P12–P14 | ⏳ Futuro | AI Integration, Collaboration, Polish & Beta |

**[Roadmap completo →](23_Planning.md)**

## Quick Start

```bash
# Requisitos: CMake 3.28+, Rust 1.75+, pnpm, Python 3, Visual Studio 2022+

# 1. Rust libraries
cd rust && cargo build && cd ..

# 2. UI dependencies
cd ui && pnpm install && cd ..

# 3. Configure & build
cmake --preset debug
cmake --build --preset debug

# 4. Run tests
ctest --preset debug --output-on-failure
```

> Alternativa con Docker: `docker build -t aria-daw .`

## Stack

| Lenguaje | Estándar | Rol |
|----------|----------|-----|
| **C++** | C++23 | Núcleo de audio, DSP, plugin host, kernel, gráficos |
| **Rust** | 2021 ed. | SQLite database, filesystem, networking, FFI bridge |
| **TypeScript** | ES2022 | UI declarativa (Component framework + VDOM) |
| **Lua** | 5.4.7 | Scripting de usuario, macros, automatización |

## Build Presets

| Preset | Tipo | LTO | ASAN | Tests | Uso |
|--------|------|-----|------|-------|-----|
| `debug` | Debug | OFF | OFF | ON | Desarrollo |
| `release` | Release | ON | OFF | ON | Pruebas de rendimiento |
| `dist` | Release | ON | OFF | OFF | Distribución final |
| `sanitized` | Debug | OFF | ON+UBSAN | — | Caza de bugs |

## Arquitectura

```
src/                         # ~150 archivos fuente
├── audio/                   # Audio engine, drivers, graph, export, metering
├── automation/              # Automation clips, modulation, envelopes
├── browser/                 # File browser + sample database
├── graphics/                # GPU rendering (Skia Graphite + Dawn)
├── kernel/                  # App lifecycle, event bus, service locator
├── midi/                    # MIDI engine + drivers (WinMM, CoreMIDI, ALSA)
├── mixer/                   # Channel strip, routing, sidechain, EQ
├── model/                   # Track, clip, session, freeze
├── pianoroll/               # Piano roll con GPU instanced rendering
├── plugin/                  # CLAP host + VST3 host + sandbox
├── scripting/               # Lua runtime, API bindings, macro recorder
└── main.cc

tests/                       # ~88 archivos, 1,010+ test cases (Catch2)
├── unit/                    # Tests unitarios por módulo
├── integration/             # Tests de integración (transport, freeze, WSOLA)
└── scripting/               # Tests de scripting Lua + file watcher

ui/                          # TypeScript UI
├── src/
│   ├── framework/           # Component base, reconciler, VDOM
│   ├── components/          # DAW widgets (Fader, MeterBar, ChannelStrip)
│   ├── docking/             # Split/tab/float layout system
│   ├── views/               # Arrangement + Session views
│   └── theme/               # Theme engine (JSON, token system)
└── tests/

rust/                        # Rust workspace (4 crates)
├── aria-bridge/             # C ABI → Rust FFI
├── aria-database/           # SQLite + FTS5
├── aria-filesystem/         # File watcher + indexer
└── aria-networking/         # Network services

vendor/                      # 11 dependencias vendored
├── dawn/ skia/              # WebGPU + GPU 2D rendering
├── lua/ sol2/               # Lua 5.4 + C++ bindings
├── clap/ vst3sdk/           # Plugin format headers
└── sqlite/ fmt/ spdlog/ json/ zstd/
```

## Testing

- **C++**: Catch2 v3.7.0 — 30+ test executables, 1,010+ test cases
- **Rust**: `cargo test` en cada crate del workspace
- **TypeScript**: Vitest + jsdom

```bash
# Todos los tests C++
ctest --preset debug --output-on-failure

# Tests Rust
cd rust && cargo test

# Tests UI
cd ui && pnpm test
```

## Plataformas

| Plataforma | Estado | Drivers |
|------------|--------|---------|
| Windows | ✅ | ASIO, WASAPI, WinMM MIDI, WebGPU D3D12 |
| macOS | 🔄 | CoreAudio, CoreMIDI, WebGPU Metal |
| Linux | 🔄 | ALSA/PipeWire, ALSA MIDI, WebGPU Vulkan |

## Licencia

Todos los derechos reservados. Pendiente de definir.

---

[Planificación y Roadmap](23_Planning.md) · [Especificaciones](openspec/specs/) · [Reportar un issue](https://github.com/biggs-100/aria-studio/issues)
