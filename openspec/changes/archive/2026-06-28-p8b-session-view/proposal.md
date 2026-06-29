# Proposal: P8b — Session View, AudioTrack/MidiTrack, Freeze & Bounce

## Intent

Complete the P8 milestone by implementing session view, typed track subclasses, time stretching, freeze/bounce, and dual-view sync. This enables Ableton-style session workflow in ARIA DAW — clip launching, scene triggering, and offline rendering.

## Scope

### In Scope
- AudioTrack + MidiTrack subclass features (crossfade pairs, instrument/transpose)
- In-house WSOLA time stretch in TrackProcessor
- Session View C++ model (Scene, ClipSlot, LaunchState, Crossfader, FollowAction)
- Session View TS Canvas UI (clip grid, scene launch, crossfader)
- Dual-view sync (session → arrangement record, transport position sync)
- Freeze & Bounce (FreezeManager, per-track offline render, unfreeze, stem bounce, master bounce)

### Out of Scope
- Rubber Band library integration (deferred premium upgrade)
- Arrangement → session extraction (less common workflow)
- Global crossfader curve shapes beyond linear/power

## Capabilities

### New Capabilities
- `session-view`: C++ data model (Scene, ClipSlot, LaunchState, Crossfader, FollowAction) + TypeScript Canvas UI (SessionView with clip grid, scene launch, crossfader)
- `freeze-bounce`: FreezeManager, per-track offline rendering, unfreeze, stem bounce, master bounce

### Modified Capabilities
- `track-types`: Extend AudioTrack with crossfade pair management; extend MidiTrack with instrument assignment and transpose offset. Requirements are already defined at basic level — this adds session-relevant behavior and implementation.
- `arrangement-view`: Add dual-view sync — capture scene to arrangement timeline, transport position synchronization
- `track-processor`: Add WSOLA time stretch algorithm with a polymorphic interface for future Rubber Band swap-in. No existing spec — this creates the first spec for this capability.

## Approach

Foundation-first: (1) AudioTrack/MidiTrack extensions, (2) WSOLA in TrackProcessor, (3) Session C++ model, (4) Session TS UI, (5) Freeze/Bounce. Each layer builds on the previous.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/core/track_types.h` | Modified | AudioTrack/MidiTrack subclass features |
| `src/core/track_processor.h` | Modified | WSOLA algorithm + time stretch interface |
| `src/model/session/` | New | Scene, ClipSlot, LaunchState, FollowAction models |
| `src/ui/session_view/` | New | Canvas-based SessionView component |
| `src/core/freeze_manager.h` | New | Freeze/bounce orchestration |
| `src/core/offline_renderer.h` | New | Per-track offline audio rendering |
| `src/ui/arrangement_view/` | Modified | Transport sync, session recording capture |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| WSOLA introduces audio artifacts at extreme ratios | Med | Unit-test with known signals, ±30% subjective review gate |
| Session→Arrangement sync drifts over long playback | Med | PPQN-based sync, periodic resync on bar boundaries |
| Freeze state invalidated by clip edit | Low | FreezeManager dirty-flag per track, auto-re-freeze on edit |
| Canvas UI performance with 100+ clips | Low | Virtual grid, clip culling outside viewport |

## Rollback Plan

Remove new files (`src/model/session/`, `src/ui/session_view/`, `freeze_manager`, `offline_renderer`). Revert modified `track_types`, `track_processor`, `arrangement_view` to pre-P8b state. All new code is additive — no existing behavior depends on it.

## Dependencies

- P8 foundation (TrackID, Clip model, Track types, Arrangement View TS) — must be fully committed
- Catch2 v3.7.0 for WSOLA unit tests

## Success Criteria

- [ ] AudioTrack creates/plays crossfade pairs between overlapping clips
- [ ] MidiTrack applies transpose offset during playback
- [ ] WSOLA time stretch produces artifact-free output for ±30% range
- [ ] Session View renders clip grid, launches scenes, crossfader affects volume
- [ ] Scene capture writes clips to arrangement timeline at correct PPQN
- [ ] Freeze renders track to audio file, unfreeze restores original clips
- [ ] Bounce exports stem and master mix to WAV
