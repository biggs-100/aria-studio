## Exploration: P8 Arrangement & Session Views

### Current State Inventory

#### What EXISTS (implemented and tested)

| Component | Location | Status | Notes |
|-----------|----------|--------|-------|
| **Track Model** | `src/model/track.h/.cc` | ✅ Basic | `TrackType` enum (Audio/MIDI/Group/VCA/Return/Master), id, name, volume, pan, mute, solo. Tested. |
| **Arrangement** | `src/project/arrangement.h/.cc` | ✅ Basic | Track ordering, loop range, length, markers, tempo track reference. Tested. |
| **MIDI Clip** | `src/midi/midi_clip.h/.cc` | ✅ Complete | Notes, events, quantize, humanize, transpose, full serialization. Tested. Standalone class (no base). |
| **Automation Clip** | `src/automation/automation_clip.h/.cc` | ✅ Complete | Points, interpolation, evaluate, loop. Tested. `AutomationEngine` manages these. |
| **Timeline** | `src/timeline/timeline.h/.cc` | ✅ Complete | Bar/beat conversion, PPQN→seconds, time signature. Tested. |
| **Tempo Track** | `src/timeline/tempo_track.h/.cc` | ✅ Complete | Tempo/time-sig events, BPM queries. Tested. |
| **Transport** | `src/audio/transport.h/.cc` | ✅ Complete | Play/stop/record/pause, loop, punch, metronome. Tested. |
| **Audio Clock** | `src/audio/audio_clock.h` | ✅ Complete | Sample position, BPM, beat/measure queries. |
| **TrackProcessor** | `src/audio/track_processor.h/.cc` | ✅ Complete | Plugin chain, gain/pan/mute/solo, PDC. Time stretch is a pass-through placeholder. Tested. |
| **Markers** | `src/model/marker.h/.cc` | ✅ Complete | CRUD on marker list, range queries. Tested. |
| **OfflineRenderer** | `src/audio/export/offline_renderer.h/.cc` | ✅ Complete | Export to WAV/AIFF/FLAC/MP3/OGG, normalization, progress, cancel. Tested. **Per-track freeze does not exist.** |
| **ProjectManager** | `src/project/project_manager.h/.cc` | ✅ Basic | Project lifecycle, track CRUD, clip creation STUBS, arrangement access, undo integration. Tested. |
| **UI Views (stubs)** | `ui/src/views/index.ts` | ❏ Stubs | `ArrangementView`, `SessionView`, `MixerView` — empty method bodies. |
| **UI Components (stubs)** | `ui/src/components/index.ts` | ❏ Stubs | `Button`, `Slider` — empty render(). |

#### What's MISSING (needs full implementation)

| Component | Spec Reference | Gap |
|-----------|---------------|-----|
| **Base Clip class** | §4.1 Clip | No `Clip` base class. `MidiClip` and `AutomationClip` are unrelated types. No shared position/loop/fade/gain model. |
| **Audio Clip** | §4.2 AudioClip | **Does not exist.** No `AudioClip` class, no `AudioFileInfo`, no `WaveformCache`, no warp markers, no transients. |
| **ArrangementClipWrapper** | §4.4 AutomationClipWrapper | **Does not exist.** No way to place automation on the arrangement timeline. |
| **Session Model** | §6.1 Session | **Does not exist.** No `Session` class, no `SceneID`, no `ClipSlot`, no scene management, no launch logic. |
| **AudioTrack** | §3.1 AudioTrack | **Does not exist.** No clip storage, no crossfade management, no warp markers, no recording. |
| **MidiTrack** | §3.2 MidiTrack | **Does not exist.** No instrument hosting, MIDI routing, drum map, transpose per track. |
| **GroupTrack** | §3.3 GroupTrack | **Does not exist.** No children management, group mode, fold/unfold. |
| **VCATrack** | §3.4 VCATrack | **Does not exist.** No slaves, no proportional fader control. |
| **ReturnTrack** | §3.5 ReturnTrack | **Does not exist.** No FX chain management, no solo-safe. |
| **MasterTrack** | §3.6 MasterTrack | **Does not exist.** No master FX chain, hardware output, dither config. |
| **FreezeManager** | §10 Freeze & Bounce | **Does not exist.** OfflineRenderer is project-wide export only. No per-track freeze, no flatten, no stem bounce. |
| **Routing Model** | §8 Routing | **Does not exist.** No `RouteTarget`, `RouteInput`, `SendSlot`, `RoutingManager`. Track has no routing fields. |
| **FXChain Model** | §9 Track FX Chain | **Does not exist.** TrackProcessor has a vector of AudioNode for processing, but no `FXChain` model with bypass/mix/presets/PDC tracking. |
| **Fade/Crossfade** | §4.1 Clip fades | **Does not exist.** No clip fade-in/out, no crossfades between clips. |
| **Time Stretch** | §4.1 Clip time_stretch | **Pass-through placeholder.** `apply_time_stretch()` in TrackProcessor is empty. No `dsp/time_stretch/` directory exists. |
| **Dual-view sync** | §6 Session→Arrangement | **Does not exist.** No mechanism for recording session clips to arrangement. |
| **Arrangement UI** | §5.2 Arrangement UI | **Does not exist.** No timeline ruler, no clip rendering (waveform or MIDI), no drag-and-drop, no fade handles, no track headers. |
| **Session UI** | §6.2 Session UI | **Does not exist.** No clip slot grid, no scene launch buttons, no crossfader widget. |

