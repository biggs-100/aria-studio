# Exploration: P10b — TypeScript Component Layer (VDOM, Component Library, Docking)

> **Status**: Complete
> **Date**: 2026-06-29
> **Scope**: TS VDOM, component library, docking system (builds on P10a GPU infrastructure)
> **Mode**: OpenSpec

---

## Current State

### P10a Completed — C++ GPU Infrastructure (Foundation)

P10a delivered the entire GPU rendering pipeline:

| Component | Status | Files |
|-----------|--------|-------|
| **GraphicsEngine** | ✅ Dawn WebGPU device, swap chain, Skia GPU backend | `src/graphics/graphics_engine.h/.cc` |
| **SkiaCanvas** | ✅ Concrete GPU-backed canvas | `src/graphics/skia_canvas.h/.cc` |
| **Widget System** | ✅ Widget, RectWidget, TextWidget, ButtonWidget, ContainerWidget | `src/graphics/widget*.h/.cc` |
| **LayoutEngine** | ✅ Two-pass measure/arrange, flexbox | `src/graphics/layout_engine.h/.cc` |
| **DirtyTracker** | ✅ Region + widget-level dirty tracking | `src/graphics/dirty_tracker.h/.cc` |
| **FFI Bridge (C++)** | ✅ JSON command parser + SkiaCanvas dispatch | `src/graphics/ffi/bridge.cc` |
| **EventDispatcher** | ✅ OS input → TS callback via global `__aria_ffi_on_event` | `src/graphics/ffi/event_dispatcher.cc` |
| **BrowserPanelGPU** | ✅ ContainerWidget tree with toolbar, tree, search, preview | `src/graphics/gpu_browser_panel.cc` |

### P10a Completed — TypeScript Layer (Skeleton)

The TypeScript layer has the FFI bridge and stub component definitions:

| File | Status | What It Does |
|------|--------|--------------|
| `ui/src/ffi/types.ts` | ✅ | `DrawCommand`, `InputEvent`, `EventHandler` type definitions matching C++ enums |
| `ui/src/ffi/commands.ts` | ✅ | Builder functions: `clear()`, `drawRect()`, `drawCircle()`, `drawText()`, `clipRect()`, `save()`, `restore()`, `flush()` |
| `ui/src/ffi/bridge.ts` | ✅ | `Bridge` class with `serialize(VNode)`, `execute()`, `render()`, `onEvent()` — walks VNode tree, emits JSON command buffer to `globalThis.__aria_ffi_execute` |
| `ui/src/index.ts` | ✅ | `UIEngine` entry point — wraps Bridge, has `initialize()`, `shutdown()`, `getBridge()`, `onEvent()` |
| `ui/src/components/index.ts` | 🟡 | `Component` interface stub + `Button`/`Slider` stubs (empty `render()`) |
| `ui/src/views/ArrangementView/` | ✅ | **Full working HTML Canvas 2D view** — NOT GPU-backed. TimeRuler, TrackList, ClipCanvas, SelectionManager, FadeOverlay. Uses `CanvasRenderingContext2D` |
| `ui/src/views/SessionView/` | ✅ | **Full working HTML Canvas 2D view** — ClipGrid, SceneStrip, CrossfaderWidget. Uses `CanvasRenderingContext2D` |

### The Bridge VNode (P10a already defines this)

```typescript
// ui/src/ffi/bridge.ts — the virtual node format P10b must extend
export interface VNode {
  type: string;
  props: Record<string, unknown>;
  children?: VNode[];
}
```

### Existing Patterns

- **Bridge.serialize()** does a recursive `switch(type)` walk over VNodes: container, rect, text, button
- Each type emits specific `DrawCommand` arrays
- **No diffing** — `render()` serializes the entire tree every time
- **No component lifecycle** — no mount/unmount/update hooks
- **No state management** — all state is manual (e.g., `this.scrollX`, `this.selectionMgr.state`)
- **No event binding** in the component tree — events go directly to view controllers
- **ArrangementView uses imperative HTML Canvas 2D** — needs porting to GPU-backed declarative components

---

## Affected Areas

### New Files to Create (TypeScript)

