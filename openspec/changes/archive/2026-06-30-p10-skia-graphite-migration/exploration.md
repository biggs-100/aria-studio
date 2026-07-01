## Exploration: Skia Ganesh→Graphite (Dawn) Migration

### Current State

ARIA DAW's GPU rendering stack has three layers:
1. **Dawn** (WebGPU implementation) — manages device/queue/adapter via `GraphicsEngine`
2. **Skia Ganesh** (deprecated GPU backend) — renders 2D via `GrDirectContext::MakeDawn()`
3. **ARIA code** (`skia_canvas.h/.cc`) — the ONLY file using Ganesh APIs

The Skia build (`cmake/FindSkiaFetch.cmake`) already compiles Skia **with Graphite enabled**:
```
skia_enable_graphite=true
```

But the ARIA code exclusively uses Ganesh. The vendored Skia is `chrome/m126`, which has removed the Ganesh+Dawn backend entirely — so the existing code **cannot compile** against this Skia version without Ganesh headers.

Confirmation: the `vendor/skia/` directory is a symlink to `C:\Users\USER\AppData\Local\Temp\skia\` (a FetchContent population).

---

### All Ganesh API Usage — Single File

**File: `src/graphics/skia_canvas.h`** (2 references)
- Line 13: `class GrDirectContext;` — forward declaration
- Line 33: `/// Wraps a \`GrDawnBackendContext\` and \`SkSurface\`` — comment only

**File: `src/graphics/skia_canvas.cc`** (8 references)
- Line 17: `#include <gpu/GrDirectContext.h>`
- Line 18: `#include <gpu/ganesh/SkSurfaceGanesh.h>`
- Line 19: `#include <gpu/ganesh/dawn/GrDawnBackendContext.h>`
- Line 32: `sk_sp<GrDirectContext> gr_context;` — member variable
- Line 97: `GrDawnBackendContext dawn_context{};` — stack variable
- Line 101: `impl_->gr_context = GrDirectContext::MakeDawn(dawn_context);`
- Line 117: `impl_->surface = SkSurfaces::RenderTarget(impl_->gr_context.get(), skgpu::Budgeted::kYes, image_info);`
- Line 237: `impl_->gr_context->flush(); impl_->gr_context->submit();`

**No other file** in `src/` or `tests/` uses Ganesh APIs — confirmed by grep for `GrDirectContext`, `GrDawnBackendContext`, `gpu/ganesh`, `GrBackendRenderTarget`.

---

### Graphite API Mapping Table

| Ganesh API | Graphite Equivalent | Notes |
|---|---|---|
| `GrDirectContext` | `skgpu::graphite::Context` | `std::unique_ptr<Context>`, not `sk_sp` |
| `GrDirectContext::MakeDawn(dawn_ctx)` | `skgpu::graphite::ContextFactory::MakeDawn(dawn_ctx, opts)` | Returns `unique_ptr` via factory in `<gpu/graphite/dawn/DawnUtils.h>` |
| `GrDawnBackendContext` | `skgpu::graphite::DawnBackendContext` | Same concept but struct fields differ slightly |
| `GrDawnBackendContext{fDevice=..., fQueue=...}` | `DawnBackendContext{fInstance, fDevice, fQueue, fTick}` | Graphite requires `fInstance` (wgpu::Instance) and `fTick` function |
| `SkSurfaces::RenderTarget(GrDirectContext*, Budgeted, ImageInfo)` | `SkSurfaces::RenderTarget(Recorder*, ImageInfo, Mipmapped, SurfaceProps*)` | Both are in `SkSurfaces` namespace — overloaded by argument type |
| `gr_context->flush(); gr_context->submit();` | `recorder->snap()` → `context->insertRecording(info)` → `context->submit(SyncToCpu)` | Three-step: snapshot recording, insert, submit |
| `gr_context->flush()` (no-arg) | `context->submit(SyncToCpu::kNo)` | Non-blocking submit |
| `skgpu::Budgeted::kYes` | (not needed — `SkSurfaces::RenderTarget` for Graphite takes `Mipmapped` instead) | Budgeted is a Ganesh concept |
| `#include <gpu/GrDirectContext.h>` | `#include <gpu/graphite/Context.h>` | — |
| `#include <gpu/ganesh/SkSurfaceGanesh.h>` | `#include <gpu/graphite/Surface.h>` | — |
| `#include <gpu/ganesh/dawn/GrDawnBackendContext.h>` | `#include <gpu/graphite/dawn/DawnBackendContext.h>` | — |
| — (new) | `#include <gpu/graphite/dawn/DawnUtils.h>` | For `ContextFactory::MakeDawn` |
| — (new) | `#include <gpu/graphite/Recorder.h>` | Recorder is a new concept in Graphite |

