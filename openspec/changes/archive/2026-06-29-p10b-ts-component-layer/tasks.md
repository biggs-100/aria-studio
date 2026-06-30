# Tasks: P10b — TypeScript Component Layer

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 1300–1700 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 Framework+FFI → PR 2 Library → PR 3 Docking → PR 4 View Ports |
| Delivery strategy | ask-on-risk |
| Chain strategy | stacked-to-main |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Framework + FFI extensions | PR 1 | base → feature branch; Component, Reconciler, VNode extensions |
| 2 | DAW Component Library | PR 2 | base → PR 1; Fader, MeterBar, ChannelStrip, TimelineRuler |
| 3 | Docking System | PR 3 | base → PR 2; DockManager, Split, TabBar, FloatWindow |
| 4 | View Ports | PR 4 | base → PR 3; ArrangementView + SessionView GPU ports |

## Phase 1: Framework + FFI

- [x] 1.1 Create `ui/src/framework/component.ts` — Component base, lifecycle, setState, dirty marking
- [x] 1.2 Create `ui/src/framework/reconciler.ts` — rAF scheduler, markDirty, collectDirty
- [x] 1.3 Create `ui/src/framework/rendering-context.ts` — layout context, parent size propagation
- [x] 1.4 Extend `ui/src/ffi/types.ts` — add `widget_id` to InputEvent and DrawCommand
- [x] 1.5 Extend `ui/src/ffi/bridge.ts` — VNode key/eventHandlers, widgetMap, serializeDirty, routeEvent
- [x] 1.6 Wire `ui/src/index.ts` — UIEngine owns Reconciler + root, useGpuComponents toggle
- [x] 1.7 Test: Component lifecycle sequence, setState dirty-chain, collectDirty coalescing, serializeDirty vs full

## Phase 2: DAW Component Library

- [x] 2.1 Create `ui/src/components/fader.ts` — vertical drag, dB label, touch acceleration
- [x] 2.2 Create `ui/src/components/meter-bar.ts` — peak/RMS bars, color zones, clip hold
- [x] 2.3 Create `ui/src/components/channel-strip.ts` — compose Fader + MeterBar + mute/solo + pan + label
- [x] 2.4 Create `ui/src/components/timeline-ruler.ts` — bar/beat ticks, loop region, zoom density
- [x] 2.5 Update `ui/src/components/index.ts` exports
- [x] 2.6 Test: Fader value clamp, MeterBar zone thresholds, ChannelStrip child composition, TimelineRuler zoom density

## Phase 3: Docking System

- [x] 3.1 Create `ui/src/docking/types.ts` — DockNode, DockLocation, DockLayout, PanelFactory types
- [x] 3.2 Create `ui/src/docking/dock-manager.ts` — layout tree, panel registry, open/close/save/load, float/reattach
- [x] 3.3 Create `ui/src/docking/dock-split-panel.ts` — calculateResize pure function, DockSplitPanel component with divider drag
- [x] 3.4 Create `ui/src/docking/dock-tab-bar.ts` — reorderTabs/isDetachEvent pure functions, DockTabBar component
- [x] 3.5 Create `ui/src/docking/dock-floating-window.ts` — createFloatingNode/isInsideWindow pure functions, DockFloatingWindow component
- [x] 3.6 Test: 44 tests across 4 test files — split resize, min-size clamp, tab reorder, detach-to-float, JSON layout round-trip, floating window creation

## Phase 4: View Ports

- [x] 4.1 Create `ui/src/views/ArrangementView/gpu-arrangement-view.ts` — TrackHeader, ClipRenderer, Playhead, Grid
- [x] 4.2 Create `ui/src/views/SessionView/gpu-session-view.ts` — ClipSlotGrid, SceneButton, CrossfaderRenderer
- [x] 4.3 Verify `useGpuComponents: true` produces zero Canvas 2D calls
- [x] 4.4 Test: clip culling, playhead reactive update, grid zoom, scene launch dispatch, crossfader position