| Path | Purpose |
|------|---------|
| `ui/src/framework/component.ts` | Base `Component` interface, lifecycle hooks, `VNode` extended types |
| `ui/src/framework/vdom.ts` | Virtual DOM diff/patch reconciler |
| `ui/src/framework/reconciler.ts` | State management, `setState()` → re-render cycle |
| `ui/src/framework/rendering_context.ts` | Bridge wrapper for component rendering |
| `ui/src/components/Layout.ts` | Container, FlexRow, FlexColumn, Spacer, Divider |
| `ui/src/components/ScrollView.ts` | Scrollable container with clipping |
| `ui/src/components/Button.ts` | Button with hover/press states, theming |
| `ui/src/components/Slider.ts` | Horizontal/vertical slider |
| `ui/src/components/Knob.ts` | Rotary knob |
| `ui/src/components/MeterBar.ts` | Audio level meter |
| `ui/src/components/Text.ts` | Text label component |
| `ui/src/components/Fader.ts` | DAW channel fader |
| `ui/src/components/ChannelStrip.ts` | Mixer channel strip |
| `ui/src/components/Timeline.ts` | Time ruler (port from ArrangementView) |
| `ui/src/components/WaveformDisplay.ts` | Audio waveform rendering |
| `ui/src/components/DragOverlay.ts` | Floating drag preview for docking |
| `ui/src/docking/DockManager.ts` | Panel registration, open/close/move |
| `ui/src/docking/DockContainer.ts` | Split/tab/float container component |
| `ui/src/docking/DockLayout.ts` | JSON layout model + persistence |
| `ui/src/docking/DockTabBar.ts` | Tab strip with drag reorder |
| `ui/src/docking/DockSplitPanel.ts` | Horizontal/vertical splitter with resize |
| `ui/src/docking/DockFloatingWindow.ts` | Floating/undocked panel support |
| `ui/src/theme/ThemeEngine.ts` | JSON theme loader, token resolution |
| `ui/src/input/InputManager.ts` | Mouse/keyboard/touch dispatch (replace native DOM listeners) |
| `ui/src/input/FocusManager.ts` | Focus + tab navigation |

### Existing Files to Modify

| File | Why |
|------|-----|
| `ui/src/ffi/bridge.ts` | Extend `VNode` interface (add `key`, event handler props); add `patch()` method for diff-based updates |
| `ui/src/index.ts` | Expand `UIEngine` to own reconciler, component tree, docking, theme; add render loop |
| `ui/src/components/index.ts` | Replace stubs with real component library re-exports |
| `ui/src/views/ArrangementView/index.ts` | Port from HTML Canvas 2D to GPU-backed declarative components |
| `ui/src/views/SessionView/index.ts` | Port from HTML Canvas 2D to GPU-backed declarative components |
| `ui/src/views/ArrangementView/*.ts` | Each sub-view (TimeRuler, TrackList, ClipCanvas) becomes a Component |
| `ui/src/views/index.ts` | Add DockingView / AppShell root component |
| `ui/package.json` | May need `@types/dom` extensions, no heavy deps expected |

---

## Approaches

### Approach 1: Self-Contained TS VDOM + Minimal Component Library + Docking System

Build a purpose-built VDOM framework (not a React clone) that fits the DAW constraints.

**VDOM Core**:
- `Component` class with `render() → VNode`, `setState()`, lifecycle hooks
- Flat diff algorithm (type+key matching): CREATE, UPDATE, MOVE, REMOVE, REPLACE
- Patch output is a sequence of operations that either update in-memory component state OR generate DrawCommands
- No JSX — components return VNode trees directly (keeps build simple, no Babel plugin needed)

**Component Library**:
- Each component is a `Component` subclass that returns typed VNodes
- `render()` calls `Bridge.serialize()` under the hood via a RenderingContext
- Theme-aware: components read from a global Theme object
- Event handlers are props passed through the VNode tree, bound at serialize time

**Docking System**:
- JSON tree: `{type: 'split', direction: 'h', children: [{type: 'tab', panels: [...]}, ...]}`
- State-driven: layout tree is component state, re-renders trigger dock redraw
- Drag tab → drop on target → update layout tree → re-render
- Layout persistence: `JSON.stringify(layoutTree)` → save to C++ via bridge
- Default layouts as presets

```
Pros:
  - Matches the 14_UI_Framework.md spec exactly
  - Declarative, easy to reason about and maintain
  - Diff avoids full re-serialize on every frame (critical at 144 FPS)
  - Component library integrates naturally with the VDOM
  - Docking system is just another component tree — no special handling
  - No external dependencies — pure TS, minimal bundle

Cons:
  - Must build a mini-framework from scratch (time investment)
  - VDOM diff is redundant if GPU render is fast enough — could be premature optimization
  - Need to design event system integration (VNode props → Bridge serialization → C++ dispatch)
  - Porting existing views (ArrangementView, SessionView) is significant work
  - No JSX ergonomics — VNode trees are verbose in pure TS

Effort: High (~6-8 weeks for full VDOM + library + docking)
```

