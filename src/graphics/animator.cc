#include "graphics/animator.h"
#include "graphics/easing.h"

#include <algorithm>
#include <cassert>

namespace aria {

// ── Easing dispatch table ────────────────────────────────────────
//
// Maps EasingCurve enum values to the corresponding easing function.
// Using a function pointer table avoids branching per animation tick.

using EasingFn = float (*)(float);

namespace {

EasingFn select_easing(EasingCurve curve) noexcept {
    switch (curve) {
        case EasingCurve::kLinear:          return ease_linear;

        case EasingCurve::kEaseInQuad:      return ease_in_quad;
        case EasingCurve::kEaseOutQuad:     return ease_out_quad;
        case EasingCurve::kEaseInOutQuad:   return ease_in_out_quad;

        case EasingCurve::kEaseInCubic:     return ease_in_cubic;
        case EasingCurve::kEaseOutCubic:    return ease_out_cubic;
        case EasingCurve::kEaseInOutCubic:  return ease_in_out_cubic;

        case EasingCurve::kEaseInQuart:     return ease_in_quart;
        case EasingCurve::kEaseOutQuart:    return ease_out_quart;
        case EasingCurve::kEaseInOutQuart:  return ease_in_out_quart;

        case EasingCurve::kEaseInQuint:     return ease_in_quint;
        case EasingCurve::kEaseOutQuint:    return ease_out_quint;
        case EasingCurve::kEaseInOutQuint:  return ease_in_out_quint;

        case EasingCurve::kEaseInSine:      return ease_in_sine;
        case EasingCurve::kEaseOutSine:     return ease_out_sine;
        case EasingCurve::kEaseInOutSine:   return ease_in_out_sine;

        case EasingCurve::kEaseInExpo:      return ease_in_expo;
        case EasingCurve::kEaseOutExpo:     return ease_out_expo;
        case EasingCurve::kEaseInOutExpo:   return ease_in_out_expo;

        case EasingCurve::kEaseInCirc:      return ease_in_circ;
        case EasingCurve::kEaseOutCirc:     return ease_out_circ;
        case EasingCurve::kEaseInOutCirc:   return ease_in_out_circ;

        case EasingCurve::kEaseInBack:      return ease_in_back;
        case EasingCurve::kEaseOutBack:     return ease_out_back;
        case EasingCurve::kEaseInOutBack:   return ease_in_out_back;

        case EasingCurve::kEaseInElastic:   return ease_in_elastic;
        case EasingCurve::kEaseOutElastic:  return ease_out_elastic;
        case EasingCurve::kEaseInOutElastic: return ease_in_out_elastic;

        case EasingCurve::kEaseInBounce:    return ease_in_bounce;
        case EasingCurve::kEaseOutBounce:   return ease_out_bounce;
        case EasingCurve::kEaseInOutBounce: return ease_in_out_bounce;
    }
    // Should never reach here — all cases covered.
    assert(false && "Unknown EasingCurve value");
    return ease_linear;
}

} // anonymous namespace

// =====================================================================
// Public API
// =====================================================================

void Animator::start(Animation anim) {
    anim.elapsed = 0.0f;
    animations_.push_back(std::move(anim));
}

void Animator::tick(float dt) {
    // Always clear completed lists at the start of each tick,
    // even when no animations are active (ensures consumed state).
    completed_.clear();
    completed_cache_.clear();

    if (animations_.empty()) {
        return; // fast no-op path
    }

    // Advance each animation; collect completions
    for (auto& anim : animations_) {
        anim.elapsed += dt;

        if (anim.elapsed >= anim.duration && anim.duration > 0.0f) {
            // Animation completed — cache final value before removal
            completed_.push_back(anim.target_id);
            completed_cache_.push_back({anim.target_id, anim.property, anim.to});

            if (anim.on_complete) {
                anim.on_complete();
            }
        }
    }

    // Remove completed animations
    auto is_completed = [](const Animation& a) {
        return a.elapsed >= a.duration && a.duration > 0.0f;
    };

    animations_.erase(
        std::remove_if(animations_.begin(), animations_.end(), is_completed),
        animations_.end()
    );
}

void Animator::cancel(WidgetID id) {
    animations_.erase(
        std::remove_if(animations_.begin(), animations_.end(),
            [id](const Animation& a) { return a.target_id == id; }),
        animations_.end()
    );
}

bool Animator::is_animating(WidgetID id) const noexcept {
    return std::any_of(animations_.begin(), animations_.end(),
        [id](const Animation& a) { return a.target_id == id; });
}

size_t Animator::active_count() const noexcept {
    return animations_.size();
}

const std::vector<WidgetID>& Animator::completed() const noexcept {
    return completed_;
}

float Animator::current_value(WidgetID id, AnimatableProperty prop) const noexcept {
    // 1. Search active animations first
    for (const auto& anim : animations_) {
        if (anim.target_id == id && anim.property == prop) {
            float t = std::min(anim.elapsed / anim.duration, 1.0f);
            auto easing_fn = select_easing(anim.easing);
            float eased = easing_fn(t);
            return anim.from + (anim.to - anim.from) * eased;
        }
    }

    // 2. Fallback: check completions from the most recent tick
    for (const auto& entry : completed_cache_) {
        if (entry.id == id && entry.prop == prop) {
            return entry.final_value;
        }
    }

    // 3. No animation found
    return 0.0f;
}

} // namespace aria