---

### Available Graphite Headers (vendor/skia/include/gpu/graphite/)

All 20 headers confirmed present:

| Header | Path | Used in Migration? |
|---|---|---|
| **Context.h** | `include/gpu/graphite/Context.h` | ✅ Yes — replaces `GrDirectContext` |
| **Recorder.h** | `include/gpu/graphite/Recorder.h` | ✅ Yes — new concept, required |
| **Surface.h** | `include/gpu/graphite/Surface.h` | ✅ Yes — `SkSurfaces::RenderTarget` for Graphite |
| **Recording.h** | `include/gpu/graphite/Recording.h` | ✅ Yes — `recorder->snap()` returns this |
| **GraphiteTypes.h** | `include/gpu/graphite/GraphiteTypes.h` | ✅ Yes — `SyncToCpu`, `InsertRecordingInfo` |
| **dawn/DawnBackendContext.h** | `include/gpu/graphite/dawn/DawnBackendContext.h` | ✅ Yes — replaces `GrDawnBackendContext` |
| **dawn/DawnUtils.h** | `include/gpu/graphite/dawn/DawnUtils.h` | ✅ Yes — `ContextFactory::MakeDawn` |
| **dawn/DawnTypes.h** | `include/gpu/graphite/dawn/DawnTypes.h` | ✅ Yes — `DawnTextureInfo` |
| **BackendTexture.h** | `include/gpu/graphite/BackendTexture.h` | 🔲 Maybe — for `WrapBackendTexture` |
| ContextOptions.h | `include/gpu/graphite/ContextOptions.h` | 🔲 Maybe — for `ContextFactory::MakeDawn` options |
| Image.h | `include/gpu/graphite/Image.h` | ❌ Not needed |
| ImageProvider.h | `include/gpu/graphite/ImageProvider.h` | ❌ Not needed |
| TextureInfo.h | `include/gpu/graphite/TextureInfo.h` | ❌ Not needed |
| BackendSemaphore.h | `include/gpu/graphite/BackendSemaphore.h` | ❌ Not needed |
| mtl/*.h, vk/*.h | Platform-specific | ❌ Not needed |
| YUVABackendTextures.h | `include/gpu/graphite/YUVABackendTextures.h` | ❌ Not needed |

---

### Dawn Backend State

**Dawn source**: vendored at `vendor/dawn/` (symlink to `C:\Users\USER\AppData\Local\Temp\dawn\`).

**Build integration** (`cmake/FindDawn.cmake`):
- Vendored Dawn preferred → `add_subdirectory("vendor/dawn" "build_vendor_dawn" EXCLUDE_FROM_ALL)`
- Defines aliases: `Dawn::dawn_native`, `Dawn::dawn_platform`, `Dawn::dawn_proc`, `Dawn::webgpu_dawn`
- `DAWN_FETCH_DEPENDENCIES` flag exists in Dawn's CMakeLists.txt (line 219) but is **OFF by default** and **NOT set** by ARIA's FindDawn.cmake
- Dawn's CMakeLists.txt at line 219: `option(DAWN_FETCH_DEPENDENCIES "Use fetch_dawn_dependencies.py..." OFF)`

**`DAWN_FETCH_DEPENDENCIES=OFF`** → Dawn relies on `depot_tools` for its dependencies (abseil, jni, etc.). The vendored Dawn at `vendor/dawn/` already has these dependencies populated, so no special action is needed.

**What `GraphicsEngine` exposes to SkiaCanvas**:
- `GraphicsEngine::Impl` has: `wgpu::Instance instance`, `wgpu::Device device`
- **Missing for Graphite**: `wgpu::Queue fQueue` — `GraphicsEngine` does NOT currently expose a queue
- The `wgpu::Device` inherently has a default queue, accessible via `device.GetQueue()`

**Key gap for Graphite**: Graphite's `DawnBackendContext` requires:
- `wgpu::Instance fInstance` ✅ (available in GraphicsEngine::Impl)
- `wgpu::Device fDevice` ✅ (available)
- `wgpu::Queue fQueue` 🔲 (needs to be exposed from engine or created)
- `DawnTickFunction* fTick` ✅ (default `DawnNativeProcessEventsFunction` works on Windows)

---

### Affected Areas

| File | Change Required | Impact |
|---|---|---|
| **`src/graphics/skia_canvas.h`** | Replace `GrDirectContext` forward decl with graphite types; update comment | Public header — consumers include via `graphics/skia_canvas.h` |
| **`src/graphics/skia_canvas.cc`** | Full migration: includes, Context creation, Recorder, flush/submit flow | Core rendering path |
| **`src/graphics/graphics_engine.h`** | Expose `wgpu::Queue` or add accessor for device/queue | Required for Graphite's `DawnBackendContext` |
| **`src/graphics/graphics_engine.cc`** | Store queue reference, possibly expose queue getter | Required for init flow |
| **`cmake/FindSkiaFetch.cmake`** | Replace Ganesh include dirs with Graphite include dirs | Build system |
| **`tests/unit/test_skia_canvas.cc`** | Update structure tests — init path changes slightly | Low impact (tests use `nullptr` init) |

---

### Migration Order (recomendado)

```
Phase 1: Build system + Headers        (FindSkiaFetch.cmake)
    ↓
Phase 2: GraphicsEngine queue expose   (graphics_engine.h/.cc)
    ↓
Phase 3: skia_canvas.cc migration      (core rewrite)
    ↓
Phase 4: skia_canvas.h cleanup         (forward decls, comments)
    ↓
Phase 5: Test adjustments              (test_skia_canvas.cc)
    ↓
Phase 6: Build + verify full suite
```

**Phase 1** must come first (otherwise headers don't exist at the right paths).
**Phase 2** must come before **Phase 3** (engine must provide queue for Graphite backend context).
**Phase 3 and 4** can be done together (they touch the same class).
**Phase 5** can verify after Phase 3-4.

---

### Skia Build Steps

**Current state**: Skia is built via GN+ninja through a custom target in `FindSkiaFetch.cmake`:
1. `gn gen out/Debug --args="skia_enable_gpu=true skia_use_dawn=true skia_enable_graphite=true ..."`
2. `ninja -C out/Debug skia`

**No build changes needed** for Skia itself — `skia_enable_graphite=true` is already set.

**Include directory changes needed** in `FindSkiaFetch.cmake` (lines 157-158):
```cmake
# CURRENT (Ganesh):
"${skia_SOURCE_DIR}/include/gpu/ganesh"
"${skia_SOURCE_DIR}/include/gpu/ganesh/dawn"

# NEW (Graphite):
"${skia_SOURCE_DIR}/include/gpu/graphite"
"${skia_SOURCE_DIR}/include/gpu/graphite/dawn"
```

Keep `"${skia_SOURCE_DIR}/include/gpu"` as-is — both Ganesh and Graphite types use `<gpu/GpuTypes.h>` etc.

---

### Dawn Build Steps

**No Dawn build changes required.**
- Vendored Dawn at `vendor/dawn/` already compiles with CMake's `add_subdirectory`
- `DAWN_FETCH_DEPENDENCIES` is OFF but vendored deps are present
- The `wgpu::Queue` is available via `device.GetQueue()` — no new Dawn targets needed
- `GraphicsEngine` must store a `wgpu::Queue` member (trivial addition)

---

### Detailed Migration: skia_canvas.cc

**Current Ganesh lifecycle:**
```
1. Create GrDawnBackendContext { fDevice=..., fQueue=... }
2. gr_context = GrDirectContext::MakeDawn(dawn_context)
3. surface = SkSurfaces::RenderTarget(gr_context, Budgeted::kYes, image_info)
4. canvas = surface->getCanvas()
5. draw...
6. gr_context->flush()
7. gr_context->submit()
```

**New Graphite lifecycle:**
```
1. Create graphite::DawnBackendContext { fInstance, fDevice, fQueue, fTick }
2. context = ContextFactory::MakeDawn(dawn_context, ContextOptions{})
3. recorder = context->makeRecorder()
4. surface = SkSurfaces::RenderTarget(recorder.get(), image_info, Mipmapped::kNo)
5. canvas = surface->getCanvas()
6. draw...
7. recording = recorder->snap()
8. context->insertRecording({.fRecording = recording.get()})
9. context->submit(SyncToCpu::kNo)
```

**Key differences:**
- `Context` is `std::unique_ptr`, not `sk_sp`
- `Recorder` is an intermediate object — holds command buffer state
- `snap()` → `insertRecording()` → `submit()` replaces single `flush()+submit()`
- Surface creation takes `Recorder*` instead of `GrDirectContext*`
- `DawnBackendContext` now requires `fInstance` AND `fQueue` (Ganesh only needed `fDevice`)

---

### Test Strategy

| Test File | Impact | Strategy |
|---|---|---|
| `tests/unit/test_skia_canvas.cc` | Low — all tests pass `nullptr` to init, none create a real GPU context | ✅ No changes needed structurally. Compile-only verification |
| `tests/unit/test_graphics_engine.cc` | None — tests GraphicsEngine, not SkiaCanvas | ✅ No changes needed |
| `tests/unit/test_render_loop.cc` | None — RenderLoop calls `canvas_->flush()` through public API | ✅ No changes needed (flush API stays) |
| `tests/unit/test_widget.cc` | None — widgets call canvas draw methods | ✅ No changes needed |

**Verification flow:**
1. `cmake --build --preset debug` — all targets compile (Ganesh→Graphite migration)
2. `ctest --preset debug --output-on-failure` — all tests pass
3. Manual: run `aria` binary and verify rendering works

**New test consideration:** A Graphite-specific init test that validates `Context::MakeDawn` creates a valid context (similar to existing `GraphicsEngine::init()` tests).

---

### Risks

| Risk | Severity | Mitigation |
|---|---|---|
| **1. `GraphicsEngine` doesn't expose `wgpu::Queue`** | HIGH | Add `wgpu::Queue queue` member and `device.GetQueue()` in engine init; expose via accessor or direct struct copy |
| **2. Graphite `DawnBackendContext` requires `wgpu::Instance`** | MEDIUM | `GraphicsEngine::Impl` already stores `wgpu::Instance instance` — just needs to be exposed |
| **3. `skia_enable_graphite=true` may increase build time** | LOW | Already set, no change |
| **4. Ganesh `GrDawnBackendContext` has different struct layout** | MEDIUM | Confirmed — Graphite's version has `fInstance` and `fTick` that Ganesh didn't require. Code must be updated |
| **5. `recorder->snap()` + `insertRecording()` + `submit()` changes flush semantics** | MEDIUM | Flush is now 3-step. Must verify no frame drops or sync issues |
| **6. Graphite may have different performance characteristics** | LOW | Monitor frame timing in CI |
| **7. Existing `skia_enable_graphite=true` may fail if Dawn deps incomplete** | LOW | `DAWN_FETCH_DEPENDENCIES=OFF` and vendored Dawn appears to have deps populated |
| **8. `gn` not installed on user machine** | HIGH | User confirmed `gn` is NOT installed. Skia build target in `FindSkiaFetch.cmake` requires `gn`. Must install or have Skia pre-built. Ninja 1.13 IS available |
| **9. MSVC compatibility with Graphite** | LOW | Skia chrome/m126 builds Graphite with MSVC on Windows — no known issues |

---

### Effort Estimation

| File | Est. Time | Complexity | Dependencies |
|---|---|---|---|
| `cmake/FindSkiaFetch.cmake` (include dirs) | 10 min | Trivial | None |
| `src/graphics/graphics_engine.h` (queue exposure) | 15 min | Low | None |
| `src/graphics/graphics_engine.cc` (queue init) | 20 min | Low | None |
| `src/graphics/skia_canvas.h` (forward decls) | 10 min | Trivial | Phase 1-2 done |
| `src/graphics/skia_canvas.cc` (core migration) | 2-3 hours | **HIGH** | Phases 1-2 done; understanding Graphite lifecycle |
| `tests/unit/test_skia_canvas.cc` (updates) | 15 min | Low | Phase 3-4 done |
| **Total** | **~3-4 hours** | Medium-High | All phases sequential |

---

### Recommendation

**Proceed with migration.** The scope is tightly bounded to a single file (`skia_canvas.cc`) plus minor supporting changes. Graphite is the only viable Skia GPU backend for `chrome/m126` and above.

**Architecture Decision:** Keep the `GraphicsEngine` as the sole owner of Dawn objects (instance, device, queue). The `SkiaCanvas::init()` should copy the necessary Dawn handles from `GraphicsEngine` into the `graphite::DawnBackendContext` struct rather than exposing raw pointers widely.

**Rollback plan:** Keep Ganesh code behind a compile flag or branch — but since Skia `chrome/m126` doesn't ship Ganesh+Dawn, rollback requires downgrading Skia. Recommend a clean cutover with a well-tested PR.

---

### Ready for Proposal

**Yes.** Exploration is complete. All Ganesh API usage is identified, Graphite equivalents are mapped, headers are confirmed present, and risks are assessed.

Next step: orchestrator should launch `sdd-propose` for change `p10-skia-graphite-migration`.
