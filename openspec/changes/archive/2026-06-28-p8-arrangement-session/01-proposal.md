# Proposal: P8 Arrangement & Session Views (Foundation Phase)

## Intent

Layer the clip model foundation and track type specializations required for arrangement view rendering. Currently `MidiClip` and `AutomationClip` have no shared base class, and all tracks use the generic `Track` class with no type-specific behavior. This change introduces the abstract `Clip` model, `AudioClip`, type-specialized tracks (Group, VCA, Return), and the arrangement timeline with clip placement and fades.

## Scope

### In Scope
- Abstract `Clip` base class (position, length, fades, gain, looping metadata)
- `AudioClip` with file reference, waveform cache, gain envelope
- `GroupTrack` (parent/child hierarchy, fold state, summing mode)
- `VCATrack` (slave fader control, proportional volume/pan/sends)
- `ReturnTrack` (FX chain, solo-safe, fixed Master routing)
- Arrangement timeline: bar/beat grid, clip rectangles, waveform thumbnails
- Clip selection, drag-and-drop on timeline
- Fade-in/out and crossfade between overlapping clips
- Track routing updates for group summing + VCA control
- Track folder hierarchy with collapse state (model only)

### Out of Scope
- Session view (clip grid, scenes, launch), Freeze & bounce, Time stretching, Dual-view sync, Session → arrangement recording, Group folder UI (visual)

## Capabilities

### New Capabilities
- `clip-model`: Base Clip class + AudioClip (ClipID, FadeShape, waveform cache)
- `track-types`: GroupTrack, VCATrack, ReturnTrack specializations
- `arrangement-view`: Timeline rendering, clip placement, selection, fades/crossfades

### Modified Capabilities
- `mixer`: Group summing integration, VCA fader control, routing updates
- `automation-clips`: Arrangement timeline wrapper (`AutomationClipWrapper`)

## Approach

Foundation-first per exploration: (1) Clip model + AudioClip, (2) track type specializations, (3) arrangement view rendering. Track-owned clip storage with Arrangement referencing by ClipID. TypeScript Canvas for timeline rendering, C++ backends for waveform data at LOD resolution. Unify `TrackID` across modules as first task (current Arrangement struct vs mixer uint64).

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/model/clip.h/.cc` | New | Base Clip + AudioClip |
| `src/model/track.h/.cc` | Modified | TrackID alignment, routing fields |
| `src/model/track_types.h/.cc` | New | Group/VCAT/ReturnTrack |
| `src/project/arrangement.h/.cc` | Extended | Clip-at-position queries, crossfade storage |
| `src/project/project_manager.h/.cc` | Extended | Track subclass instantiation |
| `src/audio/track_processor.h/.cc` | Extended | Wire clip model for playback |
| `src/midi/midi_clip.h` | Refactored | Integrate under Clip base |
| `ui/src/views/ArrangementView/` | New | Timeline rendering |
| `openspec/specs/mixer/spec.md` | Modified | Group/VCA routing |
| `CMakeLists.txt` | Updated | New source files |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Clip model design cascades to all phases | High | Design review against 08_Track_System.md reference before implementation |
| TrackID type mismatch (struct vs uint64) | Med | Unify as first task before any clip/track work |
| Waveform render perf with large files | Med | LOD-based cache, render at visible resolution only |

## Rollback Plan

Delete new files (`src/model/clip.*`, `src/model/track_types.*`, `ui/src/views/ArrangementView/`). Revert modified files via git. No data migration needed — no persisted state uses new clip model.

## Dependencies

- Existing `MidiClip` and `AutomationClip` interfaces remain stable
- `mixer` spec update for group/VCA routing

## Success Criteria

- [ ] Clip base class compiles with AudioClip subclass; serialize/deserialize round-trips
- [ ] GroupTrack stores/retrieves children; fold state persists
- [ ] VCATrack fader proportionally adjusts slave track volumes
- [ ] Arrangement view renders clips at correct timeline positions
- [ ] Clip fades produce correct gain envelope at boundaries
- [ ] All existing tests pass; new tests cover clip model + track types
