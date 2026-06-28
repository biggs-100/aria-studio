# Tasks: P7 — Automation Engine

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~2,500–3,000 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 → PR 2 → PR 3 → PR 4 |
| Delivery strategy | ask-on-risk |
| Chain strategy | stacked-to-main |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Data model + interpolation types + clip types + envelope | PR 1 | foundation layer; no deps below |
| 2 | Modulation sources + modulation matrix | PR 2 | depends on types from PR 1 |
| 3 | Lanes + recorder + D-P + parameter cache | PR 3 | depends on clip/eval from PR 1 |
| 4 | Engine wiring + build system + full tests | PR 4 | depends on all above |

## Phase 1: Data Model + Clips

- [x] 1.1 Create `src/automation/automation_types.h` — `ParameterID`, `AutomationPoint`, `InterpolationType`, `BezierControl`
- [x] 1.2 Create `src/automation/interpolation.h` — 10 eval functions + function table + `evaluate_segment()`
- [x] 1.3 Create `src/automation/automation_clip.h`/`.cc` — `AutomationClip` base (points, evaluate, bulk ops, serialization stubs)
- [x] 1.4 Create `src/automation/step_clip.h`/`.cc` — `StepAutomationClip` (step_count, step_values, smooth, swing, evaluate override)
- [x] 1.5 Create `src/automation/lfo_clip.h`/`.cc` — `LFOAutomationClip` (waveform, rate_hz, rate_sync, phase, bipolar, evaluate override)
- [x] 1.6 Create `src/automation/envelope.h`/`.cc` — `EnvelopeClip` (ADSR/AHDSR/DADSR, per-segment curves, `evaluate_gated()`)

## Phase 2: Modulation System

- [x] 2.1 Create `src/automation/modulation_source.h/cc` — `ModulationSource` base + `LFOSource` + `EnvelopeFollowerSource`
- [x] 2.2 Create `src/automation/step_sequencer_source.h/cc` + `macros.h/cc` + `random_source.h/cc` + `note_property_source.h/cc`
- [x] 2.3 Create `src/automation/modulation_types.h` — `ModulationEntry`, `ModulationStack`, `ModulationPolarity`
- [x] 2.4 Create `src/automation/modulation_matrix.h/cc` — `ModulationMatrix` (connect/disconnect, source→target DAG, cycle detection)
- [x] 2.5 Create `src/automation/nested_modulation.h/cc` — `NestedModulation` (chain links, 8-level depth enforcement, recursive eval)

## Phase 3: Lanes + Recording + Cache

- [x] 3.1 Create `src/automation/lane.h`/`lane.cc` — `AutomationLane` (target, clip binding, arm/bypass, visibility, height clamp)
- [x] 3.2 Create `src/automation/recorder.h`/`recorder.cc` — `AutomationRecorder` (ring buffer, smoothing, start/stop, `record_value()`)
- [x] 3.3 Add `AutomationRecorder::reduce_points()` — Douglas-Peucker with configurable tolerance and 3 reduction modes
- [x] 3.4 Create `src/automation/cache.h`/`cache.cc` — `ParameterCache` (double buffer, atomic swap, `update_value()`, `read_value()`, `update_range()`)

## Phase 4: Engine Wiring + Build + Tests

- [x] 4.1 Modify `src/automation/automation_engine.h` — expand stub with clip/source/matrix/lane/recorder/cache API
- [x] 4.2 Modify `src/automation/automation_engine.cc` — implement engine methods + `process_audio_thread()` pipeline
- [x] 4.3 Modify `CMakeLists.txt` — add `AUTOMATION_SOURCES` to aria_core
- [x] 4.4 Modify `tests/CMakeLists.txt` — add `aria_automation_tests` executable
- [x] 4.5 Create `tests/unit/test_automation_points.cc` — 10 interpolation eval, boundary, bezier, D-P reduction accuracy
- [x] 4.6 Create `tests/unit/test_automation_clips.cc` — clip eval, step clip, LFO waveforms, envelope gated states
- [x] 4.7 Create `tests/unit/test_modulation.cc` — matrix connect/disconnect, stack ordering, nesting depth, cycle rejection, bypass
- [x] 4.8 Create `tests/unit/test_recorder.cc` — ring buffer wrap, smoothing, D-P 1800→20-50 reduction, ReduceOnStop vs Adaptive
