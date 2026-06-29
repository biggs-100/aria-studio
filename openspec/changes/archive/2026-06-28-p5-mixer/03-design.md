# Design: P5 — Mixer

## Technical Approach

The mixer is a flat C++ class hierarchy where `Mixer` orchestrates processing across `Channel` strips, buses, `BuiltInEQ`, `MasterChannel`, `SidechainManager`, and `MixerUI`. All internal audio summation uses 64-bit float (`std::vector<double>`) to prevent truncation noise at high channel counts. The design follows a **data-oriented control flow**: `Mixer::process()` walks a pre-computed processing order, accumulates per-channel output into per-bus or master mix buffers, then sums buses into the master and applies the master chain.

## Architecture Decisions

| Decision | Option | Tradeoff | Chosen |
|---|---|---|---|
| Summing precision | 32-bit float vs 64-bit float | 64-bit uses 2x memory per sample but eliminates cumulative rounding error at 1000+ channels | **64-bit float** (`std::vector<double>`) |
| Channel hierarchy | Virtual `Channel` base with `MasterChannel` as subclass | `MasterChannel` has a very different process chain (limiter, dither) — sharing via inheritance adds complexity | **Separate `MasterChannel` class** (not a subclass of `Channel`) |
| EQ filter topology | FIR vs IIR biquad cascade | FIR requires higher order for steep slopes, costs more CPU | **IIR biquad cascade** (RBJ cookbook, zero-latency, per-channel state) |
| Pan law | Equal-power vs Linear vs StereoBalance | Equal-power preserves perceived loudness at center (−3 dB); linear keeps amplitude constant | **EqualPower_3dB** as default, 3 alternatives configurable via `PanLaw` enum |
| Solo exclusivity | Per-channel flag with bus awareness | Simple check: if any solo → non-soloed non-Return channels skip processing | **Flat solo scan** in `Mixer::process()` — 2-pass: detect → apply |
| Thread safety | Per-channel `std::atomic<double>` for volume/pan | Atomic doubles are lock-free on x86_64; no mutex contention in audio thread | **Per-field atomics** for level state; metering written by audio thread, read by UI |
| FX chain | Dedicated `FXChain` class vs inline slot vector | Dedicated class adds abstraction but the Slice 1 implementation only needs insert/bypass | **Inline `std::vector<FXSlot>`** in Channel + MasterChannel |
| UI rendering | Immediate-mode per-frame rebuild vs retained layout | Per-frame rebuild is simpler for initial implementation; layout caching can be added later | **Rebuild layout each render** in `MixerUI::render()` |

## Data Flow

```
AudioBuffer* inputs[] ──┐
                        ▼
        ┌──────────────────────────┐
        │    Mixer::process()      │
        │                          │
        │  1. Rebuild order (dirty)│
        │  2. Detect solo state    │
        │  3. For each channel:    │
        │     a. Skip muted/non-   │
        │        soloed/ MIDI/VCA  │
        │     b. Read input buffer │
        │     c. Apply width       │
        │        (mid/side)        │
        │     d. Apply pan law     │
        │        (equal-power      │
        │         sin/cos)         │
        │     e. Phase invert      │
        │     f. Gain + accumulate │
        │        into bus/master   │
        │        mix (double)      │
        │  4. Sum buses → master   │
        │  5. Master vol + invert  │
        │  6. Write output buffer  │
        │  7. Update metering      │
        └──────────────────────────┘
                        │
                        ▼
                AudioBuffer* master_output
```

### Master Channel Processing

```
Mixer sum (double) → float conversion → MasterChannel::process()
    1. Copy input → output (planar)
    2. Apply master volume (linear gain)
    3. Master FX chain (delegated to plugin host)
    4. Brickwall limiter (envelope follower, instant attack)
    5. Dither (Triangular or Shaped noise)
    6. Ceiling clamp (−0.3 dBFS safety margin)
```

## File Changes

