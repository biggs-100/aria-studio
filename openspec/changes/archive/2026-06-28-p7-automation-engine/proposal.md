# Proposal: P7 — Automation Engine

## Intent

Add a complete automation and modulation system to ARIA DAW. P1 (Audio Engine) provides parameter targets; P3 (Timeline) provides the PPQN clock. P7 bridges them — enabling parameter automation recording, playback, and deep modulation with nested source→target routing.

## Scope

### In Scope
- Automation clips (point-based, step, LFO) with 10 interpolation types
- Modulation sources (LFO, envelope follower, step sequencer, macro, random, note property)
- Modulation matrix with nested modulation (8 levels max depth)
- Automation lanes (one per parameter, arm/disarm, record)
- Real-time recording with Douglas-Peucker point reduction
- Envelope system (ADSR/AHDSR/DADSR, gated evaluation)
- Lock-free double-buffered parameter cache for audio thread reads

### Out of Scope
- Bezier curve editing in the UI (deferred — clip data model supports it)
- Piano roll automation overlay (deferred to P4/P10)
- Per-note automation expressions (deferred)
- Sidechain audio input for envelope follower (requires P5 routing)
- Scripted/Lua modulation sources (deferred to P11)
- Automation presets and templates

## Capabilities

### New Capabilities
- `automation-clips`: Point-based standard/step/LFO clips, 10 interpolation modes
- `modulation-sources`: LFO, envelope follower, step sequencer, macro, random, note property
- `modulation-matrix`: Source→target DAG, nested modulation (8 levels), bipolar/unipolar
- `automation-lanes`: Lane per parameter, arm/disarm, visibility, recording
- `automation-recording`: Real-time capture with smoothing, Douglas-Peucker reduction
- `envelope-system`: ADSR/AHDSR/DADSR envelope shapes, gated evaluation with curve per segment
- `real-time-param-cache`: Lock-free double-buffered, atomic reads on audio thread

### Modified Capabilities
None — first automation phase, no existing specs to modify.

## Approach

Layer architecture from bottom up: (1) data model (`AutomationPoint`, `ParameterBinding`), (2) clip types + envelope system, (3) modulation sources + matrix, (4) automation lanes + recording, (5) lock-free parameter cache connecting to `ParameterID` targets from P1. Existing `src/automation/automation_engine.{h,cc}` skeleton provides the entry point.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/automation/` | Modified | Stub → full engine implementation |
| `src/audio/parameter.h` | Modified | Add `ParameterBinding` + `ParameterInfo` types |
| `src/automation/clip.h` | New | `AutomationClip`, `StepAutomationClip`, `LFOAutomationClip` |
| `src/automation/envelope.h` | New | `EnvelopeClip` with ADSR/AHDSR/DADSR |
| `src/automation/modulation.h` | New | `ModulationSource`, `ModulationMatrix`, `ModulationStack` |
| `src/automation/lane.h` | New | `AutomationLane` with arm/record |
| `src/automation/recorder.h` | New | `AutomationRecorder` with Douglas-Peucker |
| `src/automation/cache.h` | New | `ParameterCache` double-buffered |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Lock-free cache race condition on edge cases | Low | Double-buffer with atomic index swap, test under heavy load |
| Nested modulation evaluation exceeds per-buffer budget | Med | Depth limit (8), early exit on bypassed links |
| Douglas-Peucker removes important micro-timing | Low | User-configurable tolerance, adaptive mode |

## Rollback Plan

Revert all files under `src/automation/` to the init-only skeleton. Remove all new headers (clip.h, envelope.h, modulation.h, lane.h, recorder.h, cache.h). Revert `src/audio/parameter.h` changes. This is isolated from P1/P3 — no cascade.

## Dependencies

- P1 (Audio Engine): `ParameterID` for target binding, `ParameterCache` consumer path
- P3 (Timeline): PPQN sample position for clip evaluation

## Success Criteria

- [ ] Automation points evaluate at < 1µs per clip (100 points)
- [ ] Modulation stack (8 sources) evaluates in < 5µs per parameter per buffer
- [ ] Full matrix evaluation (100 targets) completes in < 50µs per audio callback
- [ ] Recorded 5s knob turn reduces from ~1800 → 20-50 points (0.01 tolerance)
- [ ] Nested modulation chain at max depth (8 levels) produces correct output