### Affected Areas

- `src/model/clip.h/.cc` — **NEW**: Base Clip class (position, loop, fade, gain, mute, time-stretch, reverse)
- `src/model/audio_clip.h/.cc` — **NEW**: AudioClip with file ref, waveform cache, warp markers
- `src/model/automation_clip_wrapper.h/.cc` — **NEW**: Arrangement automation clip wrapper
- `src/model/track_types.h/.cc` — **NEW**: AudioTrack, MidiTrack, GroupTrack, VCATrack, ReturnTrack, MasterTrack
- `src/model/session.h/.cc` — **NEW**: Session view model (scenes, slots, launch)
- `src/project/arrangement.h/.cc` — **EXTEND**: Add clip-at-position queries, crossfade storage
- `src/project/project_manager.h/.cc` — **EXTEND**: Add session access, track subclass instantiation
- `src/audio/track_processor.h/.cc` — **EXTEND**: Wire to clip model for real-time playback
- `src/audio/export/offline_renderer.h/.cc` — **EXTEND**: Per-track freeze/bounce support
- `src/midi/midi_clip.h/.cc` — **REFACTOR**: Integrate under base Clip class
- `src/automation/automation_clip.h` — **REFACTOR**: Create AutomationClipWrapper for arrangement
- `src/dsp/time_stretch/` — **NEW**: Time-stretching module
- `src/model/routing.h/.cc` — **NEW**: RouteTarget, SendSlot, RoutingManager
- `src/model/fx_chain.h/.cc` — **NEW**: Track FX chain model
- `src/model/freeze_manager.h/.cc` — **NEW**: Freeze/bounce manager
- `ui/src/views/ArrangementView/` — **NEW**: TypeScript arrangement view renderer
- `ui/src/views/SessionView/` — **NEW**: TypeScript session view renderer
- `ui/src/components/Clip/` — **NEW**: Clip rendering component (waveform, MIDI mini)
- `tests/unit/test_clip.cc` — **NEW**: Clip tests
- `tests/unit/test_session.cc` — **NEW**: Session tests
- `tests/unit/test_track_types.cc` — **NEW**: Track type tests
- `CMakeLists.txt` — **UPDATE**: Add new source files

### Approaches

1. **Full P8 monolith** — Implement everything listed in 23_Planning.md at once
   - Pros: Coherent architecture, no backtracking
   - Cons: **Extremely high effort** (4-6 months per plan). Risk of scope creep and abandonment.
   - Effort: **Very High**

2. **Foundation-first (recommended)** — Implement in strict dependency order:
   - Phase A: Clip model + track specializations + routing (prerequisites)
   - Phase B: Arrangement view model + rendering
   - Phase C: Track groups & VCAs
   - Phase D: Session view
   - Phase E: Freeze & bounce + dual-view sync
   - Pros: Each phase is independently testable, iterable, and reviewable. Early phases unlock downstream work.
   - Cons: Takes longer to get to "visible" features (session view).
   - Effort: **High** but decomposable into Medium sub-phases

3. **UI-first (risky)** — Build arrangement/session UI stubs first, wire model later
   - Pros: Fast visual progress, can iterate UI with stakeholders
   - Cons: UI without model is fake — massive rework when real data structures arrive. High integration cost.
   - Effort: **Medium** initially, **Very High** overall (double work)

### Recommendation

**Approach 2: Foundation-first**. The clip model and track specializations are prerequisites for literally everything else — arrangement rendering, session view, freeze/bounce, routing, dual-view sync. Trying to build arrangement rendering without clip/position data structures is building on sand.

The recommended build order for P8 proposal:

