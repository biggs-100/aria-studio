# Tasks: P8b — Session View, AudioTrack/MidiTrack, Freeze & Bounce

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 1200–1500 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 (AudioTrack+MidiTrack) → PR 2 (WSOLA) → PR 3 (Session C++) → PR 4 (Session UI TS) → PR 5 (Freeze/Bounce) → PR 6 (Sync+Tests) |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | AudioTrack + MidiTrack subclass features | PR 1 | base for all downstream work |
| 2 | WSOLA time stretch in TrackProcessor | PR 2 | depends on PR 1 (track types exist) |
| 3 | Session C++ model (Scene, ClipSlot, LaunchState) | PR 3 | depends on PR 1 (tracks exist) |
| 4 | Session TS Canvas UI | PR 4 | depends on PR 3 (C++ model) |
| 5 | FreezeManager + per-track OfflineRenderer | PR 5 | depends on PR 1, PR 2 |
| 6 | Dual-view sync + integration tests | PR 6 | depends on PR 3, PR 4 |

## Phase 1: AudioTrack + MidiTrack subclasses

- [x] 1.1 Add freeze state accessors to `src/model/track.h/.cc`
- [x] 1.2 Declare AudioTrack with typed clip vector, crossfade map, session slot access in `src/model/track_types.h`
- [x] 1.3 Implement AudioTrack::clip_at(), clips_in_range(), evaluate_crossfade() in `track_types.cc`
- [x] 1.4 Declare MidiTrack with instrument, transpose, drum map in `track_types.h`
- [x] 1.5 Implement MidiTrack transposition, slot access in `track_types.cc`
- [x] 1.6 Add `create_audio_track()`, `create_midi_track()`, Session& access to `src/project/project_manager.h/.cc`

## Phase 2: WSOLA time stretch

- [x] 2.1 Create `src/audio/wsola.h/.cc` — WSOLAStretch class (analysis frame, similarity search, overlap-add)
- [x] 2.2 Add TimeStretchAlgorithm base class + crossfader gain input to `src/audio/track_processor.h`
- [x] 2.3 Integrate WSOLA in TrackProcessor::process() pipeline after clip sum, before gain/pan in `track_processor.cc`

## Phase 3: Session C++ model

- [x] 3.1 Create `src/model/session.h/.cc` — Session class (grid, scenes, LaunchState machine, FollowAction eval, Crossfader)
- [x] 3.2 Wire Session into ProjectData in `project_manager.h/.cc`

## Phase 4: Session UI (TypeScript)

- [x] 4.1 Create `ui/src/views/SessionView/types.ts` — SessionDataSource bridge types
- [x] 4.2 Create `ui/src/views/SessionView/ClipGrid.ts` — canvas grid renderer with virtual culling
- [x] 4.3 Create `ui/src/views/SessionView/SceneStrip.ts` — scene launch btn strip
- [x] 4.4 Create `ui/src/views/SessionView/CrossfaderWidget.ts` — crossfader slider
- [x] 4.5 Create `ui/src/views/SessionView/index.ts` — View binding, rAF loop, canvas
- [x] 4.6 Export SessionView from `ui/src/views/index.ts`

## Phase 5: Freeze & Bounce

- [x] 5.1 Add `render_track()` to OfflineRenderer in `offline_renderer.h/.cc`
- [x] 5.2 Create `src/model/freeze_manager.h/.cc` — FreezeManager (freeze, unfreeze, dirty flags, bounce)

## Phase 6: Dual-view sync + Integration tests

- [x] 6.1 Add capture-to-arrangement handler in `ui/src/views/ArrangementView/index.ts`
- [x] 6.2 Add "S" session badge in `ui/src/views/ArrangementView/ClipCanvas.ts`
- [x] 6.3 Add SessionCaptureClipData variant in `ui/src/views/ArrangementView/types.ts`
- [x] 6.4 Write unit tests: AudioTrack crossfade eval, MidiTrack transpose, WSOLA ratios, Session state machine, follow actions, crossfader gain
- [x] 6.5 Write unit tests: FreezeManager dirty flags, OfflineRenderer per-track isolation, Session→Arrangement capture
- [x] 6.6 Write integration tests: transport sync, freeze→unfreeze clip restoration, WSOLA polymorphism
