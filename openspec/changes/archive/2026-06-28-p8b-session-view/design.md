# Design: P8b — Session View, AudioTrack/MidiTrack, Freeze & Bounce

## Technical Approach

Foundation-first layered build: (1) AudioTrack/MidiTrack subclass features, (2) WSOLA in TrackProcessor, (3) Session C++ model, (4) Session TS Canvas UI, (5) FreezeManager + per-track OfflineRenderer, (6) dual-view sync. Each layer is independently testable.

## Architecture Decisions

### Decision: AudioTrack/MidiTrack typed clip storage

| Option | Tradeoff | Choice |
|--------|----------|--------|
| Base Track holds `vector<ClipPlacement>` (current) | Works for both, but no crossfade/transpose-specific ops | **Rejected** — leaks type safety |
| AudioTrack has `vector<shared_ptr<AudioClip>>` + crossfade map; MidiTrack has `vector<shared_ptr<MidiClip>>` + instrument | Clean API per type, compile-time safety | **Chosen** — existing pattern (GroupTrack, VCATrack use private lists) |

AudioTrack stores crossfades in `unordered_map<pair<ClipID,ClipID>, Crossfade>` and evaluates overlap gain envelopes during playback queries. MidiTrack stores `PluginID instrument_`, `int8_t transpose_`, and the existing `DrumMap`. Both expose session clip slot access via `clip_in_slot(SceneID)`. Freeze state lives on Track base (already has `frozen_`, `show_frozen_`).

### Decision: WSOLA implementation in TrackProcessor

| Option | Tradeoff | Choice |
|--------|----------|--------|
| Inline in `apply_time_stretch()` | Tight coupling, hard to swap | **Rejected** |
| Abstract `TimeStretchAlgorithm` base — `WSOLAStretch` default impl | Polymorphic, swap-in ready (Rubber Band later) | **Chosen** — matches spec |

`WSOLAStretch` operates on `AudioBuffer` in-place. Pre-allocated internal ring buffer for overlap-add. Ratio clamped to [0.5, 2.0]. Called from `TrackProcessor::process()` after clip summing, before gain/pan. Real-time safe: zero allocations after `configure()`.

### Decision: Session data model lives in ProjectData

| Option | Tradeoff | Choice |
|--------|----------|--------|
| Session as standalone singleton | Weird ownership, global state | **Rejected** |
| Session owned by ProjectData alongside Arrangement | Natural lifecycle, serializable together | **Chosen** — `ProjectData::session` added |

Session owns the 2D grid: `unordered_map<TrackID, unordered_map<SceneID, shared_ptr<Clip>>>`. Scenes ordered as `vector<SceneID>` with names map. Playing state `unordered_map<TrackID, ClipID>` (one per track, stops previous). `LaunchState` machine: Stopped → TriggerQueued → Playing → Stopped. Follow actions trigger when clip playback exhausts.

### Decision: Session TS UI as standalone View

| Option | Tradeoff | Choice |
|--------|----------|--------|
| Embed inside ArrangementView | Complex state machine, one giant view | **Rejected** |
| Standalone `SessionView` implementing `View` interface | Clear separation, shared transport header only | **Chosen** — same pattern as ArrangementView |

`SessionView` binds to a canvas, owns sub-components (`ClipGrid`, `SceneStrip`, `CrossfaderWidget`), uses same `ArrangementDataSource`-style `SessionDataSource` bridge. Shared `Transport` position drives both views via the single `audio_engine.transport()`.

### Decision: Freeze pipeline uses OfflineRenderer per-track

| Option | Tradeoff | Choice |
|--------|----------|--------|
| Dedicated freeze render in FreezeManager | Duplicates render logic | **Rejected** |
| OfflineRenderer accepts `TrackID` filter + range | Single render path for freeze, bounce, export | **Chosen** — `OfflineRenderer::render(engine, config, optional<TrackID>)` |

FreezeManager stores `FrozenState { AudioClip frozen_clip; bool dirty; vector<PluginID> bypassed; }`. Freeze: render track FX to buffer → create AudioClip → set `frozen_clip` on track → bypass FX. Unfreeze: remove frozen clip → re-enable FX. Dirty flag set on clip/FX edit via observer.

### Decision: Dual-view sync via shared Transport

| Option | Tradeoff | Choice |
|--------|----------|--------|
| Separate session clock | Drift between views | **Rejected** |
| Shared `Transport` PPQN + quantization scheduler | Single source of truth, sample-accurate sync | **Chosen** — `transport().sample_position()` feeds both views |

Session clips launch with quantization: `launch_quantization` sets grid (Bar/Beat/Quarter/etc.). Queued launch waits for quantization boundary. `capture_scene_to_arrangement()` reads `playing_clips_` + transport position, writes `ClipPlacement` to arrangement track at snapped PPQN.

### Decision: Crossfader as global per-session

