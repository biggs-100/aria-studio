# Proposal: P10c Animation Engine

## Intent

GPU-side interpolation engine for widget transitions and animations. Currently P10a's RenderLoop drives frame timing but does not interpolate any widget property over time — no opacity fades, no transform transitions, no built-in easing.

## Scope

### In Scope
- 30+ easing functions (CSS standard + spec extras)
- Animation struct (target, duration, easing, onComplete)
- Animator class (tick, manage active animations lifecycle)
- RenderLoop integration: tick animations before paint pass
- Dirty-mark completed animations via DirtyTracker

### Out of Scope
- Animation composer / sequence chaining
- Declarative transition system (value-change interpolation)
- Keyframe animation support
- TS-side Animator (future P11)

## Capabilities

### New Capabilities
- `animation-core`: Easing math library, Animation struct, Animator lifecycle manager
- `animation-render-integration`: RenderLoop hooking, dirty-mark propagation

### Modified Capabilities
- `render-loop`: New requirement — tick Animator each frame before paint
- `gpu-widget-system`: New requirement — dirty-mark on animation complete

## Approach

Pure math header (`easing.h`) with 30+ curve implementations. `Animation` struct owns target widget ID, interpolated property, duration, easing enum, completion callback. `Animator` class tracks active animations in a vector, advances them on `tick(dt)`, removes completed, fires callbacks. RenderLoop calls `Animator::tick()` before the paint pass. Completed animations mark affected widgets dirty via existing DirtyTracker.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/graphics/easing.h` | New | 30+ easing curve implementations |
| `src/graphics/animation_types.h` | New | Animation struct, Easing enum |
| `src/graphics/animator.h/.cc` | New | Animator class with tick/lifecycle |
| `src/graphics/render_loop.cc` | Modified | Call `Animator::tick()` each frame |
| `src/graphics/widget.h` | Modified | Add `animate()` hook for dirty-mark |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Animating widgets dirties entire tree each frame | Low | Animator returns `Set<WidgetID>`; only changed widgets marked dirty |

## Rollback Plan

Revert RenderLoop integration commit. Animator and easing are additive — no existing code depends on them. Widget dirty-mark change is isolated to `widget.h` — revert that single header.

## Dependencies

- RenderLoop (existing, P10a)
- DirtyTracker (existing, P10a)

## Success Criteria

- [ ] 30+ easing functions return correct values for t=[0,1], verified by unit tests
- [ ] Animator completes an Animation and fires onComplete
- [ ] RenderLoop ticks Animator each frame before paint pass
- [ ] Widget is marked dirty when its animation completes
