# Design: P10b — TypeScript Component Layer

## Technical Approach

Hybrid component layer: class-based `Component` with `setState()`, dirty-tree scheduling (no VDOM diff), serializing only changed subtrees through the FFI bridge to C++ SkiaCanvas. Docking lives in component state as JSON layout tree. Existing Canvas 2D files stay untouched behind a `UIEngine.useGpuComponents` toggle.

---

## Architecture Decisions

| Option | Tradeoff | Decision |
|--------|----------|----------|
| VDOM diff/patch | Full diff enables generic reconciliation but adds 2-5ms per frame at 1500 components. DAW UI has bounded tree depth, making diff overhead pure waste. | **Dirty-tree**: mark parent chain dirty on setState, collect dirty subtree IDs on rAF, serialize only changed paths. |
| C++ hit-test → TS event routing | Option A: TS maintains its own hit-test tree (duplicates C++ logic). Option B: C++ Widget::hit_test returns widget ID, TS maps ID to component via a registry. | **C++ source of truth** (Option B): VNode carries a `key` that maps to a C++ `WidgetID`. Event from C++ includes `widget_id`; Bridge maintains `Map<WidgetID, Component>`. |
| Docking as composable components vs. imperative manager | Imperative manager (mosaic/mosaic-like) is proven but lives outside the component tree — two state sources. | **Component-owned state**: `DockManager` is a component holding JSON layout tree in its state. `setState()` on tree mutation triggers dirty-marking down the dock subtree. |
| Port strategy: rewrite vs. parallel files | Rewrite in-place loses rollback safety. | **Parallel files + toggle flag**: `gpu-arrangement-view.ts` alongside existing `index.ts`. `UIEngine.useGpuComponents` flips between them. Port one sub-view (TrackList, ClipCanvas, etc.) at a time. |

---

## Data Flow

```
Component.setState(partial)
  ↓
Reconciler.markDirty(component)    ← walks parent chain to root
  ↓
requestAnimationFrame fires
  ↓
Reconciler.collectDirty(root)      ← Set<WidgetID> of dirty subtree roots
  ↓
Bridge.serializeDirty(root, dirtyIds)
  ↓   walk only marked branches, generate DrawCommand[]
Bridge.execute(commands)
  ↓   JSON.stringify → __aria_ffi_execute(json)
C++ Bridge::execute(json) → SkiaCanvas → GPU
  ↓
C++ EventDispatcher receives OS event
  ↓   event_dispatcher.h: Widget::hit_test(x,y) → WidgetID
Event { type, x, y, widget_id } → __aria_ffi_on_event(json)
  ↓
Bridge routes widget_id → Component → component.onEvent()
  ↓
Component may call setState() → repeat cycle
```

**Dirty-subtree serialization**: Only components on the dirty path produce draw commands. Clean branches are skipped — their last known draw commands remain on the GPU surface.

---

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `ui/src/framework/component.ts` | Create | `Component` base class: constructor, state, setState(), lifecycle (onMount/onUnmount), render() |
| `ui/src/framework/reconciler.ts` | Create | Dirty-tree scheduler: markDirty, collectDirty, schedule rAF flush |
| `ui/src/framework/rendering-context.ts` | Create | Layout context, parent size propagation |
| `ui/src/components/fader.ts` | Create | Vertical drag Fader, dB label, touch acceleration |
| `ui/src/components/meter-bar.ts` | Create | Peak/RMS bars, green/yellow/red zones, clip hold |
| `ui/src/components/channel-strip.ts` | Create | Composite: Fader + MeterBar + mute/solo + pan + label |
| `ui/src/components/timeline-ruler.ts` | Create | Bar/beat ticks, loop region, zoom-driven density |
| `ui/src/docking/types.ts` | Create | DockNode, DockLocation, DockLayout — JSON layout types |
| `ui/src/docking/dock-manager.ts` | Create | DockManager component; openPanel, closePanel, saveLayout, loadLayout |
| `ui/src/docking/dock-split-panel.ts` | Create | Split panel with draggable divider, min-size constraint |
| `ui/src/docking/dock-tab-bar.ts` | Create | Tab headers, drag reorder, close button, detach-to-float |
| `ui/src/docking/dock-floating-window.ts` | Create | Floating window container with position/size |
| `ui/src/views/ArrangementView/gpu-arrangement-view.ts` | Create | GPU-backed ArrangementView: TrackHeader, ClipRenderer, Playhead, Grid |
| `ui/src/views/SessionView/gpu-session-view.ts` | Create | GPU-backed SessionView: ClipSlotGrid, SceneButton, CrossfaderRenderer |
| `ui/src/ffi/bridge.ts` | Modify | Extend VNode with `key`, `eventHandlers`; add `serializeDirty(root, dirtyIds)` |
| `ui/src/ffi/types.ts` | Modify | `InputEvent` gains `widget_id: number`; `DrawCommand` gains `widget_id` for dirty rect tracking |
| `ui/src/index.ts` | Modify | UIEngine owns Reconciler + root Component; `useGpuComponents` toggle |
| `ui/src/components/index.ts` | Modify | Export new Component base and DAW widgets |

