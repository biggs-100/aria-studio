# Exploration: P10c — Theme + Animation + Input

> **Status**: Complete
> **Date**: 2026-06-29
> **Scope**: Third and final slice of P10 GPU UI Framework (Theme System, Animation Engine, Input System)
> **Mode**: OpenSpec

---

## Current State

### What P10a Delivered (GPU Rendering Foundation)
- `GraphicsEngine` — Dawn WebGPU device init, swap chain lifecycle, adapter selection
- `SkiaCanvas` — GPU-backed 2D canvas via Skia Ganesh (`clear`, `drawRect`, `drawCircle`, `drawTextBlob`, `flush`, `save`/`restore`, `clipRect`)
- `RenderLoop` — Frame timing at 144 FPS target, profiling counters, budget monitoring
- `Widget` base class — identity (WidgetID), bounds, parent/child tree, paint pass, hit-testing
- `RectWidget`, `TextWidget`, `ButtonWidget`, `ContainerWidget` — concrete widget types
- `LayoutEngine` — Two-pass measure/arrange with flex layout
- `DirtyTracker` — Per-widget dirty rect tracking and merging
- `FFI Bridge` — JSON command buffer parsing, dispatch to SkiaCanvas, HiDPI scaling
- `EventDispatcher` — Mouse/key event dispatch with HiDPI scaling (subset of full Input System)

### What P10b Delivered (TypeScript Component Layer)
- `Component` base class — generic over Props/State, `setState()` with shallow equality, dirty propagation up the parent chain
- `Reconciler` — Dirty-tree scheduler: `requestAnimationFrame` coalescing, `collectDirty()`, `clearDirty()`, `serializeDirty()` via bridge
- `RenderingContext` — Layout context with width/height/dpr and usage tracking
- `Fader` — Vertical drag component, dB mapping, flick-to-snap
- `MeterBar` — Peak/RMS bars, green/yellow/red zones, clip hold
- `ChannelStrip` — Composite: Fader + MeterBar + mute/solo + pan + label
- `TimelineRuler` — Bar/beat ticks, loop region, zoom-driven density
- `DockManager`, `DockSplitPanel`, `DockTabBar`, `DockFloatingWindow` — Full docking system
- `gpu-arrangement-view.ts`, `gpu-session-view.ts` — GPU-backed view ports
- `UIEngine` — Orchestrates bridge + reconciler, `useGpuComponents` toggle

### What is NOT Yet Built (P10c Scope)

#### 1. Theme System — 0% Complete
Zero theme infrastructure exists. The C++ widgets hardcode colors:
- `ButtonWidget`: `Color::from_rgba8(60, 60, 60)` for normal, `Color::from_rgba8(80, 80, 80)` for hover, etc.
- `Fader`: `fillR: 0.15, fillG: 0.15, fillB: 0.18` hardcoded in VNode props
- `MeterBar`: hardcoded zone color thresholds

The `15_Theme_System.md` design doc (874 lines) defines:
- JSON theme model with inheritance (`base` field)
- Color palette: bg, text, accent, border, track, note, meter, waveform, scrollbar, selection, drag — 65+ semantic color tokens
- Typography: font families, weights, sizes, line heights, letter spacing, 9 text styles (h1–h4, body, caption, mono, label, button)
- Spacing: 4px unit scale (13 steps), panel/channel strip dimensions, border radius scale
- Component styles: Button, Slider, Knob, ChannelStrip, PianoRoll variants with hover/pressed/disabled states
- Animation timing: 5 durations, 5 easing presets, 5 transition groups
- Icon system: Material Symbols library, icon mapping for 30+ DAW actions
- Live reload: File watcher, recompile on change, targeted re-render
- Theme manager: Install/export/import, theme directory per platform
- 16 RFCs (RFC-TH-001 through RFC-TH-016) — all 🔲 Pending

Key gap: **no code implements any of this**.

#### 2. Animation Engine — 0% Complete
Zero animation infrastructure exists. No files match `animation`, `easing`, or `interpolat` in `src/graphics/` or `ui/src/`.

The original P10 exploration listed `animation_engine.h/.cc` as a planned file — it was NOT built in P10a or P10b. The `RenderLoop` provides frame timing but does not interpolate any widget properties over time.

What the spec (`15_Theme_System.md` §7) requires:
- 5 duration presets: instant (0), fast (80), normal (150), slow (300), slower (500) — ms
- 5 easing curves: ease-out, ease-out-back, ease-in, ease-out-elastic, ease-in-out-cubic
- 5 transition groups: hover, press, panel_open, page_transition, value_change
- Scroll animation: momentum decay (500ms), snap duration (200ms), ease-out-quart

