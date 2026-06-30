# Exploration: Phase 10 — GPU UI Framework

> **Status**: Complete
> **Date**: 2026-06-29
> **Scope**: Full P10 architecture exploration
> **Mode**: OpenSpec

---

## Current State

### Graphics Engine (src/graphics/) — EMPTY

The `src/graphics/` directory exists but is **completely empty** — zero files. All graphics engine code must be written from scratch.

Four detailed design documents exist but have **zero C++ implementation**:
- **17_Graphics.md** — Widget primitives, rendering passes, dirty tracking, layout, animations, renderers (848 lines of API spec)
- **14_UI_Framework.md** — TypeScript component model, VDOM diff/patch, dock system, event system, input system (945 lines)
- **15_Theme_System.md** — JSON theme format, color palette, typography, spacing, components, live reload (874 lines)
- **16_Rendering.md** — WebGPU abstraction, Skia canvas, render graph, shaders, textures, HiDPI, post-processing (940 lines)

### SkiaCanvas — Forward-Declared Everywhere, Implemented Nowhere

Multiple modules already depend on `SkiaCanvas` as an **opaque forward declaration**:

| File | Usage |
|---|---|
| `src/pianoroll/piano_roll_renderer.h/.cc` | Full render pipeline with 10+ render methods, ALL stubbed |
| `src/pianoroll/piano_roll_canvas.h/.cc` | `render(SkiaCanvas*, Rect)` — one live call, body stubbed |
| `src/pianoroll/piano_keyboard.h/.cc` | `render(SkiaCanvas*, Rect, ...)` — stubbed |
| `src/pianoroll/expression_lane.h/.cc` | `render(SkiaCanvas*, Rect, ...)` — stubbed |
| `src/pianoroll/velocity_lane.h/.cc` | `render(SkiaCanvas*, Rect, ...)` — stubbed |
| `src/mixer/mixer_ui.h/.cc` | Mixer console with channel strip, fader, meter, pan, buttons — ALL stubbed |

Common pattern: `(void)canvas;` + `// TODO: implement once SkiaCanvas is concrete`

### ImGui Browser (P9) — Provisional

The browser panel (`src/browser/browser_panel.h/.cc`) uses ImGui guarded by `ARIA_FEATURE_IMGUI`. Comment everywhere: "Provisional — P10 replaces with GPU view."

- `aria_imgui` is an INTERFACE library requiring `IMGUI_DIR`
- No vendored ImGui in `vendor/`
- ArrangementDropTarget also ImGui-based with "P10 replaces" comment

### TypeScript Layer — Basic Skeleton