### Approach 2: Incremental — Extend Bridge.serialize() + No Diff + Imperative Components

Extend the existing `Bridge.serialize()` pattern with richer component types, but skip the VDOM diff. Re-render = full re-serialize. Components are higher-level imperative objects that generate VNode trees.

**Core**:
- Components are objects with `render()` that return VNode arrays
- `UIEngine.render()` calls `root.render()` → `Bridge.serialize()` → `Bridge.execute()`
- No diffing — full tree is sent to GPU every frame
- Dirty tracking follows the C++ DirtyTracker pattern: skip render unless dirty

**Component Library**:
- Same as Approach 1, but components return VNode trees directly (no lifecycle props like React)
- Components manage their own state internally

**Docking System**:
- Same as Approach 1 — docking layout is state-driven JSON tree
- Tab/split components generate VNode trees for their panels

```
Pros:
  - Fastest path to working components (weeks, not months)
  - No VDOM framework to build or debug
  - Bridge.serialize() already works — just add more node types
  - GPU rendering is so fast (0.5ms for UI widgets) that full re-serialize is fine
  - Less code, fewer bugs
  - Simpler mental model

Cons:
  - No declarative lifecycle (mount/unmount must be manual)
  - No efficient partial updates — always redraw everything
  - Loses the architectural vision from 14_UI_Framework.md
  - State management is ad-hoc per component
  - Animation triggers require manual dirty marking
  - Harder to reason about component interactions at scale (~4000 components)

Effort: Medium (~3-4 weeks for library + docking, no VDOM layer)
```

### Approach 3: Hybrid — Light VDOM for Component Tree + Full Re-Serialize for Paint

Build a lightweight VDOM for state management and lifecycle, but skip the diff/patch for paint. Instead, after state update, just re-serialize the active subtree through Bridge.

**Core**:
- Components have lifecycle and `setState()`
- `setState()` marks the component dirty and schedules a frame
- On frame: collect all dirty components, call their `render()` → VNode, serialize
- No diff against previous VNode tree — the serialization is the "diff" already
- GPU receives only the changed commands (because only dirty components re-render)

**Component Library**: Same as Approach 1
**Docking System**: Same as Approach 1

```
Pros:
  - Gets 80% of the benefit of VDOM (declarative state, lifecycle) without 80% of the complexity
  - Dirty tracking at component level avoids redundant serialization
  - Bridge.serialize() doesn't need a patch() mode — it always emits full subtrees
  - Matches the C++ DirtyTracker pattern exactly
  - Components are reusable, testable, composable

Cons:
  - Subtree-level serialization may still be redundant if only a small prop changed
  - No intelligent batching of cross-component updates
  - Still need to design the dirty tree walking + serialization logic
  - Must ensure serialization order matches C++ render order (depth-first, children after parent)

Effort: Medium (~4-5 weeks)
```

### Comparison Table

| Aspect | Approach 1 (Full VDOM) | Approach 2 (Incremental) | Approach 3 (Hybrid) |
|--------|----------------------|------------------------|-------------------|
| **VDOM diff/patch** | Full implementation | None | None (dirty tree) |
| **Component lifecycle** | Full | Minimal | Lifecycle yes, diff no |
| **State management** | Built-in (setState) | Manual per component | setState + dirty |
| **Re-render scope** | Minimal (diff) | Full tree | Dirty subtree |
| **Complexity** | High | Low | Medium |
| **Time to first working component** | 3-4 weeks | 1-2 weeks | 2-3 weeks |
| **Time to complete** | 6-8 weeks | 3-4 weeks | 4-5 weeks |
| **Porting existing views** | Moderate effort | High effort (imperative→declarative) | Moderate effort |
| **Alignment with 14_UI_Framework.md** | Perfect | Partial | Good |

---

## Recommendation

**Recommend Approach 3 (Hybrid) with the following scope for P10b:**

1. **Component Framework** (2 weeks):
   - `Component` class with `render() → VNode[]`, `setState()`, lifecycle (`onMount`, `onUnmount`, `onPropsChange`)
   - Dirty-tree scheduling: `setState()` → mark dirty → schedule frame → collect dirty → re-serialize dirty subtrees
   - `VNode` extended from bridge.ts: add `key`, `ref`, event handler props
   - `RenderingContext` wraps Bridge for component-level serialization