#### 3. Input System — ~30% Complete (foundation built, features missing)
**Built in P10a:**
- `EventDispatcher` (C++): `dispatch_mouse_move/down/up`, `dispatch_key_down/up`, HiDPI scaling, modifier extraction
- Event types: `kMouseMove`, `kMouseDown`, `kMouseUp`, `kMouseDrag`, `kKeyDown`, `kKeyUp`
- TS bridge: `routeEvent()`, `widgetMap` (WidgetID → Component mapping), `onEvent()`/`offEvent()`
- `Widget::hit_test()` — C++ hit-testing with depth-first reverse traversal

**NOT Built (all P10c):**
- `FocusManager` — Tab-order traversal, focus/blur events, focus ring rendering, programmatic focus
- `ShortcutManager` — Keyboard shortcut registration, chord/key composition, conflict detection
- Touch input — Touch events, multi-touch gestures, touch→drag mapping
- Pen input — Pressure, tilt, barrel button
- Gesture recognition — Swipe, pinch-zoom, long-press, two-finger scroll
- Accessibility — No semantic metadata on widgets, no screen reader support, no ARIA-like attributes
- No keyboard navigation on individual components (Tab, Enter, Escape, Arrow keys)

### Build System Status
- `ARIA_FEATURE_GPU=ON` (default) — Dawn + Skia via FetchContent from source
- GPU rendering tests exist: `test_graphics_engine.cc`, `test_widget.cc`, `test_widget_container.cc`
- Render loop tests: `test_render_loop.cc`
- FFI bridge tests: `test_ffi_bridge.cc`

No theme, animation, or input tests exist.

---

## Affected Areas

### Theme System

#### New C++ files
| File | Purpose |
|------|---------|
| `src/graphics/theme_engine.h/.cc` | ThemeEngine — JSON loader, token resolution (`resolve_color(path)`, `resolve_spacing(path)`), token cache |
| `src/graphics/theme_types.h` | Theme data structures (ColorPalette, TypographySettings, SpacingSystem, ComponentStyles, AnimationTheme, IconTheme) |
| `src/graphics/widget_themed.h/.cc` | ThemedWidget — base class extension that resolves colors from active theme |
| `src/graphics/theme_manager.h/.cc` | ThemeManager — install/export/import, theme directory scanning, theme enumeration |

#### New TypeScript files
| File | Purpose |
|------|---------|
| `ui/src/theme/ThemeEngine.ts` | TS-side theme loader, token resolution, `resolveColor(path)` |
| `ui/src/theme/ThemeLiveReload.ts` | File watcher for live theme reload |
| `ui/src/theme/themes/aria-dark.json` | Built-in dark theme |
| `ui/src/theme/themes/aria-light.json` | Built-in light theme |
| `ui/src/theme/themes/aria-high-contrast.json` | Built-in high contrast theme |

#### Existing files to modify
| File | Change |
|------|--------|
| `src/graphics/widget_button.h/.cc` | Replace hardcoded colors with theme token lookups |
| `src/graphics/widget_rect.h/.cc` | Optional theme token binding |
| `src/graphics/widget_text.h/.cc` | Font resolution from theme typography |
| `ui/src/components/fader.ts` | Replace hardcoded fillR/fillG/fillB with theme tokens |
| `ui/src/components/meter-bar.ts` | Zone colors from theme |
| `ui/src/components/channel-strip.ts` | Track colors from theme |
| `ui/src/components/timeline-ruler.ts` | Grid/beat/bar colors from theme |
| `ui/src/index.ts` | Integrate ThemeEngine into UIEngine |
| `src/kernel/application.cc` | Initialize ThemeEngine on startup |
| `CMakeLists.txt` | Add theme source files |

### Animation Engine

#### New C++ files
| File | Purpose |
|------|---------|
| `src/graphics/animation_engine.h/.cc` | AnimationEngine — GPU-side interpolation, easing functions, active animation list |
| `src/graphics/animation_types.h` | AnimationState, EasingCurve enum, keyframe definitions |
| `src/graphics/easing.h/.cc` | 30+ easing curve implementations (from spec: ease-out, ease-out-back, ease-in, ease-out-elastic, ease-in-out-cubic, ease-out-quart; plus standard CSS easings) |

#### New TypeScript files
| File | Purpose |
|------|---------|
| `ui/src/animation/Easing.ts` | Easing functions mirroring C++ implementations |
| `ui/src/animation/Animator.ts` | TS-side animation composer, property interpolation, transition system |