`ui/` directory has:
- **package.json**: esbuild, TypeScript 5.4, vitest, eslint
- **tsconfig.json**: ES2022 target, strict mode
- **src/index.ts**: `UIEngine` class (stub with `initialize()` and `shutdown()`)
- **src/components/index.ts**: `Component` interface + `Button`/`Slider` stubs
- **src/views/ArrangementView/**: Working HTML Canvas 2D arrangement with TimeRuler, TrackList, ClipCanvas, SelectionManager, FadeOverlay — uses `CanvasRenderingContext2D`, NOT GPU
- **src/views/SessionView/**: Exists

The ArrangementView currently uses **HTML Canvas 2D** (`ctx.getContext('2d')`) — this will need to be replaced with GPU-backed rendering in P10.

### Build System — GPU Dependencies Declared but Unresolved

`cmake/FindDependencies.cmake`:
- `ARIA_FEATURE_GPU` = ON (default)
- **Skia**: `find_package(Skia QUIET)` + `SKIA_DIR` fallback — **Not found** on system
- **WebGPU**: `find_package(webgpu QUIET)` + `WEBGPU_DIR` fallback — **Not found** on system
- `${SKIA_LIBRARIES}` and `${WEBGPU_LIBRARIES}` already linked into `aria_core`
- ImGui: requires `IMGUI_DIR` — not vendored, not found

`CMakeLists.txt` already references:
- TypeScript build target (`ts_ui_build`) via pnpm
- Links `aria_ui_bundle` (INTERFACE) into `aria_core`

### Existing Architecture Patterns

- **ServiceLocator**: Type-keyed registration (`typeid(T).name()`). Application owns it at `app.services()`. Pattern: `register_service<T>(unique_ptr<T>)` / `get_service<T>()`
- **EventBus**: Typed publish/subscribe with sync + queued dispatch. Method: `publish(event)`, `queue(event)`, `dispatch_pending()`
- **Application**: Singleton via `Application::instance()`. Run loop: `dispatch_pending()` → `process_pending()` — currently has NO render loop, NO frame timing
- **Kernel**: `service_locator.h/cc`, `event_bus.h/cc`, `command_queue.h/cc`, `undo_stack.h/cc`, `application.h/cc`, `crash_handler.h/cc`

### Rust Workspace

4 crates: `aria-database`, `aria-filesystem`, `aria-networking`, `aria-bridge`. **No GPU/WebGPU crates**. C API via `rust/include/aria_rust.h`.

### Vendor Directory

`vendor/` has: clap, lua, sqlite, vst3sdk — **no Skia, no WebGPU, no imgui**.

---

## Affected Areas

### New Files to Create (Graphics Engine — C++)

| Path | Purpose |
|---|---|
| `src/graphics/gpu_device.h/.cc` | WebGPU device init, buffer/texture/shader factories |
| `src/graphics/swap_chain.h/.cc` | Swap chain management, present, resize |
| `src/graphics/skia_canvas.h/.cc` | Concrete SkiaCanvas — the one everyone forward-declares |
| `src/graphics/render_graph.h/.cc` | Render pass management, barrier insertion |
| `src/graphics/command_buffer.h/.cc` | GPU command recording |
| `src/graphics/shader_manager.h/.cc` | WGSL shader compilation, caching |
| `src/graphics/texture_manager.h/.cc` | Texture loading, atlas packing, cache |
| `src/graphics/glyph_atlas.h/.cc` | Glyph atlas for text rendering |
| `src/graphics/icon_atlas.h/.cc` | SVG icon atlas |
| `src/graphics/font_manager.h/.cc` | Font loading, SkTypeface management |
| `src/graphics/post_processor.h/.cc` | Blur, glow, drop shadow compute shaders |
| `src/graphics/hidpi_manager.h/.cc` | DPI scaling, logical→physical |

### New Files to Create (Widget System — C++)

| Path | Purpose |
|---|---|
| `src/graphics/widget.h/.cc` | Base widget class |
| `src/graphics/widget_rect.h/.cc` | RectWidget |
| `src/graphics/widget_text.h/.cc` | TextWidget |
| `src/graphics/widget_button.h/.cc` | ButtonWidget |
| `src/graphics/widget_slider.h/.cc` | SliderWidget |
| `src/graphics/widget_knob.h/.cc` | KnobWidget |
| `src/graphics/widget_fader.h/.cc` | FaderWidget |
| `src/graphics/widget_meter.h/.cc` | MeterWidget |
| `src/graphics/widget_waveform.h/.cc` | WaveformWidget |
| `src/graphics/widget_scroll.h/.cc` | ScrollWidget |
| `src/graphics/widget_dock.h/.cc` | DockWidget |
| `src/graphics/layout_engine.h/.cc` | Flexbox layout computation |
| `src/graphics/animation_engine.h/.cc` | GPU-side animation interpolation |
| `src/graphics/dirty_tracker.h/.cc` | Dirty region tracking |
| `src/graphics/render_scheduler.h/.cc` | Frame scheduling, timing |

### New Files to Create (TypeScript UI — TS)

| Path | Purpose |
|---|---|
| `ui/src/framework/component.ts` | Base component, VNode types |
| `ui/src/framework/vdom.ts` | Virtual DOM diff/patch algorithm |
| `ui/src/framework/reconciler.ts` | State management, reactivity |
| `ui/src/framework/layout.ts` | Flexbox layout engine (TS side) |
| `ui/src/ffi/bridge.ts` | TypeScript→C++ FFI bridge |
| `ui/src/components/Button.ts` | GPU Button component |
| `ui/src/components/Slider.ts` | GPU Slider component |
| `ui/src/components/Knob.ts` | GPU Knob component |
| `ui/src/components/MeterBar.ts` | GPU MeterBar component |
| `ui/src/components/WaveformDisplay.ts` | GPU Waveform display |
| `ui/src/components/ScrollView.ts` | GPU ScrollView |
| `ui/src/components/Dialog.ts` | Dialog/Modal system |
| `ui/src/components/ChannelStrip.ts` | DAW-specific strip |
| `ui/src/components/Fader.ts` | DAW-specific fader |
| `ui/src/components/Timeline.ts` | Timeline ruler |
| `ui/src/components/PianoRollCanvas.ts` | Piano roll overlay |
| `ui/src/input/InputManager.ts` | Mouse/key/touch/pen dispatch |
| `ui/src/input/ShortcutManager.ts` | Keyboard shortcut system |
| `ui/src/input/FocusManager.ts` | Focus & tab navigation |
| `ui/src/theme/ThemeEngine.ts` | JSON theme loader, token resolution |
| `ui/src/theme/ThemeLiveReload.ts` | File watcher for live theme reload |

### Existing Files to Modify

| File | Why |
|---|---|
| `src/main.cc` | Add render loop alongside event dispatch |
| `src/kernel/application.cc` | Initialize graphics engine on startup |
| `src/kernel/application.h` | Add GraphicsEngine* / UIEngine* accessors |
| `CMakeLists.txt` | Add new graphics source files |
| `cmake/FindDependencies.cmake` | Add proper FetchContent for Skia, WebGPU, or vcpkg |
| `CMakePresets.json` | May need GPU-specific presets |
| `src/pianoroll/piano_roll_renderer.cc` | Replace stubs with real SkiaCanvas calls |
| `src/mixer/mixer_ui.cc` | Replace stubs with real SkiaCanvas calls |
| `src/pianoroll/expression_lane.cc` | Replace stubs with real SkiaCanvas calls |
| `src/pianoroll/velocity_lane.cc` | Replace stubs with real SkiaCanvas calls |
| `src/browser/browser_panel.cc` | Replace ImGui with GPU widget tree |
| `ui/src/views/ArrangementView/index.ts` | Replace HTML Canvas 2D with GPU-backed rendering |
| `ui/src/index.ts` | Expand UIEngine into real framework |
| `ui/src/components/index.ts` | Expand into full component library |
| `ui/package.json` | May need new deps (FFI helpers, etc.) |

---

## Approaches

### Approach 1: Direct WebGPU C++ (wgpu-native + Skia via GPU backend)

Use **wgpu-native** (Rust-based WebGPU implementation) natively linked as a C library, with Skia's **GPU backend** (skia/src/gpu/ganesh).

```
┌──────────────────────────────────────────────┐
│  C++ Widget Tree (render commands)            │
├──────────────────────────────────────────────┤
│  Skia Ganesh GPU Backend (sk_gpu.h)          │
│  - SkSurface::MakeFromBackendRenderTarget    │
│  - GrDirectContext::makeRenderTarget         │
├──────────────────────────────────────────────┤
│  wgpu-native C API (webgpu.h)                │
│  - wgpuCreateInstance, wgpuDeviceCreate      │
│  - wgpuSwapChain, wgpuRenderPassEncoder      │
├──────────────────────────────────────────────┤
│  Vulkan / Metal / DX12                       │
└──────────────────────────────────────────────┘
```

**Pros**:
- Industry-standard approach (Chrome, Firefox, Flutter use this stack)
- Skia GPU backend is battle-tested at millions of FPS
- Direct C/C++ API without FFI overhead
- Native performance for waveform rendering

**Cons**:
- wgpu-native is primarily Rust — C API is secondary
- Dawn (Google's WebGPU) has better C++ support but heavier build
- Skia build is complex (~30 min first build)
- No TypeScript integration path without separate FFI

**Effort**: High (6-9 months, matches roadmap estimate)

### Approach 2: Dawn WebGPU C++ API + Skia

Use **Dawn** (Google's WebGPU implementation in C++) with native Vulkan/Metal/DX12 backends. Skia integrates directly with Dawn via shared GrContext.

**Pros**:
- First-class C++ API (better than wgpu-native)
- Google-maintained, used by Chrome
- Skia's Dawn backend is well-tested
- Strong multi-window support

**Cons**:
- Dawn build is complex (uses GN/ninja, not CMake)
- Larger binary size than wgpu-native
- Still no direct TypeScript bridge

**Effort**: High

### Approach 3: Hybrid — Dawn for GPU, Skia for 2D, TypeScript for Components

The most ambitious but correct approach matching the roadmap spec:

```
┌──────────────────────────────────────────────┐
│  TypeScript Component Tree                   │
│  (declarative, reactive)                     │
├──────────────────────────────────────────────┤
│  JS/C++ FFI Bridge (v8 / QuickJS / custom)   │
├──────────────────────────────────────────────┤
│  C++ Graphics Bridge                         │
│  (converts VDOM → Skia draw commands)        │
├──────────────────────────────────────────────┤
│  Skia Ganesh / Dawn GPU Backend              │
│  (text, paths, shadows, blur)                │
├──────────────────────────────────────────────┤
│  Dawn (WebGPU C++ API)                       │
│  (device, swap chain, compute shaders)       │
└──────────────────────────────────────────────┘
```

**Pros**:
- Matches the roadmap architecture spec exactly
- Best separation of concerns (TS UI logic / C++ graphics perf)
- Declarative UI is easier to maintain than imperative ImGui/C++ widgets
- TypeScript ecosystem for tooling, testing, theming

**Cons**:
- Requires a JS→C++ FFI bridge (major new component)
- Entire UI pipeline has TS→C++ serialization overhead
- Most complex to implement
- Hardest to debug (3-layer stack)
- Skia + Dawn + JS engine = heaviest binary

**Effort**: Very High (likely 9-12 months)

### Approach 4: Skia-Only C++ Widget System (No WebGPU, No TypeScript)

Use Skia's CPU backend initially, then GPU backend. Widgets are pure C++. Skip TypeScript and WebGPU for P10, defer to P11.

**Pros**:
- Fastest path to a working GPU renderer
- Skia only — no Dawn/wgpu dependency
- All existing SkiaCanvas forward declarations become real
- No FFI complexity
- Smallest risk

**Cons**:
- No WebGPU compute shaders (spectrogram, particle effects)
- No TypeScript component model
- No declarative UI (returns to imperative C++ widgets)
- Must rewrite existing TypeScript views (ArrangementView, etc.) in C++

**Effort**: Medium (3-5 months)

---

## Recommendation

**Execute P10 as 4 sequential sub-phases**:

### Phase 10a: Graphics Foundation (WebGPU + Skia) — 2 months
1. Integrate Dawn as WebGPU implementation via FetchContent or vcpkg
2. Integrate Skia's Ganesh GPU backend via FetchContent
3. Implement `GPUDevice`, `SwapChain`, buffer/texture/pipeline factories
4. Implement concrete `SkiaCanvas` using Skia's GPU backend
5. Make all currently stubbed renderers real (piano roll, mixer, browser)
6. Add render loop to `Application::run()` alongside event dispatch

### Phase 10b: Widget System + Layout Engine — 2 months
1. Implement base `Widget` class hierarchy
2. Implement `RectWidget`, `TextWidget`, `ButtonWidget`, `SliderWidget`, `KnobWidget`
3. Implement `LayoutEngine` (CPU-based flexbox)
4. Implement `DirtyTracker`
5. Implement `AnimationEngine` (CPU-based initially, GPU later)
6. Rewrite `BrowserPanel` from ImGui to GPU widgets

### Phase 10c: TypeScript Component Layer — 2 months
1. Design C++→JS FFI bridge (consider `quickjs` or embedding a minimal JS runtime)
2. Implement TypeScript VDOM component model
3. Implement diff/patch reconciler
4. Implement reactive state management
5. Port ArrangementView from HTML Canvas 2D to GPU components
6. Implement docking system in TypeScript

### Phase 10d: Polish — 1-2 months
1. Write WGSL shaders for custom effects (note instancing, waveform, automation)
2. Implement post-processing (blur, glow, drop shadow)
3. Implement HiDPI support
4. Implement input system (mouse, keyboard, touch, pen)
5. Implement theme system (JSON loader, token resolution, live reload)
6. Multi-window rendering support
7. Performance optimization, draw call batching

### Why this order

The top priority is **unblocking all the stubbed renderers** (piano roll, mixer, browser). Every module that forward-declares `SkiaCanvas` and has `(void)canvas;` stubs is blocked on a concrete SkiaCanvas. Get that done in Phase 10a, then the entire codebase starts rendering.

Widget system (10b) is the next logical step — it replaces the ImGui browser and gives us GPU-native UI. TypeScript layer (10c) can be done in parallel or after, since the C++ widget tree works independently.

---

## Risks

### Risk 1: Skia + Dawn Build Complexity (HIGH)
Both Skia and Dawn are notoriously complex to build. Skia requires GN/ninja or a custom CMake build. Dawn is built with GN. Neither has first-class CMake FetchContent support. **Mitigation**: Consider vcpkg for Skia (has official port), and Dawn's standalone CMake build (recently improved).

### Risk 2: JS→C++ FFI Design (HIGH)
The TypeScript component layer requires a bridge from TS to C++. Options:
- **Embed V8/QuickJS**: Adds ~20MB to binary, complex interop
- **WebAssembly**: Run TS via wasm? Impractical for GPU
- **Use TypeScript only for DOM-like abstraction**: TS generates a VNode tree, C++ reads it via flatbuffers/protobuf — no JS engine needed
- **Skip TS for P10**: Do C++ widgets first, add TS layer in P11

**Decision needed**: Whether to embed a JS engine or find an alternative bridge mechanism.

### Risk 3: Scope Creep (HIGH)
P10 is 1500h by roadmap estimate. The four sub-phases proposed above total ~2000h. **Mitigation**: Strictly gate each sub-phase. Phase 10a is the critical path — everything else depends on it.

### Risk 4: GPU Memory Budget (MEDIUM)
Roadmap estimates ~332 MB GPU memory budget. Real-world DAW with complex projects (100 tracks, 100k notes, 16x oversampling displays) may exceed this. **Mitigation**: Implement GPU memory tracking and compression from day one.

### Risk 5: Existing TypeScript Views (MEDIUM)
`ui/src/views/ArrangementView/` is fully working with HTML Canvas 2D. Rewriting it to GPU-backed rendering means losing working code. **Mitigation**: Keep HTML Canvas 2D as a fallback renderer. Port gradually, view by view, with feature parity tests.

### Risk 6: Cross-Platform GPU Drivers (MEDIUM)
WebGPU abstracts Vulkan/Metal/DX12, but each backend has edge cases:
- Intel GPUs on Windows have known WebGPU quirks
- Linux requires Vulkan 1.2+ or SwiftShader fallback
- Apple Silicon M1+ works well with Metal backend

**Mitigation**: Feature-detection and graceful fallback. Skia software backend as universal fallback.

---

## Ready for Proposal

**Yes** — but with a caveat.

The exploration reveals that P10 is actually **two independent tracks** that could be planned separately:

1. **Track A: GPU Rendering Engine** (C++ — WebGPU + Skia + Widget System) — ~4 months
2. **Track B: TypeScript Component Framework** (TS — VDOM + Layout + Docking) — ~2 months

These can be done in **parallel** by different developers/teams. Track A blocks all existing stubbed renderers (piano roll, mixer). Track B is independent but requires Track A for rendering.

**Recommendation to orchestrator**: Propose P10 as two parallel workstreams with separate specs:
- `p10-gpu-rendering` — SkiaCanvas, WebGPU, widget system, layout engine
- `p10-ts-component-layer` — VDOM, FFI bridge, component library, docking
- `p10-theme-system` — JSON themes, tokens, live reload (smaller, can be a sub-spec)
- `p10-input-system` — input dispatch, shortcuts, accessibility

Consider breaking into 3-4 separate SDD changes rather than one monolithic P10 change. Each change is independently deliverable and verifiable.
