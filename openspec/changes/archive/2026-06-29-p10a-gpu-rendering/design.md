# Design: P10a — GPU Rendering Foundation

## Technical Approach

Four-layer GPU stack replacing ImGui with native C++ widgets, Dawn WebGPU, and Skia Ganesh. TS FFI bridge provides the declarative→imperative pipeline for future P10b+ components. Browser panel rewritten as C++ `ContainerWidget` tree. Render loop drives `Application::run()` at 144 FPS target.

## Architecture Decisions

| Decision | Choice | Alternatives | Rationale |
|----------|--------|-------------|-----------|
| WebGPU lib | **Dawn** (Google C++) | wgpu-native | User chose Dawn; richer C++ API, Google-maintained, `GrDawnBackendContext` for Skia |
| Skia build | **FetchContent from source** | vcpkg, system | User chose source; cross-platform consistency; no CI binary cache needed yet |
| Skia backend | **Ganesh (GPU)** | Software raster | Direct Dawn texture sharing; zero-copy; spec requires `WGPUTexture`→`SkSurface` |
| Cmd buffer | **JSON** serialized array | FlatBuffers, binary | P10a debug ergonomics; upgradeable in P10c; no perf bottleneck at current widget count |
| Widget tree | **C++ native** (own tree) | TS-owned C++ proxy | Render loop in C++; avoids FFI roundtrip every frame for browser panel |
| Browser panel | **ContainerWidget tree** | Hybrid ImGui | Eliminates `ARIA_FEATURE_IMGUI` dep per spec; direct GPU rendering |
| Render loop | **`Application::run()` owns loop** | Separate render thread | Simpler threading; no IPC; matches existing event-dispatch pattern |

## Data Flow

```
TS Components (future P10b)    C++ Widget Tree (P10a)
        │                              │
        ▼                              ▼
  bridge.serialize()            Widget::render(SkiaCanvas*)
        │                              │
        ▼                              │
  JSON cmd buffer ◄──── FFI Bridge ────┤
        │                              │
        ▼                              ▼
  bridge.execute(buffer) ───► SkiaCanvas::drawRect/drawTextBlob
                                      │
                                      ▼
                              Skia Ganesh (flush)
                                      │
                                      ▼
                              Dawn WebGPU (Present)
                                      │
                                      ▼
                                    GPU
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `cmake/FindDependencies.cmake` | **Modify** | Replace `find_package` with `FetchContent` for Dawn + Skia from source |
| `cmake/FindDawn.cmake` | Create | FetchContent Dawn (google/dawn) with GPU+present features |
| `cmake/FindSkiaFetch.cmake` | Create | FetchContent Skia source, build with `skia_enable_gpu=true` |
| `CMakeLists.txt` | **Modify** | Add `GRAPHICS_SOURCES`, link Dawn+Skia, guard with `ARIA_FEATURE_GPU` |
| `src/graphics/graphics_engine.h/.cc` | Create | `GraphicsEngine` — Dawn device, swapchain factory, SkiaCanvas factory |
| `src/graphics/skia_canvas.h/.cc` | Create | `SkiaCanvas` — concrete impl via `GrDawnBackendContext`, exposes `clear/drawRect/drawCircle/drawTextBlob/flush` |
| `src/graphics/render_loop.h/.cc` | Create | `RenderLoop` — frame timing (144 FPS), vsync present, profiling counters, budget monitor |
| `src/graphics/widget.h/.cc` | Create | `Widget` base class — identity, bounds, parent/child tree, paint pass, hit-testing |
| `src/graphics/widget_rect.h/.cc` | Create | `RectWidget` — rounded rect with fill/border/shadow |
| `src/graphics/widget_text.h/.cc` | Create | `TextWidget` — GPU text via Skia textblob |
| `src/graphics/widget_button.h/.cc` | Create | `ButtonWidget` — rect + label + hover/pressed states |
| `src/graphics/widget_container.h/.cc` | Create | `ContainerWidget` — clipping, transform stack, scroll |
| `src/graphics/layout_engine.h/.cc` | Create | `LayoutEngine` — two-phase measure/arrange, flex layout |
| `src/graphics/dirty_tracker.h/.cc` | Create | `DirtyTracker` — per-widget dirty rect tracking |
| `src/graphics/ffi/bridge.h/.cc` | Create | C++ FFI bridge — receive JSON cmd buffer, dispatch to SkiaCanvas |
| `src/graphics/ffi/command_types.h` | Create | Cmd type enum + JSON deserialization |
| `src/graphics/ffi/event_dispatcher.h/.cc` | Create | Input event dispatch from OS→TS callbacks |
| `src/kernel/application.cc` | **Modify** | Integrate `RenderLoop` into `run()`; register `GraphicsEngine*` via `ServiceLocator` |
| `src/browser/browser_panel.h/.cc` | **Modify** | Rewrite: `ContainerWidget` root, toolbar/search/tree/preview as children, remove `#ifdef ARIA_FEATURE_IMGUI` |
| `src/browser/browser_panel_gpu.h/.cc` | Create | GPU widget extensions for browser (tree node widget, waveform preview widget) |
| `ui/src/ffi/bridge.ts` | Create | TS bridge — `serialize(component)`, `execute(buffer)`, event callback registration |
| `ui/src/ffi/types.ts` | Create | Command type definitions (`DrawRect`, `DrawTextBlob`, `DrawImage`, `SetClip`, etc.) |
| `ui/src/ffi/commands.ts` | Create | Command builder helpers for each type |
| `ui/src/index.ts` | **Modify** | Wire FFI bridge into `UIEngine` |
| `tests/unit/test_graphics_engine.cc` | Create | Dawn device init, swapchain lifecycle, fallback tests |
| `tests/unit/test_skia_canvas.cc` | Create | Draw primitives, flush, surface recreation |
| `tests/unit/test_render_loop.cc` | Create | Frame timing, budget monitor, profiling counters |
| `tests/unit/test_widget.cc` | Create | Widget tree, layout pass, paint pass, hit-testing |
| `tests/unit/test_widget_container.cc` | Create | Clipping, transform stack, scroll |
| `tests/unit/test_ffi_bridge.cc` | Create | C++ bridge execute, JSON deserialize, event dispatch |
| `tests/CMakeLists.txt` | **Modify** | Register GPU test executables |