1. **Foundation Layer** (~40% of P8 effort):
   - Base `Clip` class (position, length, fades, gain, looping, time-stretch metadata)
   - `AudioClip` with file reference and waveform cache
   - Integrate `MidiClip` under `Clip` base
   - `AutomationClipWrapper` for arrangement timeline
   - All track types (`AudioTrack`, `MidiTrack`, `GroupTrack`, `VCATrack`, `ReturnTrack`, `MasterTrack`) with clip storage
   - Routing model (`RouteTarget`, `SendSlot`, `RoutingManager`)
   - Update `Arrangement` to hold clip references per track

2. **Arrangement View** (~30%):
   - Timeline ruler rendering (bar/beat grid, loop range, markers)
   - Clip rendering pipeline (waveform for audio, mini piano roll for MIDI)
   - Track headers (name, color, controls, mute/solo/arm)
   - Clip drag-and-drop placement
   - Fade handles and crossfade visualization
   - Real-time playback integration (wire TrackProcessor to clip model)

3. **Track Groups & VCAs** (~10%):
   - Group summing (audio graph changes)
   - VCA fader linking
   - Folder mode visual in arrangement

4. **Session View** (~15%):
   - Session/Slot/Scene model
   - Scene launch logic and quantization
   - Follow actions
   - Session UI grid rendering
   - Crossfader

5. **Freeze/Bounce & Dual-View Sync** (~5%):
   - Per-track freeze using OfflineRenderer
   - Flatten and stem bounce
   - Session→Arrangement recording
   - Arrangement→Session extraction

### Risks

| Risk | Level | Mitigation |
|------|-------|------------|
| **Volume of work** | 🔴 High | P8 is the largest phase. Foundation-first ensures incremental value. Split into chained PRs. |
| **Clip model affects everything** | 🔴 High | Must be designed correctly upfront. The architecture decisions here cascade to all downstream phases. |
| **Waveform rendering performance** | 🟡 Medium | Need efficient downsampling/LOD for large audio files and many visible tracks. |
| **Session→Arrangement sync complexity** | 🟡 Medium | Transport sync, clip recording, undo integration across two views. Architecturally tricky. |
| **Time stretch algorithm** | 🟡 Medium | Requires real-time capable implementation (WSOLA, phase vocoder, or elastique-licensed). Build on existing DSP framework. |
| **Track audio graph changes** | 🟡 Medium | Adding Group summing and VCA control requires changes to `AudioGraph` topology management. |
| **Undo for clip operations** | 🟢 Low | Existing `UndoStack` infrastructure is solid. Need to wire clip mutations through it. |
| **Testing coverage** | 🟢 Low | Good Catch2 infrastructure in place. TDD is configured as mandatory (`strict_tdd: true`). |

### Key Architecture Decisions Needed

1. **Clip ownership model**: Who owns clips? Options:
   - **Track-owns**: Each track owns its clips (most intuitive for audio/MIDI tracks)
   - **Arrangement-owns**: Arrangement holds all clip references, tracks reference them (cleaner for session→arrangement sync)
   - **Hybrid**: ProjectManager owns clips, tracks and arrangement reference by ID (flexible but complex)
   - **Recommendation**: Track-owns for AudioTrack/MidiTrack clips. Arrangement references by ClipID for display. Session has separate clip instances.

2. **Base class placement**: Where does the `Clip` base class live?
   - `src/model/clip.h` — alongside Track, Marker (logical)
   - `src/project/clip.h` — next to Arrangement (pragmatic)
   - **Recommendation**: `src/model/clip.h`. The `model/` directory is the data model layer.

3. **Session location**: Separate `src/model/session.h` or inside `project/`?
   - **Recommendation**: `src/model/session.h`. Parallel to `Arrangement`.

4. **Rendering pipeline**: How to render clips in the arrangement view?
   - C++ Skia path (GPU-rendered, fast)
   - TypeScript Canvas path (flexible, existing infrastructure)
   - **Recommendation**: TypeScript to start, with Skia acceleration for waveform LOD when performance demands it. The existing `ui/` directory is TypeScript-based.

5. **TrackProcessor ↔ Clip model integration**: How does the real-time audio engine read clip data?
   - **Recommendation**: Lock-free snapshot. Track model provides a snapshot of active clips at sample position via atomic pointer swap. Audio thread reads snapshots, control thread writes next snapshots. No locks on audio thread.

### Ready for Proposal

**Yes**. This exploration surfaces the full picture:

- The gap is **very large** — P8 is the biggest phase so far by a significant margin
- The clip model foundation is the critical path prerequisite
- The recommended approach is **foundation-first** with 5 sub-phases delivered as chained PRs
- The orchestrator should tell the user: "P8 is substantial. Recommend we start with the proposal focusing on **Phase A: Clip Model + Track Specializations + Routing**. This is the foundation everything else depends on, and it's independently testable."
