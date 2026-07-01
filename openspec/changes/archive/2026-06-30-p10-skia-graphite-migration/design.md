# Design: P10 — Skia Ganesh→Graphite Migration (Dawn)

## Technical Approach

Pure refactor replacing Ganesh APIs with Graphite equivalents across 5 files. Zero behavioral change to the public `SkiaCanvas` API — `flush()`, `init()`, and all draw methods keep their signatures. Build system include dirs change first, then `GraphicsEngine` exposes `wgpu::Queue`, then `skia_canvas.cc` core migration with the new 3-step flush lifecycle (`snap` → `insertRecording` → `submit`). The vendored Skia (`chrome/m126`) already builds with `skia_enable_graphite=true` — only ARIA's code needs updating.

## Architecture Decisions

| Decision | Options | Tradeoffs | Chosen |
|----------|---------|-----------|--------|
| **Dawn object ownership** | (a) GraphicsEngine owns all (b) SkiaCanvas holds wgpu handles | (a) single init/shutdown path, tighter coupling (b) risk of dangling handles | **a** — engine remains sole owner; accessors return copies of ref-counted handles |
| **Queue exposure** | (a) wgpu accessors on engine (b) pass via init() param (c) friend class | (a) clean, reusable by other consumers (b) couples SkiaCanvas init signature (c) breaks encapsulation | **a** — `device()`, `queue()`, `instance()` const accessors on GraphicsEngine |
| **Recorder lifecycle** | (a) store + recreate after each snap (b) create per draw frame externally | (a) natural per-frame mapping (b) more allocations, ownership unclear | **a** — `recorder` stored in Impl; `makeRecorder()` after each `snap()` |
| **Migration order** | (a) single atomic PR (b) per-file sequential phases | (a) large diff, hard to review (b) clean commits, testable increments | **b** — 4 ordered phases matching the proposal |
| **Surface recreation** | (a) reuse existing recorder (b) create new recorder | (a) may be consumed by prior snap (b) safe, explicit, no assumptions | **b** — create new recorder inside `recreate_surface()` |

## Graphite Lifecycle

```
init():
  engine.device()   ──→ DawnBackendContext ──→ ContextFactory::MakeDawn() ──→ Context (unique_ptr)
  engine.queue()    ──→ DawnBackendContext                                        │
  engine.instance() ──→ DawnBackendContext                                        │
                                                                                  ▼
                                                                           context→makeRecorder() ──→ Recorder
                                                                                                          │
                                                                                                          ▼
                                                                                                     SkSurfaces::RenderTarget()
                                                                                                          │
                                                                                                          ▼
                                                                                                     canvas_ptr (from surface)

flush():
  recorder→snap() ──→ Recording ──→ InsertRecordingInfo ──→ context→insertRecording()
                                                                          │
                                                                          ▼
                                                                    context→submit(SyncToCpu::kNo)
                                                                          │
                                                                          ▼
                                                              context→makeRecorder() (new for next frame)

recreate_surface():
  context→makeRecorder() ──→ Recorder ──→ SkSurfaces::RenderTarget() ──→ fresh canvas_ptr
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `cmake/FindSkiaFetch.cmake` | Modify | Lines 157-158: `ganesh/` → `graphite/` include dirs. Keep `include/gpu` as-is. |
| `src/graphics/graphics_engine.h` | Modify | Add `device()`, `queue()`, `instance()` const accessors. Include `<dawn/webgpu_cpp.h>`. |
| `src/graphics/graphics_engine.cc` | Modify | Add `wgpu::Queue queue` to `Impl`; init via `device.GetQueue()` in `init_device()`. Implement accessors. |
| `src/graphics/skia_canvas.h` | Modify | Replace `class GrDirectContext;` with graphite fwd decls. Update doc comment (Ganesh → Graphite). |
| `src/graphics/skia_canvas.cc` | Modify | Core migration — 8 Ganesh refs replaced. `sk_sp<GrDirectContext>` → `unique_ptr<Context>`, add `Recorder`, 3-step flush, surface creation via recorder. |
| `tests/unit/test_skia_canvas.cc` | Modify | None expected structurally (all tests pass `nullptr`). Compile-only verification. |

## Interfaces / Contracts

```cpp
// ── New on GraphicsEngine (graphics_engine.h) ─────────────────────
class GraphicsEngine {
public:
    wgpu::Device   device()   const noexcept;   // valid after init()
    wgpu::Queue    queue()    const noexcept;   // valid after init()
    wgpu::Instance instance() const noexcept;   // valid after init()
};

// ── New Impl layout (skia_canvas.cc) ──────────────────────────────
struct SkiaCanvas::Impl {
    std::unique_ptr<skgpu::graphite::Context>  context;    // was sk_sp<GrDirectContext>
    std::unique_ptr<skgpu::graphite::Recorder> recorder;   // new — per-frame command capture
    sk_sp<SkSurface>                           surface;
    SkCanvas*                                  canvas_ptr = nullptr;
    GraphicsEngine*                            engine     = nullptr;
    WGPUSwapChainImpl*                         swapchain  = nullptr;
    uint32_t                                   draw_calls = 0;
    bool                                       ready      = false;
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | `test_skia_canvas.cc` (9 cases) | Compile-only — all pass `nullptr`, no GPU init; type changes compile-check |
| Build | Full CMake `--preset debug` | Zero Ganesh references permitted; `ctest --preset debug --output-on-failure` |
| Manual | Visual output parity | Run `aria` binary, verify rendering matches pre-migration |

No new tests planned — existing coverage validates the public API contract. A Graphite init smoke test is noted as optional.

## Migration Order

```
Phase 1: cmake/FindSkiaFetch.cmake   — ganesh/ → graphite/ include dirs
Phase 2: GraphicsEngine (h/cc)       — wgpu::Queue storage + accessors
Phase 3: skia_canvas.cc              — core Graphite migration (Context, Recorder, 3-step flush)
Phase 4: skia_canvas.h               — forward decls + comment cleanup
Phase 5: Build + verify              — cmake --build --preset debug && ctest --preset debug
```

Phases 1–2 must precede phase 3 (engine must expose queue, headers must exist at right paths). Phases 3–4 can be atomically swapped or combined.

## Open Questions

- [ ] Does `getCanvas()` on a Graphite `SkSurface` remain valid after the associated Recorder is snapped? Or does the canvas pointer need re-retrieval after each new recorder?
- [ ] Is `context->makeRecorder()` cheap enough to call per-frame (after each flush)? Needs perf verification.
- [ ] Does `DawnNativeProcessEventsFunction` require an explicit `#include <dawn/native/DawnNative.h>` beyond what GraphicsEngine already includes?
