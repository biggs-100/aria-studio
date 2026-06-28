# Design: P6 — Plugin Host

## Technical Approach

Build a plugin host layer that bridges third‑party formats (CLAP, VST3) and native ARIA DSP through a unified `AudioPlugin` interface. Plugins run on dedicated sandbox threads with watchdog isolation — the audio thread never blocks. An adapter (`PluginAudioNode`) wraps `AudioPlugin` as `AudioNode` so the existing mixer chain stays unchanged. Scanning is incremental by file mtime with a JSON cache in `~/.aria/`. Built‑in DSP registers as CLAP through `NativeCLAPFactory` — no internal fast path.

## Architecture Decisions

| Decision | Option A | Option B | Choice & Rationale |
|----------|----------|----------|-------------------|
| **Thread model** | Process audio-thread-inline | Per‑plugin worker thread + watchdog | **B**. Per‑plugin thread is the spec'd Level 1 sandbox. Audio thread queues process requests; worker threads execute. Watchdog monitors each thread's deadline. |
| **CLAP wrapping** | Full C++ OOP abstraction | Thin struct wrappers around C API | **Thin wrappers**. CLAP C API is stable and well‑documented. A full OOP layer adds maintenance cost with no benefit. Use `clap_host_bridge` RAII struct to manage extensions. |
| **AudioPlugin ↔ AudioNode** | AudioPlugin inherits AudioNode | PluginAudioNode adapter wraps AudioPlugin | **Adapter**. AudioNode's `process()` signature differs (no MIDI, no parameter events). An adapter keeps existing `TrackProcessor` untouched while AudioPlugin stays clean. |
| **Watchdog timeout** | Hardcoded 100ms | Per‑category configurable defaults | **Configurable**. Reverbs need 500ms; synths 50ms. Default per category, user override per instance. Escalate: warn → skip next → bypass. |
| **State persistence** | Custom binary format | Plugin‑owned byte vector | **Plugin‑owned byte vector** (CLAP `clap_state` / VST3 `IBStream`). Host stores opaque bytes in the project file — no format to maintain. |
| **Plugin factory** | Subclass per format | Registry singleton with format‑specific creators | **Registry singleton** (`PluginFactory::instance()`), matching existing `ServiceLocator` pattern. Format scanners register creators. `NativeCLAPFactory` is just another scanner at startup. |

## Data Flow

```text
Audio callback (real‑time)
       │
       ▼
TrackProcessor ──[AudioBuffer]──► PluginAudioNode (adapter)
                                        │
                                        ▼
                                   AudioPlugin::process() sandbox queue
                                        │
                          ┌─────────────┼─────────────┐
                          ▼             ▼             ▼
                   CLAPHost        VST3Host     NativeCLAPPlugin
                   (sandbox        (sandbox      (sandbox
                    thread)         thread)       thread)
                          │             │             │
                          └─────────────┼─────────────┘
                                        ▼
                                  Watchdog thread
                                  (monitors deadlines)
                                        │
                                        ▼
                              PluginBlacklist::report_crash()
                                    if timeout
```

MIDI events flow via `AudioPlugin::set_midi_events()` before `process()`. Parameter automation arrives in `ProcessContext.parameter_changes` with sample offsets.

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/plugin/audio_plugin.h` | Create | Base `AudioPlugin` interface — lifecycle, audio/MIDI, params, state, latency |
| `src/plugin/audio_plugin.cc` | Create | Common parameter manager, state serialization helpers |
| `src/plugin/audio_plugin_types.h` | Create | `ParamID`, `ProcessContext`, `PluginID`, `PluginCategory`, `ParameterInfo` |
| `src/plugin/clap_host.h` | Create | CLAP host implementation — wraps `clap_plugin_entry`, hosts extensions |
| `src/plugin/clap_host.cc` | Create | CLAP plugin loading, extension query, process bridge |
| `src/plugin/vst3_host.h` | Create | VST3 host — `IComponent`/`IEditController` separation |
| `src/plugin/vst3_host.cc` | Create | VST3 module loading, bus config, process bridge |
| `src/plugin/plugin_factory.h` | Create | Singleton `PluginFactory` — registry of descriptors + format scanners |
| `src/plugin/plugin_factory.cc` | Create | Discover/create by ID, search, text query |
| `src/plugin/plugin_scanner.h` | Create | `PluginScanner` — incremental mtime scan, JSON cache |
| `src/plugin/plugin_scanner.cc` | Create | File enumeration, mtime diff, atomic cache write |
| `src/plugin/format_scanner.h` | Create | Abstract `FormatScanner` interface for CLAP/VST3 scanners |
| `src/plugin/clap_scanner.h` | Create | CLAP `.clap` binary scanner |
| `src/plugin/vst3_scanner.h` | Create | VST3 `.vst3` bundle scanner |
| `src/plugin/plugin_sandbox.h` | Create | `PluginSandbox` — per‑plugin worker thread + watchdog |
| `src/plugin/plugin_sandbox.cc` | Create | Thread pool, deadline tracking, crash/terminate handling |
| `src/plugin/plugin_blacklist.h` | Create | `PluginBlacklist` — crash tracking, warning/disabled/banned |
| `src/plugin/plugin_blacklist.cc` | Create | JSON persist at `~/.aria/blacklist.json`, escalation logic |
| `src/plugin/plugin_audio_node.h` | Create | `PluginAudioNode` — adapter from `AudioPlugin` to `AudioNode` |
| `src/plugin/plugin_audio_node.cc` | Create | Buffers input audio, forwards parameter automation |
| `src/plugin/native_clap_factory.h` | Create | `NativeCLAPFactory` — registers built‑in DSP as CLAP plugins |
| `src/plugin/native_clap_factory.cc` | Create | EQ, compressor, reverb, delay, synth CLAP entries |
| `src/plugin/plugin_host.h` | **Modify** | Extend `PluginHost` — facade that wires factory, scanner, sandbox |
| `src/plugin/plugin_host.cc` | **Modify** | Implementation delegating to sub‑components |
| `CMakeLists.txt` | Modify | Add `PLUGIN_SOURCES` set, link CLAP/VST3 SDK targets |
| `tests/unit/test_plugin_*.cc` | Create | Unit tests per component |
| `tests/CMakeLists.txt` | Modify | Add `aria_plugin_tests` executable |
| `vendor/clap/` | Create | CLAP C API headers (v1.2+, fetched via FetchContent or git submodule) |
| `vendor/vst3sdk/` | Create | Steinberg VST3 SDK (optional, behind `ARIA_FEATURE_VST3`) |

## Interfaces

```cpp
// src/plugin/audio_plugin_types.h
namespace aria {
using ParamID = uint32_t;
using PluginID = std::string;

struct ParameterInfo { ParamID id; std::string name, unit;
    double min, max, default_value; uint32_t step_count; };
struct ProcessContext { float** inputs, **outputs;
    uint32_t input_channels, output_channels, frames;
    uint64_t sample_position; double sample_rate, tempo;
    bool is_playing, is_recording, transport_changed;
    struct ParamChange { ParamID id; double value;
        uint32_t sample_offset; };
    std::span<const ParamChange> parameter_changes; };
}

