# Design: P7 — Automation Engine

## Technical Approach

Bottom-up: (1) data model → (2) clip types + envelope → (3) modulation sources + DAG matrix → (4) lanes + recording → (5) lock-free cache bridging to P1 audio thread. Each depends only on layers below.

## Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Point storage | `std::vector<AutomationPoint>` sorted by PPQN | Binary search meets < 1µs budget; cache-friendly for < 100 pts/clip |
| Point evaluation | Binary search + function table | Function table indexed by `InterpolationType` avoids vtable on hot path |
| Clip inheritance | Base + virtual `evaluate()` | Step/LFO clips override cleanly |
| Modulation graph | `unordered_map<ParameterID, vector<ModulationEntry>>` | Sparse connections; O(1) lookup |
| Nested modulation | Recursive with depth counter | Simpler than flat expansion; 8-level cap |
| Cycle detection | Track visited source IDs | Rare event (main thread); cheaper than SCC |
| Parameter cache | Double buffer + `atomic<uint32_t>` index | Consistent snapshot per callback |
| ParameterID | `uint64_t` typedef (0 = invalid) | Matches existing ID patterns |
| D-P algorithm | In `AutomationRecorder::reduce_points()` | Co-located with recording |
| Envelope eval | Gated state machine | Standard DAW model |

## Data Flow

```
 Audio Thread                          Control Thread
 ────────────                         ──────────────
                                                         
 AutomationClip.evaluate(ppqn)       User edits points
        │                                   │
        ▼                                   ▼
 ModulationStack.evaluate(base, ctx)  ModulationMatrix.connect()
        │                                   │
        ▼                                   ▼
 ParameterCache.update_value(id, v)   ParameterCache.swap_buffers()
        │                                   │
        ▼                                   ▼
 AudioEngine reads cache per          AutomationRecorder.record_value()
 callback (consistent snapshot)        ← audio thread writes ring buffer
                                             │
                                             ▼
                                       reduce_points() (Douglas-Peucker)
                                             │
                                             ▼
                                       AutomationClip (recorded result)

 Nested modulation:
 LFO1 ──rate──▶ LFO2 ──amount──▶ Filter Cutoff
                  ▲
             EnvFollower (nested at depth 2)
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/automation/automation_engine.h` | Modify | Expand stub → full `AutomationEngine` with clip/source/matrix/lane/cache API |
| `src/automation/automation_engine.cc` | Modify | Implement all engine methods, `process_audio_thread()` |
| `src/automation/automation_types.h` | Create | Core types: `AutomationPoint`, `InterpolationType`, `ParameterID` |
| `src/automation/interpolation.h` | Create | 10 interpolation functions + lookup table |
| `src/automation/clip.h` | Create | `AutomationClip`, `StepAutomationClip`, `LFOAutomationClip` |
| `src/automation/envelope.h` | Create | `EnvelopeClip` ADSR/AHDSR/DADSR |
| `src/automation/modulation_source.h` | Create | 6 modulation source types |
| `src/automation/modulation_matrix.h` | Create | `ModulationStack`, `ModulationMatrix` + cycle detection |
| `src/automation/lane.h` | Create | `AutomationLane` arm/bypass/visibility |
| `src/automation/recorder.h` | Create | Ring buffer recorder + Douglas-Peucker |
| `src/automation/cache.h` | Create | Lock-free double-buffered `ParameterCache` |
| `CMakeLists.txt` | Modify | Add `AUTOMATION_SOURCES` |
| `tests/CMakeLists.txt` | Modify | Add `aria_automation_tests` |
| `tests/unit/test_automation_points.cc` | Create | Point evaluation + interpolation tests |
| `tests/unit/test_automation_clips.cc` | Create | Clip type tests |
| `tests/unit/test_modulation.cc` | Create | Matrix, stack, depth tests |
| `tests/unit/test_recorder.cc` | Create | Recording + D-P reduction tests |

## Interfaces / Contracts

```cpp
// automation_types.h
using ParameterID = uint64_t;      // 0 = invalid
using AutomationClipID = uint64_t;
struct AutomationPoint { uint64_t ppqn; double value; InterpolationType interp; BezierControl bezier; };

// cache.h — lock-free, main thread writes inactive, audio thread reads active
class ParameterCache {
public:
    void update_value(ParameterID, double);
    double read_value(ParameterID) const;
    void swap_buffers();  // atomic toggle
private:
    Buffer buffers_[2];
    std::atomic<uint32_t> active_buffer_{0};
};

// modulation_matrix.h — DAG with cycle detection on connect
class ModulationMatrix {
public:
    ModulationEntryID connect(ModulationSource*, ParameterID, double, ModulationPolarity);
    double apply_modulations(ParameterID, double, uint64_t, const ModulationContext&) const;
};

// recorder.h — audio thread writes ring buffer, main thread reduces
class AutomationRecorder {
public:
    void record_value(uint64_t, double);  // audio thread
    void start_recording(AutomationLane*);
    void stop_recording();
    std::vector<AutomationPoint> reduce_points(const std::vector<AutomationPoint>&, double tol);
};
```

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Unit | Point evaluation (10 interpolation types) | Known input/output pairs, boundary conditions (t=0, t=1) |
| Unit | Clip evaluate at various PPQN | Pre/post point changes, loop on/off |
| Unit | Modulation stack with N sources | Ordering tests, bypass, nesting depth |
| Unit | D-P reduction accuracy | 1800-point synthetic curve → verify reduced to 20-50 pts |
| Unit | Parameter cache lock-free semantics | Thread sanitizer build, concurrent read/write stress |
| Integration | Full `process_audio_thread()` pipeline | Mock sources + cache → verify consistent per-callback snapshot |
| Performance | Point eval latency | < 1µs per 100-point clip |
| Performance | Full matrix eval | < 50µs per 100-target callback |

## Migration / Rollout

No migration required. Greenfield module with a stub entry point. Audio engine integration (P1) will wire `AudioEngine::process()` → `AutomationEngine::process_audio_thread()` in a separate phase.

## Open Questions

- [ ] `ParameterID` namespace/ownership: P1 defines per-plugin params as `uint32_t` or compound IDs. Confirm P7 uses `uint64_t` and P1 maps accordingly.
- [ ] Ring buffer size for recording: 44100 samples = 1s at 44.1kHz. Sufficient for most real-time captures, but confirm overflow strategy (blocking vs. lossy wrap).
