# Design: P8 Arrangement & Session Views (Foundation Phase)

## Technical Approach

Foundation-first: (1) Clip model + AudioClip, (2) track type specializations, (3) arrangement TS Canvas rendering, (4) group/VCA mixer bridge. Unify `TrackID` first. Track-owns clips, Arrangement references by `ClipID`. TS Canvas for timeline; C++ provides LOD waveform data. ProjectManager bridges model→mixer for group/VCA topology.

## Architecture Decisions

| Decision | Choice | Alternatives | Rationale |
|----------|--------|-------------|-----------|
| TrackID unification | `struct TrackID` in `model/types.h` | 3 fragmented types | Single source of truth, backward-compat ctor |
| Clip base placement | `src/model/clip.h` | `src/project/` | model/ is data-model layer |
| Clip ownership | Track-owns `shared_ptr<Clip>` | Arrangement-owns, ProjectManager-owns | Simple audio-thread read; Arrangement queries by ClipID |
| Track specialization | Subclass `Track` | Composition + type-switch | Idiomatic OOP, direct `type()` dispatch |
| Rendering | TypeScript Canvas | C++ Skia | Matches existing `ui/` infra |
| Group summing | ProjectManager bridge | Direct model→mixer | Single orchestration point, no circular deps |
| VCA propagation | PM pushes to `vca_contribution_db_` atomic | Mixer reads VCA per-process | Lock-free audio thread read |
| Fade evaluation | `static evaluate_fade(shape, t)` | Virtual per-shape | Simple, testable, 5-variant enum |

## Data Flow

### Clip Model Hierarchy

```
Clip (abstract)                    ← src/model/clip.h
├── AudioClip                      ← src/model/audio_clip.h
├── MidiClip (refactored)           ← src/midi/midi_clip.h
└── AutomationClipWrapper           ← src/model/automation_clip_wrapper.h
```

### Group Summing Audio Flow

```
Child Tracks → per-channel gain/pan/FX → Group Bus (64-bit float)
       → GroupTrack Channel (FX, fader) → Master Mix → HW Out
```

### VCA Propagation

```
VCATrack.set_volume(-6) → PM::propagate_vca() → Channel::vca_contribution_db_
  → Audio thread: effective_volume() = volume_db_ + vca_contribution_db_
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/model/types.h` | Create | `TrackID`, `ClipID`, `FadeShape`, `TrackType` |
| `src/model/clip.h/.cc` | Create | Abstract `Clip` base |
| `src/model/audio_clip.h/.cc` | Create | `AudioClip` with WaveformCache LOD |
| `src/model/automation_clip_wrapper.h/.cc` | Create | Automation timeline wrapper |
| `src/model/track_types.h/.cc` | Create | AudioTrack, MidiTrack, GroupTrack, VCATrack, ReturnTrack, MasterTrack |
| `src/model/track.h/.cc` | Modify | Shared TrackID, virtual dtor, routing fields |
| `src/model/routing.h/.cc` | Create | RouteTarget, SendSlot, RoutingManager |
| `src/project/arrangement.h/.cc` | Extend | clip_at(), group-fold visible tracks, crossfades |
| `src/project/project_manager.h/.cc` | Extend | Track subclass instantiation, group/VCA bridge |
| `src/midi/midi_clip.h/.cc` | Refactor | Inherit from Clip |
| `src/mixer/channel.h` | Modify | TrackID → shared struct |
| `src/mixer/mixer.h/.cc` | Extend | Nested group summing, VCA audio-skip |
| `src/audio/track_processor.h/.cc` | Extend | Wire clip model for playback |
| `ui/src/views/ArrangementView/` | Create | TS Canvas timeline |
| `ui/src/views/index.ts` | Modify | Populate render() |
| `CMakeLists.txt` | Update | New MODEL/PROJECT sources |
| `tests/unit/test_clip.cc` | Create | Clip + AudioClip tests |
| `tests/unit/test_track_types.cc` | Create | Group/VCAT/Return/Master tests |

## Key Interfaces

```cpp
// src/model/clip.h
class Clip {
public:
    ClipID id() const;
    uint64_t start() const, length() const, end() const;
    uint64_t fade_in(), fade_out();
    FadeShape fade_in_shape(), fade_out_shape();
    double gain() const;  bool is_muted() const;
    static double evaluate_fade(FadeShape s, double t);  // t∈[0,1]
};

// GroupTrack
class GroupTrack : public Track {
    std::vector<TrackID> children_;
    GroupMode group_mode_ = Summing;
    bool folded_ = false;
};

// VCATrack
class VCATrack : public Track {
    std::vector<TrackID> slaves_;
    VCAAffects affects_ = All;
};
```

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Unit | Clip base: position, loop wrap, 5 fade shapes at 0/50/100% | test_clip.cc |
| Unit | AudioClip: file ref, WaveformCache LOD, gain envelope | test_clip.cc |
| Unit | GroupTrack: children CRUD, fold, 3 GroupModes | test_track_types.cc |
| Unit | VCATrack: proportion (-6 slave → -10 effective), ∞-stays | test_track_types.cc |
| Unit | ReturnTrack: solo-safe, FX chain | test_track_types.cc |
| Unit | MasterTrack: cannot-delete | test_track_types.cc |
| Unit | MidiClip under Clip: existing tests pass | Modify test_midi_main.cc |
| Unit | Arrangement: clip_at, folded visible tracks | Extend test_arrangement.cc |
| Integr. | PM + Mixer: group bus, VCA bridge | Extend test_project_manager.cc |

## Migration

No migration — no persisted state uses new types. Chained PRs recommended: (1) TrackID + Clip base, (2) AudioClip + MidiClip refactor + AutomationClipWrapper, (3) track subclasses + Routing, (4) Arrangement TS Canvas.

## Open Questions

- [ ] `MidiClip::length_ppqn` is `uint32_t` vs `Clip` base `uint64_t` — confirm alignment (PPQN fits 32-bit for any practical project).