// src/plugin/audio_plugin.h — base interface
class AudioPlugin {
public: virtual ~AudioPlugin() = default;
    virtual bool init() = 0;
    virtual bool activate(double sr, uint32_t min, uint32_t max) = 0;
    virtual void deactivate() = 0;
    virtual void process(const ProcessContext& ctx) = 0;
    virtual void set_midi_events(const MidiEventList&) = 0;
    virtual uint32_t parameter_count() const = 0;
    virtual ParameterInfo param_info(uint32_t index) const = 0;
    virtual double param_value(ParamID) const = 0;
    virtual void set_param_value(ParamID, double) = 0;
    virtual void begin_edit(ParamID) = 0;
    virtual void end_edit(ParamID) = 0;
    virtual bool save_state(std::vector<uint8_t>&) const = 0;
    virtual bool load_state(const std::vector<uint8_t>&) = 0;
    virtual uint32_t latency() const = 0;
    virtual uint32_t tail_samples() const = 0;
    virtual void set_bypass(bool) = 0;
    virtual bool is_bypassed() const = 0;
    virtual PluginID id() const = 0;
    virtual PluginCategory category() const = 0;
    // ... (name, vendor, format, version — from descriptor)
};

// src/plugin/plugin_audio_node.h — adapter
class PluginAudioNode : public AudioNode {
    std::unique_ptr<AudioPlugin> plugin_;
    std::unique_ptr<PluginSandbox> sandbox_;
    AudioBuffer scratch_[2]; // double-buffered I/O
    void process(ProcessContext& ctx) override;
};
```

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Unit | `AudioPlugin` base, `ParamID`, `ParameterInfo` | Pure C++ — no plugins needed |
| Unit | `PluginBlacklist` escalation, persist | JSON file I/O with temp dirs |
| Unit | `PluginScanner` mtime diff logic | Mock filesystem with known mtimes |
| Unit | Watchdog timeout + crash detection | Inject a `process()` that sleeps too long |
| Integration | `PluginAudioNode` adapter → `TrackProcessor` | Use `TestPlugin` harness (see below) |
| E2E | CLAP plugin lifecycle | Load a real `.clap` if available; skip if not |

**TestPlugin harness**: A minimal `AudioPlugin` subclass that returns configurable latency, accepts parameters, optionally simulates crashes/timeouts. Compiled into the test binary — no real plugin needed for most tests.

```cpp
class TestPlugin : public AudioPlugin {
    bool simulate_crash_ = false;
    uint32_t simulate_latency_ = 0;
    uint32_t process_duration_ms_ = 0;
    std::unordered_map<ParamID, double> params_;
    // ... implement all AudioPlugin methods
};
```

## Migration / Rollout

No migration required — plugin host is greenfield. The existing `PluginHost` stub is replaced with the full implementation. The `AudioNode`-based mixer continues working; `PluginAudioNode` adapts `AudioPlugin` instances as they're loaded.

## Open Questions

- [ ] CLAP headers: FetchContent vs git submodule? Proposal: FetchContent for build reproducibility.
- [ ] VST3 SDK license: Steinberg requires acceptance — document manual download step if FetchContent can't auto‑fetch.
- [ ] What thread priority should sandbox threads use? Real‑time (SCHED_FIFO) on Linux, `SetThreadPriority(THREAD_PRIORITY_TIME_CRITICAL)` on Windows?
- [ ] Plugin crash recovery: should we attempt N restarts before blacklisting, or is 1 crash → bypass sufficient for v1?