## Interfaces / Contracts

```cpp
// GraphicsEngine — device + swapchain lifecycle
class GraphicsEngine {
public:
    bool init();                          // Dawn device + adapter
    SwapChainHandle create_swapchain(WindowHandle w);
    void resize_swapchain(SwapChainHandle s, int w, int h);
    SkiaCanvas* create_canvas(SwapChainHandle s);
    void present(SwapChainHandle s);
};

// SkiaCanvas — GPU 2D draw surface
class SkiaCanvas {
public:
    void clear(const Color& c);
    void drawRect(const Rect& r, const Paint& p);
    void drawCircle(float x, float y, float r, const Paint& p);
    void drawTextBlob(const char* text, const Font& f, float x, float y, const Paint& p);
    void flush();
    void save(); void restore();
    void clipRect(const Rect& r);
};

// RenderLoop — frame timing + profiling
struct FrameCounters { float frame_time_ms, flush_ms, present_ms; int draw_calls; };
class RenderLoop { /* tick(), profiling_counters(), target_fps(), pacing sleep */ };
```

```typescript
// TS FFI Bridge
interface DrawCommand { type: 'DrawRect'|'DrawTextBlob'|'DrawImage'|'SetClip'|'PushTransform'|'PopTransform'; params: Record<string, unknown>; }
interface Bridge { serialize(component: VNode): DrawCommand[]; execute(commands: DrawCommand[]): void; onEvent(type: string, handler: (e: InputEvent) => void): void; }
```

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Unit | Dawn device init/fallback | Mock no-GPU via `WGPUInstanceDescriptor` null backend |
| Unit | SkiaCanvas draw primitives | Create in-memory surface, verify pixel at (0,0) |
| Unit | RenderLoop frame timing | Inject `steady_clock` mock, verify sleep/present cycle |
| Unit | Widget tree (layout/hit-test) | Pure C++: construct tree, measure, arrange, query bounds |
| Unit | FFI bridge (JSON → SkiaCanvas) | Deserialize known JSON, verify SkiaCanvas mock called in order |
| Integration | BrowserPanel GPU rewrite | Construct widget tree, paint pass, verify SkiaCanvas calls |
| E2E | Full frame cycle | `Application::run()` one tick: input→layout→paint→present |

## Migration / Rollout

**3-4 PR slices** (400-line budget per PR):

| PR | Scope | Key Files | Risk |
|----|-------|-----------|------|
| **1** | Build infra + Dawn/Skia fetch + empty `GraphicsEngine` skeleton | `cmake/FindDawn.cmake`, `cmake/FindSkiaFetch.cmake`, `src/graphics/graphics_engine.h/cc` | HIGH — build complexity |
| **2** | Widget base + `SkiaCanvas` + layout engine + `RenderLoop` in `Application::run()` | Widget tree, SkiaCanvas, RenderLoop, LayoutEngine | MED — new patterns |
| **3** | TS FFI bridge + BrowserPanel GPU rewrite + event dispatch | `ui/src/ffi/bridge.ts`, `browser_panel_gpu.*`, `ffi/bridge.cc` | MED — FFI protocol |
| **4** | Waveform preview widget + DnD integration + profiling polish | preview widget, drag source, counters, tests | LOW |

Rollback: `ARIA_FEATURE_GPU=OFF` restores ImGui fallback via existing `#ifdef` guard.

## Open Questions

- [ ] Dawn `WGPUInstanceDescriptor` null backend for headless testing — does Dawn support this?
- [ ] JSON command buffer size limit for large widget trees (KB?) — monitor in PR3
- [ ] Skia FetchContent build time on CI — may need ccache in PR1
