# Proposal: P10 — Skia Ganesh→Graphite Migration

## Intent

Migrate ARIA's Skia GPU backend from Ganesh (deprecated, removed in chrome/m126) to Graphite — the only viable Skia GPU backend in the vendored Skia version. Full behavioral parity required before completion.

## Scope

### In Scope
- Replace Ganesh Dawn APIs with Graphite equivalents in `skia_canvas.h/.cc`
- Expose `wgpu::Queue` from `GraphicsEngine` for Graphite's `DawnBackendContext`
- Update `FindSkiaFetch.cmake` include dirs from `ganesh/` to `graphite/`
- Adjust test structure expectations if type changes break compilation
- Full parity — all rendering, flush, and presentation behavior identical to pre-migration

### Out of Scope
- Performance tuning or optimization of the Graphite pipeline
- Adding new GPU features (compute shaders, new surface types)
- Changing the public `SkiaCanvas` API surface
- Migrating other Skia backends (CPU, Vulkan, Metal)

## Capabilities

### New Capabilities
None — behavioral contract is unchanged.

### Modified Capabilities
None — requirements do not change (full parity). Implementation-only migration.

## Approach

Per-file migration following exploration phases:

1. **FindSkiaFetch.cmake**: Replace `ganesh/` include dirs with `graphite/`
2. **GraphicsEngine**: Store `wgpu::Queue` from `device.GetQueue()`, expose via accessor
3. **skia_canvas.h/.cc**: Replace `GrDirectContext`→`Context`, `GrDawnBackendContext`→`DawnBackendContext`, add Recorder, migrate flush/submit to `snap→insertRecording→submit`
4. **Tests**: Compile-only verification across all test targets; optional Graphite init test
5. **Build + verify**: Full CMake build + `ctest --preset debug --output-on-failure`

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/graphics/skia_canvas.cc` | Modified | Core migration (8 Ganesh references) |
| `src/graphics/skia_canvas.h` | Modified | Forward decls, comments |
| `src/graphics/graphics_engine.h/.cc` | Modified | Expose `wgpu::Queue` |
| `cmake/FindSkiaFetch.cmake` | Modified | Graphite include dirs |
| `tests/unit/test_skia_canvas.cc` | Modified | Structure type updates |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| `gn` not installed for Skia build | High | Install GN via package manager or use pre-built Skia |
| Graphite init requires queue not exposed | Med | Add `device.GetQueue()` in engine init |
| Three-step flush changes sync semantics | Med | Match `SyncToCpu::kNo` behavior; verify with ctest suite |

## Rollback Plan

Keep Ganesh code on a separate branch. Since vendored Skia chrome/m126 lacks Ganesh+Dawn, true rollback requires downgrading Skia or reverting the PR fully.

## Dependencies

- **GN** (build tool for Skia) — not currently installed
- Vendored Skia at `vendor/skia/` — Graphite headers already present
- Vendored Dawn at `vendor/dawn/` — no build changes required

## Success Criteria

- [ ] Full CMake build succeeds with Graphite (zero Ganesh references remaining)
- [ ] All existing `ctest --preset debug` tests pass (full parity verified)
- [ ] Manual rendering test confirms visual output matches pre-migration
