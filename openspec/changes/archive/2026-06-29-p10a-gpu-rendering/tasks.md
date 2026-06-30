# Tasks: P10a — GPU Rendering Foundation

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 3500–5500 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 → PR 2 → PR 3 → PR 4 |
| Delivery strategy | ask-on-risk |
| Chain strategy | feature-branch-chain |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Build infra + Dawn/Skia fetch + GraphicsEngine skeleton | PR 1 | base=feature/tracker; HIGH build risk |
| 2 | Widget tree + SkiaCanvas + LayoutEngine + RenderLoop | PR 2 | base=PR1 branch; MED risk |
| 3 | TS FFI bridge + BrowserPanel GPU rewrite + event dispatch | PR 3 | base=PR2 branch; MED risk |
| 4 | Waveform preview + DnD + profiling polish + tests | PR 4 | base=PR3 branch; LOW risk |

## Phase 1: Build Infrastructure

- [x] 1.1 Create `cmake/FindDawn.cmake` — FetchContent google/dawn with GPU+present features
- [x] 1.2 Create `cmake/FindSkiaFetch.cmake` — FetchContent Skia, `skia_enable_gpu=true`
- [x] 1.3 Create `src/graphics/graphics_engine.h/.cc` — Dawn device, swapchain factory, `init()`
- [x] 1.4 Modify `CMakeLists.txt` — add `GRAPHICS_SOURCES`, link Dawn+Skia, `ARIA_FEATURE_GPU` guard
- [x] 1.5 Modify `cmake/FindDependencies.cmake` — include FindDawn + FindSkiaFetch

## Phase 2: Core Widget System + Render Loop

- [x] 2.1 Create `src/graphics/skia_canvas.h/.cc` — SkiaCanvas via `GrDawnBackendContext`, clear/drawRect/drawCircle/drawTextBlob/flush
- [x] 2.2 Create `src/graphics/widget.h/.cc` — Widget base: id, bounds, parent/child tree, hit-test
- [x] 2.3 Create `src/graphics/widget_rect.h/.cc` — RectWidget: rounded rect fill/border/shadow
- [x] 2.4 Create `src/graphics/widget_text.h/.cc` — TextWidget via Skia textblob
- [x] 2.5 Create `src/graphics/widget_button.h/.cc` — ButtonWidget with hover/pressed states
- [x] 2.6 Create `src/graphics/widget_container.h/.cc` — ContainerWidget: clipping, transform stack, scroll
- [x] 2.7 Create `src/graphics/layout_engine.h/.cc` — LayoutEngine: measure/arrange, flex layout
- [x] 2.8 Create `src/graphics/dirty_tracker.h/.cc` — DirtyTracker: per-widget dirty rect tracking
- [x] 2.9 Create `src/graphics/render_loop.h/.cc` — RenderLoop: 144 FPS timing, vsync present, profiling counters
- [x] 2.10 Modify `src/kernel/application.cc` — wire RenderLoop into `run()`, register GraphicsEngine

## Phase 3: FFI Bridge + Browser Panel

- [x] 3.1 Create `src/graphics/ffi/command_types.h` — cmd type enum + JSON deserialization
- [x] 3.2 Create `src/graphics/ffi/bridge.h/.cc` — receive JSON cmd buffer, dispatch to SkiaCanvas
- [x] 3.3 Create `src/graphics/ffi/event_dispatcher.h/.cc` — Input event dispatch OS→TS with HiDPI scaling
- [x] 3.4 Create `ui/src/ffi/types.ts` — DrawCommand type definitions
- [x] 3.5 Create `ui/src/ffi/commands.ts` — Command builder helpers
- [x] 3.6 Create `ui/src/ffi/bridge.ts` — serialize(component), execute(buffer), onEvent()
- [x] 3.7 Modify `ui/src/index.ts` — wire FFI bridge into UIEngine
- [x] 3.8 Create `src/browser/browser_panel_gpu.h/.cc` — tree node + waveform preview widgets
- [x] 3.9 Modify `src/browser/browser_panel.h/.cc` — ContainerWidget root, remove ARIA_FEATURE_IMGUI

## Phase 4: Testing

- [x] 4.1 Create `tests/unit/test_graphics_engine.cc` — init, swapchain lifecycle, fallback
- [x] 4.2 Create `tests/unit/test_skia_canvas.cc` — draw primitives, flush, surface recreation
- [x] 4.3 Create `tests/unit/test_render_loop.cc` — frame timing, budget monitor, profiling
- [x] 4.4 Create `tests/unit/test_widget.cc` — tree, layout, paint pass, hit-testing
- [x] 4.5 Create `tests/unit/test_widget_container.cc` — clipping, transform stack, scroll
- [x] 4.6 Create `tests/unit/test_ffi_bridge.cc` — execute, JSON deserialize, event dispatch
- [x] 4.7 Modify `tests/CMakeLists.txt` — register GPU test executables

## Phase 5: Polish & DnD Integration (PR 4 — Final)

- [x] 5.1 Polish `WaveformPreviewWidget` — add time ruler ticks, zoom level indicator, improved peak/minima rendering
- [x] 5.2 Wire drag-and-drop from GPU browser panel → `ProjectManager::create_audio_clip()`
- [x] 5.3 Add profiling counters to `RenderLoop` — frame time histogram (min/max/avg), budget violation logging
- [x] 5.4 Stabilize build — ensure ALL files compile under `ARIA_FEATURE_GPU`
