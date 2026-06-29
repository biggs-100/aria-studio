# Tasks: P8 Arrangement & Session Views (Foundation)

## Review Workload Forecast

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

| Field | Value |
|-------|-------|
| Estimated changed lines | ~800–1000 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending |

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | TrackID unification + Clip base | PR 1 | Base `main`; types.h, clip.h/.cc, refactor TrackID across model/arrangement/mixer |
| 2 | AudioClip + MidiClip refactor + AutomationClipWrapper | PR 2 | Base PR 1; audio_clip, wrap MidiClip, AutomationClipWrapper |
| 3 | Track subclasses + Routing | PR 3 | Base PR 2; GroupTrack, VCATrack, ReturnTrack, routing.h/.cc, PM extension |
| 4 | Mixer integration + TS Arrangement View | PR 4 | Base PR 3; group summing, VCA prop, Canvas timeline, fades |

## Phase 1: Foundation — Types, TrackID, Clip Base

- [x] 1.1 RED: Test TrackID struct (equality, ctor, uint64_t compat)
- [x] 1.2 Create `src/model/types.h` — `TrackID`, `ClipID`, `FadeShape`, `TrackType`
- [x] 1.3 Refactor `Track::id_` → `TrackID`, `Arrangement::TrackID` → model/types, `Channel::TrackID` → model/types, `Mixer::TrackID` → model/types
- [x] 1.4 RED: Test Clip base (position, end, looping, evaluate_fade at 0/50/100%)
- [x] 1.5 Create `src/model/clip.h/.cc` — abstract Clip, evaluate_fade(shape, t)
- [x] 1.6 Update `CMakeLists.txt` for new model sources

## Phase 2: AudioClip + MidiClip Refactor + AutomationClipWrapper

- [x] 2.1 RED: Test AudioClip (waveform LOD cache, sample_offset, gain envelope)
- [x] 2.2 Create `src/model/audio_clip.h/.cc` — AudioClip : Clip, WaveformCache
- [x] 2.3 Refactor `MidiClip` to inherit from Clip; update existing tests
- [x] 2.4 Create `src/model/automation_clip_wrapper.h/.cc`
- [x] 2.5 Update `CMakeLists.txt` for new sources

## Phase 3: Track Subclasses + Routing

- [x] 3.1 RED: Test GroupTrack (children CRUD, fold, Summing/Folder/Routing modes)
- [x] 3.2 RED: Test VCATrack (−6 slave → −12 effective, −∞ stays)
- [x] 3.3 RED: Test ReturnTrack (solo-safe, no clips → Master)
- [x] 3.4 RED: Test MasterTrack (cannot delete)
- [x] 3.5 Create `src/model/track_types.h/.cc` — Audio/Midi/Group/VCA/Return/MasterTrack
- [x] 3.6 Add virtual dtor, routing fields to `Track` base
- [x] 3.7 Create `src/model/routing.h/.cc` — RouteTarget, SendSlot, RoutingManager
- [x] 3.8 Extend `ProjectManager` for track subclass instantiation

## Phase 4: Mixer Integration — Group Summing + VCA

- [x] 4.1 RED: Integration test — group bus sums children, nested hierarchy
- [x] 4.2 RED: Integration test — VCA fader effective volume propagation
- [x] 4.3 Extend `Mixer::process` for nested group summing (child→parent bus)
- [x] 4.4 Wire `vca_contribution_db_` — PM pushes via `Channel::set_vca()`
- [x] 4.5 Extend `track_processor` to wire clips for audio playback

## Phase 5: Arrangement View — TypeScript Canvas

- [x] 5.1 Create `ui/src/views/ArrangementView/` with time-axis grid (bars/beats/16ths)
- [x] 5.2 Render clip rectangles with waveform thumbnails (LOD peaks)
- [x] 5.3 Implement clip selection, drag-drop with grid snap
- [x] 5.4 Render fade-in/out overlays and crossfade regions
- [x] 5.5 Wire `folded` GroupTrack → hide children in visible track list

## Phase 6: Integration Testing

- [x] 6.1 Write `tests/unit/test_clip.cc` — Clip base + AudioClip + evaluate_fade
- [x] 6.2 Write `tests/unit/test_track_types.cc` — Group/VCAT/Return/Master
- [x] 6.3 Extend `test_arrangement.cc` — clip_at, folded visible tracks
- [x] 6.4 Extend `test_project_manager.cc` — PM + Mixer group bus, VCA bridge
- [x] 6.5 Verify all existing tests pass (ctest --preset debug)