---

## Interfaces / Contracts

```typescript
// Component base
abstract class Component<P = {}, S = {}> {
  abstract render(): VNode;
  setState(partial: Partial<S>): void;  // merges + marks dirty
  onMount(): void;                       // override
  onUnmount(): void;                     // override
}

// Extended VNode for dirty-tree
interface VNode {
  type: string;
  key: string;                           // maps to C++ WidgetID
  props: Record<string, unknown>;
  children?: VNode[];
  eventHandlers?: Record<string, (e: InputEvent) => void>;
}

// Bridge addition
class Bridge {
  private widgetMap: Map<number, Component> = new Map();
  serializeDirty(root: VNode, dirtyIds: Set<number>): DrawCommand[];
  routeEvent(event: InputEvent): void;  // widget_id → component
}
```

---

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | Component lifecycle (constructor → onMount → setState → render → onUnmount) | Instantiate, mount, assert lifecycle hook sequence |
| Unit | setState dirty-chain marking | Assert parent chain has `isDirty` after child calls setState |
| Unit | Reconciler collectDirty coalesces multiple marks into single rAF | Fire 2 setState calls, assert single flush pass |
| Unit | Bridge serializeDirty vs full serialize produce same output for dirty subtree | Build tree, mark one branch dirty, compare commands |
| Unit | Fader value [0,1] clamp, dB formatting | Set values at boundaries and mid-range |
| Unit | MeterBar color zone threshold transitions | Assert color change at -18dB and -6dB boundaries |
| Unit | Docking split/resize, tab reorder, serialization roundtrip | Build layout, mutate, serialize, restore, assert equality |
| Integration | GPU ArrangementView renders same visual output as Canvas 2D for known input | Rasterize both to offscreen buffers, pixel-compare (deferred: P10c tooling) |

---

## Migration / Rollout

4 sequential sub-phases via the parallel-files pattern:

1. **Framework + FFI**: Component base, Reconciler, RenderingContext, VNode/InputEvent extensions, `serializeDirty` — unit tests pass, empty app runs through GPU.
2. **DAW Library**: Fader, MeterBar, ChannelStrip, TimelineRuler — rendered as isolated test components, visually match Canvas 2D reference.
3. **Docking**: DockManager, Split, TabBar, FloatWindow, JSON layout — panels open/close/resize correctly, layout round-trips through JSON.
4. **View Ports**: ArrangementView (TrackHeader, ClipRenderer, Playhead, Grid) + SessionView (ClipSlotGrid, SceneButton, Crossfader) ported sub-view by sub-view. `useGpuComponents: true` on the toggle removes all Canvas 2D calls.

No data migration. Toggle `useGpuComponents` is the rollback mechanism.

---

## Open Questions

- [ ] Touch/gesture acceleration in Fader — constant coefficients verified against hardware?
- [ ] Clip culling threshold — viewport-relative bounds check in which coordinate space (logical vs. PPQN)?
- [ ] Floating window: separate OS window (requires Electron/Tauri) or same-window overlay?