| Option | Tradeoff | Choice |
|--------|----------|--------|
| Per-scene crossfader | Complex UI, edge cases with scene switching | **Rejected** |
| One global crossfader (−1..1) with per-track A/B/Both assignment | Simple, matches Ableton workflow | **Chosen** — CrossfaderAssignment on Session model |

Crossfader gain applied at TrackProcessor level: each track gets a crossfader gain multiplier computed from its assignment + global position. Signal flow: clip mix → crossfader gain → track gain → FX → output.

### Decision: Follow actions in Session model

| Option | Tradeoff | Choice |
|--------|----------|--------|
| External scheduler | Adds indirection for simple trigger logic | **Rejected** |
| Session owns follow action evaluation after clip-end events | Co-located with state machine, easy to test | **Chosen** — `evaluate_follow_actions()` called when clip playback ends |

Follow actions: None, Stop, PlayNext, PlayRandom, ContinueAsLoop. Per-clip and per-scene. ContinueAsLoop restarts the clip; PlayNext advances to next scene in scene_order.

## Data Flow Diagrams

### Session clip launch → audio output

```
User clicks clip slot (UI)
        │
        ▼
Session::launch_clip(track, scene, options)
        │
        ├── Queued until quantization boundary
        │
        ▼
Session state: previous clip → Stopped, new clip → Playing
        │
        ▼
TrackProcessor::set_clip_slots({source_data, gain, stretch_ratio})
        │
        ▼  (audio thread, per process block)
┌───────────────────────────────────────┐
│ TrackProcessor::process()             │
│   │                                   │
│   1. Sum active clip slots → input    │
│   2. WSOLAStretch per-clip if ratio≠1 │
│   3. Crossfader gain (from Session)   │
│   4. Track gain/pan                   │
│   5. Plugin chain                     │
│   6. → output bus                     │
└───────────────────────────────────────┘
        │
        ▼
Mixer → Master → Hardware out
```

### Freeze pipeline

```
User clicks "Freeze" on Track T
        │
        ▼
FreezeManager::freeze_track(T)
        │
        ├── OfflineRenderer::render(engine, {range: project_range}, track=T)
        │       │
        │       ├── Process engine blocks (track-only, FX active)
        │       ├── Collect output buffer
        │       └── Return AudioClip with rendered samples
        │
        ├── Track::set_frozen_clip(rendered_clip)
        ├── Track::bypass_all_fx()
        └── Track::set_frozen(true)

Toggle "Show Frozen Audio"
        │
        ▼
Track::set_show_frozen_audio(true/false)
  ── Shows/hides waveform preview of frozen clip

Unfreeze
        │
        ▼
FreezeManager::unfreeze_track(T)
  ── Remove frozen_clip → re-enable FX → set_frozen(false)
```

### AudioTrack crossfade evaluation

```
Two clips overlapping:
  Clip A: [0, 960), Clip B: [864, 1824)
              overlap: 96 PPQN
  Crossfade defined: A→B, 96 PPQN, EqualPower

Playback at PPQN 912 (mid-overlap):
        │
        ▼
AudioTrack::evaluate_crossfade(ppqn=912)
        │
        ├── Find crossfade pair containing PPQN
        ├── Compute fade t = (912 - 864) / 96 = 0.5
        ├── gain_A = ::evaluateFade(EqualPowerOut, 0.5) ≈ 0.707
        └── gain_B = ::evaluateFade(EqualPowerIn,  0.5) ≈ 0.707

ClipA read * gain_A + ClipB read * gain_B → sum into output buffer
```

### Session → Arrangement sync

