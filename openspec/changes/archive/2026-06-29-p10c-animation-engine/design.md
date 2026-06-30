# Design: P10c Animation Engine

## Technical Approach

Header-only easing math library plus an `Animator` lifecycle manager. The `Animator` lives as a standalone object (registered via `ServiceLocator`), and `RenderLoop::tick()` calls `Animator::tick(dt)` between the layout and paint passes. Completed animations collect affected `WidgetID`s and push them to `DirtyTracker`.

Ref: Specs `animation-core` and `animation-render-integration`.

## Architecture Decisions

| Decision | Choice | Alternatives | Rationale |
|----------|--------|-------------|-----------|
| Easing library | **Header-only `easing.h`** | .cc with math | Zero setup, all constexpr, no link deps — matches project's header-first style |
| Animation struct | **Plain struct, movable** | OOP Animation base | Value semantics for ownership transfer; `on_complete` via `std::function` |
| Animator storage | **`std::vector<Animation>`** | Priority queue, intrusive list | Small N (rarely > 10 active), linear scan for cancel() is fine |
| Integration point | **RenderLoop calls Animator::tick()** | Dedicated animation thread | Same-thread means no locking; animation state is frame-aligned with layout/paint |
| Dirty notification | **Animator returns completed set** | Widget self-marks | Single source of truth avoids missing marks when Animator removes an animation |

## Data Flow

```
RenderLoop::tick()
    │
    ├─ 1. Process input
    ├─ 2. Update state
    ├─ 3. Animator::tick(dt)  ← NEW
    │       │
    │       ├─ Advance each animation by dt
    │       ├─ Interpolate: value = lerp(from, to, ease(t))
    │       ├─ Fire on_complete when t ≥ 1.0
    │       └─ Collect completed WidgetIDs
    │
    ├─ 4. Mark completed widgets dirty via DirtyTracker
    ├─ 5. Layout pass
    ├─ 6. Paint pass (skips clean widgets)
    └─ 7. Present
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/graphics/easing.h` | Create | 30+ constexpr easing functions (linear, quad, cubic, quart, quint, sine, expo, circ, elastic, back, bounce × in/out/inOut) |
| `src/graphics/animation_types.h` | Create | `Animation` struct, `AnimatableProperty` enum, `EasingCurve` enum |
| `src/graphics/animator.h` | Create | `Animator` class — `start()`, `tick(dt)`, `cancel()`, `is_animating()` |
| `src/graphics/animator.cc` | Create | Animator implementation |
| `src/graphics/render_loop.h` | Modify | Add `set_animator(Animator*)` setter |
| `src/graphics/render_loop.cc` | Modify | Call `animator_->tick(dt)` before paint pass; mark dirty on completion |
| `src/graphics/widget.h` | Modify | Add `set_animated_property()` and `animated_value()` for property interpolation |

## Interfaces / Contracts

```cpp
// easing.h — all functions are constexpr, float → float, t in [0, 1]
constexpr float ease_linear(float t);
constexpr float ease_in_quad(float t);    constexpr float ease_out_quad(float t);    constexpr float ease_in_out_quad(float t);
constexpr float ease_in_cubic(float t);   constexpr float ease_out_cubic(float t);   constexpr float ease_in_out_cubic(float t);
// … 30+ variants

// animation_types.h
enum class EasingCurve { kLinear, kEaseInQuad, kEaseOutQuad, /* … */ kEaseInOutBounce };
enum class AnimatableProperty { kOpacity, kX, kY, kWidth, kHeight, kRotation, kScaleX, kScaleY };

struct Animation {
    WidgetID            target_id;
    AnimatableProperty  property;
    float               from = 0.0f, to = 0.0f;
    float               duration = 0.0f;       // seconds
    EasingCurve         easing = EasingCurve::kLinear;
    std::function<void()> on_complete;
    // … internal: elapsed time
    Animation() = default;
    Animation(Animation&&) = default;
    Animation& operator=(Animation&&) = default;
};

// animator.h
class Animator {
public:
    void start(Animation anim);
    void tick(float dt);                          // returns set of completed IDs
    void cancel(WidgetID id);
    bool is_animating(WidgetID id) const;
    const std::vector<WidgetID>& completed() const noexcept;
};
```

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Unit | Each easing function returns 0 at t=0, 1 at t=1 | Direct call with known inputs; verify float equality within epsilon |
| Unit | Monotonicity for in-variants | Sample t ∈ {0, 0.25, 0.5, 0.75, 1} and assert non-decreasing |
| Unit | Animator::tick advances, fires callback, removes completed | Start 0.5s anim, tick(0.5), verify callback called and `is_animating` false |
| Unit | Animator::cancel removes by WidgetID | Start 2 anims for same ID, cancel, verify both removed |
| Integration | RenderLoop tick calls Animator before paint | Mock Animator, inject into RenderLoop, verify tick order in frame cycle |

## Migration / Rollout

No migration required. All new code is additive — existing widgets are unaffected until they call `Animator::start()`.

## Open Questions

- [ ] Should `Animator` hold a `DirtyTracker*` directly or return completed IDs for the caller to mark? (Returning IDs is more flexible — RenderLoop does the marking.)
