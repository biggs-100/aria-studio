# Proposal: P10b — TypeScript Component Layer

## Intent

Build declarative TS component framework with lifecycle, dirty-tree scheduling, and DAW-specific library over P10a's Skia GPU pipeline. Port ArrangementView and SessionView from Canvas 2D to GPU — zero Canvas 2D code remaining.

## Scope

### In Scope
- Component framework: `Component` base, mount/update/unmount, `setState()`, dirty-tree scheduling
- DAW library: Fader, MeterBar, ChannelStrip, TimelineRuler (GPU-rendered via FFI)
- Full docking: DockManager, split/tab/float panels, JSON layout persistence
- Port ArrangementView + SessionView to GPU-backed components
- Extend VNode with key/event handlers, add dirty-subtree serialization

### Out of Scope
- Generic components (Slider, Knob, Button), theme system, animation engine — P10c
- VDOM diff/patch — deferred (dirty-tree sufficient at current scale)

## Capabilities

### New Capabilities
- `ts-component-framework`: Component base, lifecycle, setState(), dirty-tree scheduler, RenderingContext
- `ts-component-library`: DAW widgets — Fader, MeterBar, ChannelStrip, TimelineRuler
- `ts-docking-system`: DockManager, split/tab/float panels, JSON layout persistence
- `ts-view-arrangement`: Ported ArrangementView as GPU-backed components
- `ts-view-session`: Ported SessionView as GPU-backed components

### Modified Capabilities
- `ts-ffi-bridge`: VNode extended with key + event handlers; dirty-subtree serialization; event dispatch routes through component hit-test tree

## Approach

Hybrid (Approach 3): Component lifecycle + setState() + dirty-tree scheduling, no VDOM diff. On state change: mark dirty → schedule rAF → collect dirty subtrees → re-serialize changed subtrees only. Docking lives in component state as JSON tree, re-renders on mutation.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `ui/src/framework/` | New | Component base, reconciler |
| `ui/src/components/` | New | DAW component library |
| `ui/src/docking/` | New | Manager, split/tab/float, layouts |
| `ui/src/ffi/bridge.ts` | Modified | Extended VNode, dirty-subtree mode |
| `ui/src/index.ts` | Modified | UIEngine owns reconciler + root |
| `ui/src/views/ArrangementView/` | Modified | Sub-views → GPU Components |
| `ui/src/views/SessionView/` | Modified | Sub-views → GPU Components |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Event system: C++ → TS component hit-test | High | C++ hit-test is source of truth; TS receives widget ID |
| Docking drag-and-drop | Medium | Tab reorder + fixed dividers first; float windows deferred |
| Port regression: losing working Canvas 2D | Medium | Parallel files + toggle flag; port one sub-view at a time |

## Rollback Plan

Keep Canvas 2D files untouched. New code lives in `framework/`, `components/`, `docking/` — parallel directories. Toggle `UIEngine.useGpuComponents` flag to flip back instantly.

## Dependencies

- P10a complete (GraphicsEngine, SkiaCanvas, C++ widget tree, FFI bridge, EventDispatcher)

## Success Criteria

- [ ] All DAW components match Canvas 2D rendering visually
- [ ] Docking creates/moves/closes panels with correct geometry
- [ ] ArrangementView renders through GPU — zero Canvas 2D calls
- [ ] SessionView renders through GPU — zero Canvas 2D calls
- [ ] Dirty-tree re-renders only changed subtrees
- [ ] Layout persists across sessions and restores correctly