```
Shared Transport (single source of truth)
        │
        ├── Arrangement: timeline playhead at PPQN X
        ├── Session: quantization scheduler at PPQN X
        │
        ▼
User clicks "Capture Scene S1 to Arrangement"
        │
        ▼
Session::capture_scene_to_arrangement(S1)
        │
        ├── For each (track, playing_clip) in S1:
        │       ppqn = transport().sample_position() → PPQN
        │       snapped = snapToGrid(ppqn, grid_ppqn)
        │       Arrangement::place_clip(track, clip_copy, snapped)
        │
        └── Captured clips rendered with "S" badge
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/model/track.h` | Modify | Add freeze state accessors (already declared in spec) |
| `src/model/track.cc` | Modify | Implement freeze state |
| `src/model/track_types.h` | Modify | Add AudioTrack, MidiTrack subclass declarations + Crossfade struct |
| `src/model/track_types.cc` | Modify | Implement AudioTrack (crossfades, typed clip storage), MidiTrack (instrument, transpose) |
| `src/project/project_manager.h` | Modify | Add Session& to ProjectData, create_session(), Audio/MIDI track creation |
| `src/project/project_manager.cc` | Modify | Implement AudioTrack/MidiTrack creation, session access |
| `src/audio/track_processor.h` | Modify | Add TimeStretchAlgorithm base, WSOLAStretch, crossfader gain input |
| `src/audio/track_processor.cc` | Modify | Implement WSOLAStretch::process(), integrate in pipeline |
| `src/audio/wsola.h` | **Create** | WSOLAStretch class |
| `src/audio/wsola.cc` | **Create** | WSOLA implementation (analysis frame, similarity search, overlap-add) |
| `src/audio/export/offline_renderer.h` | Modify | Add optional TrackID filter to render() |
| `src/audio/export/offline_renderer.cc` | Modify | Implement per-track render mode |
| `src/model/session.h` | **Create** | Session class (SceneID, ClipSlot, LaunchState, Crossfader, FollowAction) |
| `src/model/session.cc` | **Create** | Session implementation |
| `src/model/freeze_manager.h` | **Create** | FreezeManager class |
| `src/model/freeze_manager.cc` | **Create** | FreezeManager implementation |
| `ui/src/views/SessionView/index.ts` | **Create** | SessionView (View interface, canvas, rAF loop) |
| `ui/src/views/SessionView/ClipGrid.ts` | **Create** | Grid renderer (scene rows × track columns) |
| `ui/src/views/SessionView/SceneStrip.ts` | **Create** | Scene launch buttons |
| `ui/src/views/SessionView/CrossfaderWidget.ts` | **Create** | Crossfader slider |
| `ui/src/views/SessionView/types.ts` | **Create** | SessionDataSource, session clip/scene data types |
| `ui/src/views/ArrangementView/index.ts` | Modify | Capture-to-arrangement handler |
| `ui/src/views/ArrangementView/ClipCanvas.ts` | Modify | "S" badge for captured clips |
| `ui/src/views/ArrangementView/types.ts` | Modify | SessionCaptureClipData variant |
| `ui/src/views/index.ts` | Modify | Export SessionView |

## Interfaces / Contracts

```cpp
// TimeStretchAlgorithm base
class TimeStretchAlgorithm {
public:
    virtual ~TimeStretchAlgorithm() = default;
    virtual void process(const float* input, float* output,
                         uint32_t frames, uint32_t channels,
                         double ratio) = 0;
    virtual void reset() = 0;
    virtual uint32_t latency() const = 0;
};

class WSOLAStretch : public TimeStretchAlgorithm { /* ... */ };

// Session model
class Session {
public:
    SceneID add_scene(const std::string& name);
    void set_clip_slot(TrackID, SceneID, shared_ptr<Clip>);
    Clip* clip_at(TrackID, SceneID) const;
    void launch_clip(TrackID, SceneID, LaunchOptions);
    void launch_scene(SceneID, LaunchOptions);
    Clip* playing_clip(TrackID) const;
    void capture_scene_to_arrangement(SceneID, Arrangement&, Transport&);
    // crossfader, follow actions, quantization...
};

// FreezeManager
class FreezeManager {
public:
    void freeze_track(TrackID);
    void unfreeze_track(TrackID);
    bool is_frozen(TrackID) const;
    bool is_dirty(TrackID) const;
    void mark_dirty(TrackID);
    void bounce(const BounceOptions&);
};

// Crossfade (AudioTrack)
struct Crossfade {
    ClipID clip_a;
    ClipID clip_b;
    uint32_t duration_ppqn;
    FadeShape shape;  // EqualPower for crossfade
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | AudioTrack clip queries, crossfade gain eval | Catch2 — known clip placements, verify gain_at_ppqn |
| Unit | MidiTrack transpose, instrument assignment | Catch2 — MIDI note in, verify output note shifted |
| Unit | WSOLA at 1.0x (pass-through), 2.0x (double speed), edge clamp | Catch2 — sine sweep, measure output length, FFT artifacts |
| Unit | Session launch/stop state machine, quantization queue | Catch2 — launch clip, verify state, verify queue |
| Unit | Session follow actions trigger after clip end | Catch2 — PlayNext, Stop, ContinueAsLoop |
| Unit | Crossfader gain evaluation | Catch2 — position -1..1, verify per-track gain |
| Unit | FreezeManager dirty flag on edit | Catch2 — freeze, edit clip, verify is_dirty |
| Unit | OfflineRenderer per-track render produces silence on others | Catch2 — two tracks, render one, verify other silent |
| Unit | Session→Arrangement capture creates correct ClipPlacement | Catch2 — capture scene, verify arrangement has clip at snapped PPQN |
| Unit | WSOLATimeStretch polymorphism | Catch2 — set RubberBandStub, verify delegate called |
| Integration | Transport sync — session launch at quantization boundary | Mock transport, advance PPQN, verify launch timing |
| Integration | Freeze→unfreeze restores original clips | Catch2 — freeze track with FX, verify clips intact after unfreeze |
| E2E | Session view renders grid with playing indicators | Puppeteer/playwright — mock data source, verify canvas pixels |

## Migration / Rollout

No migration required. All new code is additive. AudioTrack/MidiTrack classes replace the generic `new Track(TrackType::Audio)` path in ProjectManager; existing projects use the new subclasses upon next save.

## Open Questions

- [ ] WSOLA overlap window size: fixed 60ms or adaptive based on ratio?
- [ ] Crossfader gain curve: linear crossfade or equal-power for all modes?