| File | Lines | Role |
|---|---|---|
| `src/mixer/channel.h` | 181 | Channel strip class: atomics for volume/pan/mute/solo/phase/width, FX slots, sends, built-in EQ, metering |
| `src/mixer/channel.cc` | 116 | Volume clamping, FX chain CRUD, send management, `db_to_linear`/`linear_to_db` |
| `src/mixer/mixer.h` | 123 | `Mixer` engine: channel/bus CRUD, 4-configurable `PanLaw`, `MixerConfig`, `process()` |
| `src/mixer/mixer.cc` | 422 | Core process loop: solo detection, per-channel DSP (width → pan → phase → gain), 64-bit bus/master accumulation, metering |
| `src/mixer/routing.h` | 50 | `RouteTarget` (Master/Bus/Track/External/None), `AudioInput` (Internal/External/Sidechain) |
| `src/mixer/routing.cc` | 14 | Placeholder — data structures used by `Mixer::process()` directly |
| `src/mixer/sidechain.h` | 53 | `SidechainManager`: source→target routing, buffer preparation |
| `src/mixer/sidechain.cc` | 61 | Connection management; `process()` is a placeholder for future integration with Mixer loop |
| `src/mixer/builtin_eq.h` | 140 | `BiquadFilter` (RBJ cookbook, 7 types) + `BuiltInEQ` (8-band cascade, zero-latency) |
| `src/mixer/builtin_eq.cc` | 348 | Coefficient calculation (LowPass/HighPass/BandPass/Notch/AllPass/LowShelf/HighShelf/Peak), per-channel state, cascade processing |
| `src/mixer/master_channel.h` | 98 | `MasterChannel`: volume, `LimiterConfig`, `BrickwallLimiter`, dither (None/Triangular/Shaped), FX chain |
| `src/mixer/master_channel.cc` | 238 | Full master chain: copy → volume → FX → limiter → dither → ceiling clamp; envelope follower with peak detector |
| `src/mixer/mixer_ui.h` | 85 | `MixerUI`: Skia console renderer, 4 channel widths, layout config, mouse interaction |
| `src/mixer/mixer_ui.cc` | 193 | Layout rebuild per frame, channel strip drawing, fader/meter/pan/buttons, drag-to-volume mapping (2px ≈ 1 dB) |

All files compiled into `aria_core` static library via `CMakeLists.txt`.

## Interfaces / Contracts

```cpp
// Core processing contract — called from audio thread
void Mixer::process(AudioBuffer** inputs, uint32_t num_inputs,
                    AudioBuffer* master_output, uint32_t frames);

// Master chain contract
void MasterChannel::process(const AudioBuffer* input,
                            AudioBuffer* output, uint32_t frames);

// Biquad cascade contract (zero-latency)
void BuiltInEQ::process(const float* input, float* output,
                        uint32_t frames, uint32_t channels);

// Sidechain routing (placeholder — full integration pending)
void SidechainManager::process(uint32_t frames);

// UI contract
void MixerUI::render(SkiaCanvas* canvas, const Rect& bounds, Mixer& mixer);
```

## Testing Strategy

| Layer | What | Approach |
|---|---|---|
| Channel strip | Volume, pan, mute/solo, phase, width, automation, groups, VCA, FX, sends, EQ, metering, db conversion | `test_channel.cc` — Catch2 REQUIRE/Approx on all getters/setters and edge cases (clamping, default values) |
| Mixer engine | Init/shutdown, channel/bus CRUD, pan law, basic summing, bus summing, master volume, process edge cases | `test_mixer.cc` — Catch2 with `AudioBuffer` fixtures validating per-sample output against expected pan/gain math |
| Built-in EQ | Biquad coefficient calculation, band management, bypass, frequency response, cascade order | `test_builtin_eq.cc` |
| Master channel | Volume, limiter threshold/ceiling, dither, FX chain, ceiling clamp after processing | `test_master_channel.cc` |
| Routing/Sidechain | RouteTarget types, sidechain connection management, buffer preparation | `test_routing.cc` |

## Migration / Rollout

No migration required. Mixer is a net-new component — no existing code depends on the mixer API. Sidechain full integration (routing source audio through `Mixer::process()`) and `SendManager` are marked as future slices.

## Open Questions

- [ ] Sidechain `process()` is a placeholder — actual source audio routing through the Mixer loop is deferred
- [ ] `ReturnChannel` class (from `09_Mixer.md`) is not yet implemented — returns are represented as generic `ChannelType::Return`
- [ ] VCA engine that writes `vca_contribution_db_` per-channel does not exist yet
- [ ] `AudioBuffer` uses planar layout but `Mixer::process` interleaves internally — consider unified access pattern
