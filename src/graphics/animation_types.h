#ifndef ARIA_ANIMATION_TYPES_H
#define ARIA_ANIMATION_TYPES_H

#include "graphics/graphics_types.h"

#include <functional>

namespace aria {

// ── EasingCurve ───────────────────────────────────────────────────

/// Enum mapping to easing functions in `easing.h`.
enum class EasingCurve {
    kLinear,

    kEaseInQuad,
    kEaseOutQuad,
    kEaseInOutQuad,

    kEaseInCubic,
    kEaseOutCubic,
    kEaseInOutCubic,

    kEaseInQuart,
    kEaseOutQuart,
    kEaseInOutQuart,

    kEaseInQuint,
    kEaseOutQuint,
    kEaseInOutQuint,

    kEaseInSine,
    kEaseOutSine,
    kEaseInOutSine,

    kEaseInExpo,
    kEaseOutExpo,
    kEaseInOutExpo,

    kEaseInCirc,
    kEaseOutCirc,
    kEaseInOutCirc,

    kEaseInBack,
    kEaseOutBack,
    kEaseInOutBack,

    kEaseInElastic,
    kEaseOutElastic,
    kEaseInOutElastic,

    kEaseInBounce,
    kEaseOutBounce,
    kEaseInOutBounce,
};

// ── AnimatableProperty ────────────────────────────────────────────

/// Properties that can be animated on a widget.
enum class AnimatableProperty {
    kOpacity,
    kX,
    kY,
    kWidth,
    kHeight,
    kRotation,
    kScaleX,
    kScaleY,
};

// ── Animation ─────────────────────────────────────────────────────

/// Describes a single animation for one widget property.
///
/// Movable but not copyable. Ownership is transferred to the
/// Animator when `Animator::start()` is called.
struct Animation {
    WidgetID            target_id = kInvalidWidgetID;
    AnimatableProperty  property = AnimatableProperty::kOpacity;
    float               from = 0.0f;
    float               to = 0.0f;
    float               duration = 0.0f;   ///< In seconds
    EasingCurve         easing = EasingCurve::kLinear;
    std::function<void()> on_complete;

    // Internal: elapsed time tracked by Animator
    float               elapsed = 0.0f;

    Animation() = default;
    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;
    Animation(Animation&&) = default;
    Animation& operator=(Animation&&) = default;
};

} // namespace aria

#endif // ARIA_ANIMATION_TYPES_H
