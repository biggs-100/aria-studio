# Proposal: P10a — GPU Rendering Foundation

## Intent

Unblock stubbed renderers with a concrete GPU stack. Replace ImGui provisional UI with native GPU widgets. Establish Dawn+Skia+TypeScript as P10 foundation.

## Scope

### In Scope
- Dawn WebGPU device, swap chain, buffer/texture factories
- SkiaCanvas concrete impl (Ganesh GPU backend)
- Render loop (vsync, present) in Application::run()
- TS→C++ FFI bridge for widget render dispatch
- Basic widget tree (Widget base, Rect/Text/Button)
- Rewrite browser_panel.cc from ImGui to GPU widgets
- Wire build: FetchContent Dawn+Skia, ARIA_FEATURE_GPU activation

### Out of Scope
- Full widget library (P10b), theme/animation/input (P10c)
- Docking (P10b), post-processing WGSL compute (P10d)

## Capabilities

All new — no existing specs match GPU. Each → `openspec/specs/<name>/spec.md`.

### New Capabilities
- `gpu-rendering`: Dawn WebGPU + Skia GPU canvas backend
- `render-loop`: Frame timing, vsync, present, profiling
- `ts-ffi-bridge`: TypeScript ↔ C++ FFI for draw dispatch
- `gpu-widget-system`: Widget tree, layout, dirty tracking
- `gpu-browser-panel`: Rewrite ImGui browser to GPU widgets

### Modified Capabilities
None

## Approach

Four-layer stack: Dawn (device) → Skia Ganesh (2D GPU) → C++ Widget Tree → TS FFI Bridge. Render loop drives Application::run() at vsync. Browser panel rewritten to GPU; ImGui dependency removed.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/graphics/` | New | Dawn, SkiaCanvas, widgets, render loop, FFI |
| `src/browser/*` | Modified | ImGui→GPU; drop ARIA_FEATURE_IMGUI |
| `src/kernel/application.*` | Modified | Render loop + GraphicsEngine* accessor |
| `CMakeLists.txt` | Modified | Graphics sources, Dawn+Skia link |
| `cmake/FindDependencies.cmake` | Modified | FetchContent Dawn + Skia |
| `ui/src/ffi/bridge.ts` | New | TS→C++ FFI bridge |
| `ui/src/index.ts` | Modified | Wire GPU FFI into UIEngine |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Skia+Dawn build complexity | HIGH | vcpkg or FetchContent; CI cache |
| TS FFI bridge design | HIGH | Flatbuffer cmd buffer first; QuickJS in P10c |
| GPU memory budget | MED | Track from day one; Skia sw fallback |

## Rollback Plan

Set `ARIA_FEATURE_GPU=OFF`; restore `ARIA_FEATURE_IMGUI`. Stubbed renderers return. HTML Canvas 2D ArrangementView unaffected.

## Dependencies

- Dawn (WebGPU C++): FetchContent or vcpkg
- Skia (Ganesh GPU): FetchContent or vcpkg
- CMake 3.28+ (existing)

## Success Criteria

- [ ] Dawn device creates visible swap chain (test window)
- [ ] SkiaCanvas renders GPU-backed shapes (unit test)
- [ ] Render loop delivers consistent frame timing
- [ ] TS→C++ FFI dispatches one draw command
- [ ] Browser panel renders via GPU; ImGui dep removed
- [ ] Piano roll + mixer stubs compile against concrete SkiaCanvas
- [ ] `ctest --preset debug` passes, no regressions