#### Existing files to modify
| File | Change |
|------|--------|
| `src/graphics/render_loop.cc` | Tick AnimationEngine each frame before paint pass |
| `src/graphics/widget.h` | Add `animate_property()` hook or property animation registry |
| `src/graphics/skia_canvas.h/.cc` | May need animation-friendly draw helpers (opacity, transform interpolation) |
| `ui/src/framework/component.ts` | Optional animation lifecycle hooks (onAnimate) |
| `ui/src/framework/reconciler.ts` | Animation frame scheduling awareness |

### Input System (Extension)

#### New C++ files
| File | Purpose |
|------|---------|
| `src/graphics/focus_manager.h/.cc` | FocusManager — tab-order, focus/blur events, focus ring rendering |
| `src/graphics/shortcut_manager.h/.cc` | ShortcutManager — key chord registration, conflict detection, platform key mapping |
| `src/graphics/input_types.h` | Extended input types: TouchEvent, PenEvent, GestureEvent |

#### New TypeScript files
| File | Purpose |
|------|---------|
| `ui/src/input/InputManager.ts` | Mouse/key/touch/pen dispatch, gesture recognition |
| `ui/src/input/ShortcutManager.ts` | Keyboard shortcut registration, chord matching |
| `ui/src/input/FocusManager.ts` | Focus & tab navigation, focus trap, programmatic focus |

#### Existing files to modify
| File | Change |
|------|--------|
| `src/graphics/ffi/event_dispatcher.h/.cc` | Extend with touch/pen dispatch methods, gesture routing |
| `src/graphics/ffi/event_dispatcher.h` | Add touch/pen event types to EventType enum |
| `src/graphics/widget.h` | Add `on_focus()`/`on_blur()`, `tab_order`, `accessible_name`, `accessible_role` |
| `ui/src/ffi/types.ts` | Add TouchEvent, PenEvent types |
| `ui/src/framework/component.ts` | Focus lifecycle integration, tab order |
| `ui/src/index.ts` | Register InputManager, ShortcutManager, FocusManager |

---

## Approaches

### Approach 1: Three Parallel Sub-Changes (Recommended)

Split P10c into three independent sub-changes that can be developed and delivered concurrently:

**Sub-change A: P10c-theme** — Theme system
1. Define C++ theme types (`theme_types.h`) — mirror 15_Theme_System.md JSON schema
2. Implement `ThemeEngine` — JSON file loader, token resolution with `{path}` reference syntax, token cache
3. `WidgetThemed` base — widget subclass that resolves colors/fonts/spacing from active theme
4. Migrate existing widgets (`ButtonWidget`, `RectWidget`, `TextWidget`) to use theme tokens
5. TS `ThemeEngine` — same token resolution on TS side for component VNode props
6. Built-in themes: aria-dark.json, aria-light.json, aria-high-contrast.json
7. Live reload via existing file-watcher infrastructure (P9 file-watcher spec)
8. Theme manager: install/export/import

**Sub-change B: P10c-animation** — Animation engine
1. Easing curves (`easing.h/.cc`) — 30+ implementations (CSS standard + spec extras)
2. `AnimationEngine` (C++) — manages active animations, per-frame interpolation tick, fires completion callbacks
3. Property interpolation: float, Vec2, Color, Rect
4. Transition system: declarative transitions on property changes
5. RenderLoop integration: tick animations before paint pass
6. TS `Animator` — TS-side animation composer for VNode prop interpolation

**Sub-change C: P10c-input** — Input system extension
1. `FocusManager` — focus ring rendering, tab-order traversal (Z-order), focus/blur events, programmatic focus `setFocus()`
2. `ShortcutManager` — key chord registration (`Ctrl+S`), conflict detection, platform-specific key mapping
3. Touch/pen dispatch in `EventDispatcher` — touch events, pressure, tilt, gesture recognition (swipe, pinch-zoom, long-press)
4. Accessibility metadata: `accessible_name`, `accessible_role`, `accessible_description` on Widget base
5. Keyboard navigation: Tab/Shift+Tab, Enter, Escape, Arrow keys on interactive widgets
6. Widget focus state rendering (focus ring)

**Pros**:
- Three independent parallel tracks (different engineers)
- Theme and Animation share no dependencies (fully parallel)
- Input depends on EventDispatcher (already built) — minimal coupling
- Each sub-change is independently deliverable and testable
- Each fits within 400-line PR review budget
- Theme unblocks the most visible problem (hardcoded colors everywhere)

**Cons**:
- Requires coordinating three parallel specs and task plans
- Animation engine needs RenderLoop integration (minor coupling)
- FocusManager needs Widget hit-test tree (already built) and EventDispatcher (already built)

**Effort**: Medium (estimated 3-4 weeks total across all three)

### Approach 2: Sequential Monolithic Change