2. **Component Library** (2 weeks):
   - Layout primitives: Container, FlexRow, FlexColumn, Spacer, Divider
   - Input widgets: Button (with hover/press), Slider, Knob
   - Display widgets: Text, MeterBar, Timeline ruler
   - DAW widgets: Fader, ChannelStrip, WaveformDisplay (thumbnail from BrowserEngine)
   - ScrollView with clipping, momentum scroll
   - All widgets theme-aware from a ThemeEngine singleton

3. **Docking System** (1-2 weeks):
   - `DockManager` — registerPanel, openPanel, closePanel, movePanel
   - `DockSplitPanel` — horizontal/vertical split with draggable dividers
   - `DockTabBar` — tabs with drag-to-reorder, close button
   - `DockLayout` — JSON serialization: `{type, direction, size, children: [...]}` or `{type: 'tab', panels: [...]}`
   - Layout persistence via C++ bridge (save/load JSON)
   - Layout presets: Production, Mixing, Editing, Performance, Minimal

4. **Integration & Porting** (1 week):
   - `UIEngine` expanded to own the component root, reconciler, docking manager
   - Frame loop: rAF → process events → reconcile dirty components → serialize → execute
   - Port ArrangementView sub-components to GPU-backed Component classes
   - Port SessionView sub-components to GPU-backed Component classes
   - Wire event system: C++ EventDispatcher → TS `onEvent` → hit test → component dispatch

### Why Not Full VDOM Diff?

The GPU rendering pipeline is so fast (budget: 2ms for UI widgets at 144 FPS) that a full tree re-serialize of 200-1500 components is negligible. Building a VDOM diff/patch system adds **significant complexity** with **negligible performance benefit** at this scale. The dirty-tree approach achieves the same practical result (only changed parts re-render) with 1/3 the code.

The full VDOM diff (Approach 1) makes sense when you hit 4000+ components and the re-serialize cost exceeds the paint budget — but that's a future optimization. Build it when you need it.

---

## Risks

### Risk 1: Event System Integration (HIGH)
The current event flow is: C++ EventDispatcher → `__aria_ffi_on_event` global → TS handler. The ArrangementView handles events directly with DOM listeners on the canvas. P10b needs to redirect events through the component tree (hit testing → dispatch to component). **Mitigation**: The C++ Widget::hit_test() is already implemented. The TS side needs a matching tree + coordinates for hit testing. Keep the C++ hit test as the single source of truth; TS receives "which widget was hit" from C++.

### Risk 2: Porting Existing Views (MEDIUM)
ArrangementView and SessionView are fully working with HTML Canvas 2D. Porting them to GPU-backed components means losing working code temporarily. **Mitigation**: Port incrementally — keep the HTML Canvas 2D fallback. Port sub-components one at a time (TimeRuler first, then TrackList, then ClipCanvas). Feature parity tests validate each port.

### Risk 3: Docking Drag-and-Drop (MEDIUM)
Dragging tabs between panels, splitting, and floating requires hit testing on the dock container's internal layout. The drag overlay must render above all other content. **Mitigation**: Keep drag-and-drop simple initially (no floating windows). Tab reorder and split with fixed dividers. Float windows deferred to P10d.

### Risk 4: Build Pipeline (LOW)
No build changes needed beyond what P10a set up. `esbuild src/index.ts --bundle` handles all TS. The framework components are plain TypeScript classes — no JSX, no Babel, no compile step changes. **Mitigation**: None needed.

### Risk 5: Theme System Scope (LOW)
Components need theme colors/spacing but a full JSON theme engine with live reload is scope for P10d. **Mitigation**: Start with a simple `Theme` object with hardcoded tokens (colors, fonts, spacing). Extract to JSON in a later phase.

### Risk 6: State Management Growth (LOW)
As more components are added, ad-hoc state management becomes unwieldy. **Mitigation**: The `setState` + dirty-tree pattern scales well for hundreds of components. If needed, add a simple global event bus for cross-component communication.

---

## Ready for Proposal

**Yes** — P10b is well-scoped and builds directly on P10a's completed infrastructure.

The proposed scope (Approach 3) delivers:
- A working declarative component framework with lifecycle and dirty-tree updates
- A reusable component library covering all GPU-rendered UI primitives
- A functional docking system with JSON layout persistence
- Ported ArrangementView and SessionView — NO more HTML Canvas 2D

**Estimated effort**: 6-8 weeks for a single developer (full-time)
**Hard dependency**: P10a complete (confirmed)
**Recommendation to orchestrator**: Proceed to `sdd-propose` with the Hybrid approach (Approach 3). The proposal should split into 4 sub-phases: (1) Component Framework, (2) Component Library, (3) Docking System, (4) Integration & Porting.