Build all three systems in one sequential change: Theme → Animation → Input.

**Pros**:
- Single spec, design, task plan
- Easier to see cross-cutting dependencies
- Single PR (though oversized — would need chained PRs)

**Cons**:
- Single point of failure (one big change)
- Theme has no dependency on Animation/Input — unnecessarily blocked
- 1200+ lines estimated — far exceeds 400-line review budget
- Harder to parallelize

**Effort**: High (4-6 weeks)

### Approach 3: Hybrid — Theme + Input First, Animation Later

Theme (highest visibility) and Input (needs focus management for accessibility/completeness) first. Animation deferred to P11.

**Pros**:
- Fastest path to eliminating hardcoded colors
- Input system becomes feature-complete for P10 UI
- Animation is optional polish — not blocking any P10 functionality
- De-risks: theme has the most complex integration (all widgets)

**Cons**:
- Animation deferred yet again (P10 exploration also deferred it)
- No transition system means no smooth hover/panel transitions
- Theme live reload without animation transitions feels jarring

**Effort**: Medium (Theme + Input: 2-3 weeks)

---

## Recommendation

**Approach 1: Three Parallel Sub-Changes**.

This is the most pragmatic path:

1. **Theme is the highest priority** — every widget currently hardcodes colors. Building the theme system makes the entire UI customizable and eliminates technical debt from P10a/P10b. It also directly integrates with the existing Component framework via token resolution in VNode props.

2. **Animation is the most contained** — it's purely additive (easing curves + interpolation engine), touches no existing widget code, and integrates at the RenderLoop boundary. Low risk, high polish value.

3. **Input extension has the most coupling** — FocusManager depends on Widget tree (done) and EventDispatcher (done). ShortcutManager needs keyboard events (done). Touch/pen extends the existing EventDispatcher. Each extension is well-contained.

**Integration order**: All three can start in parallel. Theme will ship first since it's the most visible. Animation ships second (no dependencies on other sub-changes). Input ships third (minor integration with theme's Widget metadata for accessibility labeling).

**Recommendation to orchestrator**: Create three SDD changes rather than one:
- `p10c-theme-system` — ThemeEngine, token resolution, widget theming, live reload
- `p10c-animation-engine` — Easing curves, interpolation engine, transition system
- `p10c-input-system` — FocusManager, ShortcutManager, touch/pen, accessibility

Each independently planned, spec'd, designed, and reviewed.

---

## Risks

### Risk 1: Theme JSON Reference Resolution ({path} syntax) — MEDIUM
The spec requires `{colors.bg.primary}` → `#1A1A1A` resolution. This is a non-trivial parser:
- Must support nested path resolution (dot-separated)
- Must handle circular references
- Must handle type coercion (string → number for spacing values)
- **Mitigation**: Implement as a simple recursive descent token resolver with cycle detection and 10-level max depth

### Risk 2: Animation Engine Integration with Dirty Tracker — MEDIUM
Animating widget properties (opacity, transform) must correctly mark the widget dirty each frame. A per-frame animation update that marks the whole tree dirty would negate the benefit of dirty tracking.
- **Mitigation**: AnimationEngine returns `Set<WidgetID>` of changed widgets; only those are marked dirty after each animation tick

### Risk 3: Focus Manager Tab Order with Dynamic Widget Trees — LOW
P10b's Component tree is dynamic (mount/unmount). Tab order must be deterministic and stable across tree changes.
- **Mitigation**: Default tab order is widget creation order (insertion order into parent). Explicit `tab_index` overrides. Removed widgets collapse the order.

### Risk 4: Shortcut Manager Conflict Detection — LOW
Multiple components may register the same shortcut (e.g., both ArrangementView and PianoRoll want `Delete`).
- **Mitigation**: Priority-based resolution (focused component gets first chance, then propagate). Explicit conflict warning on registration.

### Risk 5: No Direct Dependency on External Libraries — LOW
All three sub-systems are pure C++/TypeScript with no new external dependencies:
- Theme: JSON parsing via existing nlohmann/json
- Animation: Pure math (no need for physics engine)
- Input: Pure platform abstraction (no new OS dependencies)

---

## Ready for Proposal

**Yes** — but as three separate SDD changes, not one monolithic change.

The orchestrator should propose three parallel SDD workflows:
1. `p10c-theme-system` — highest priority, unblocks hardcoded colors everywhere
2. `p10c-animation-engine` — medium priority, pure additive, no existing code changes
3. `p10c-input-system` — medium priority, extends existing EventDispatcher foundation

Each change is independently specifiable (no cross-change requirements), independently implementable, and independently verifiable. The 400-line review budget is easily maintained per change (estimate: 250-350 lines each).
