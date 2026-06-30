#ifndef ARIA_EASING_H
#define ARIA_EASING_H

#include <cmath>

namespace aria {
namespace easing {

// ── Linear ─────────────────────────────────────────────────────────

constexpr float ease_linear(float t) noexcept {
    return t;
}

// ── Quadratic ──────────────────────────────────────────────────────

constexpr float ease_in_quad(float t) noexcept {
    return t * t;
}

constexpr float ease_out_quad(float t) noexcept {
    return t * (2.0f - t);
}

constexpr float ease_in_out_quad(float t) noexcept {
    if (t < 0.5f) {
        return 2.0f * t * t;
    }
    return -1.0f + (4.0f - 2.0f * t) * t;
}

// ── Cubic ──────────────────────────────────────────────────────────

constexpr float ease_in_cubic(float t) noexcept {
    return t * t * t;
}

constexpr float ease_out_cubic(float t) noexcept {
    float p = t - 1.0f;
    return p * p * p + 1.0f;
}

constexpr float ease_in_out_cubic(float t) noexcept {
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    }
    float p = 2.0f * t - 2.0f;
    return 0.5f * p * p * p + 1.0f;
}

// ── Quartic ────────────────────────────────────────────────────────

constexpr float ease_in_quart(float t) noexcept {
    return t * t * t * t;
}

constexpr float ease_out_quart(float t) noexcept {
    float p = t - 1.0f;
    return p * p * p * (1.0f - t) + 1.0f;
}

constexpr float ease_in_out_quart(float t) noexcept {
    if (t < 0.5f) {
        return 8.0f * t * t * t * t;
    }
    float p = t - 1.0f;
    return 1.0f - 8.0f * p * p * p * p;
}

// ── Quintic ────────────────────────────────────────────────────────

constexpr float ease_in_quint(float t) noexcept {
    return t * t * t * t * t;
}

constexpr float ease_out_quint(float t) noexcept {
    float p = t - 1.0f;
    return p * p * p * p * p + 1.0f;
}

constexpr float ease_in_out_quint(float t) noexcept {
    if (t < 0.5f) {
        return 16.0f * t * t * t * t * t;
    }
    float p = 2.0f * t - 2.0f;
    return 0.5f * p * p * p * p * p + 1.0f;
}

// ── Sine ───────────────────────────────────────────────────────────

inline float ease_in_sine(float t) noexcept {
    return 1.0f - std::cos(t * 3.14159265358979323846f * 0.5f);
}

inline float ease_out_sine(float t) noexcept {
    return std::sin(t * 3.14159265358979323846f * 0.5f);
}

inline float ease_in_out_sine(float t) noexcept {
    return 0.5f * (1.0f - std::cos(3.14159265358979323846f * t));
}

// ── Exponential ────────────────────────────────────────────────────

inline float ease_in_expo(float t) noexcept {
    if (t == 0.0f) return 0.0f;
    return std::pow(2.0f, 10.0f * (t - 1.0f));
}

inline float ease_out_expo(float t) noexcept {
    if (t == 1.0f) return 1.0f;
    return 1.0f - std::pow(2.0f, -10.0f * t);
}

inline float ease_in_out_expo(float t) noexcept {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    if (t < 0.5f) {
        return 0.5f * std::pow(2.0f, 20.0f * t - 10.0f);
    }
    return 0.5f * (2.0f - std::pow(2.0f, -20.0f * t + 10.0f));
}

// ── Circular ───────────────────────────────────────────────────────

inline float ease_in_circ(float t) noexcept {
    return 1.0f - std::sqrt(1.0f - t * t);
}

inline float ease_out_circ(float t) noexcept {
    float p = t - 1.0f;
    return std::sqrt(1.0f - p * p);
}

inline float ease_in_out_circ(float t) noexcept {
    if (t < 0.5f) {
        return 0.5f * (1.0f - std::sqrt(1.0f - 4.0f * t * t));
    }
    float p = -2.0f * t + 2.0f;
    return 0.5f * (std::sqrt(1.0f - p * p) + 1.0f);
}

// ── Back (with overshoot) ──────────────────────────────────────────

constexpr float kBackOvershoot = 1.70158f;

constexpr float ease_in_back(float t) noexcept {
    return t * t * ((kBackOvershoot + 1.0f) * t - kBackOvershoot);
}

constexpr float ease_out_back(float t) noexcept {
    float p = t - 1.0f;
    return p * p * ((kBackOvershoot + 1.0f) * p + kBackOvershoot) + 1.0f;
}

constexpr float ease_in_out_back(float t) noexcept {
    constexpr float c = kBackOvershoot * 1.525f;
    if (t < 0.5f) {
        float p = 2.0f * t;
        return 0.5f * (p * p * ((c + 1.0f) * p - c));
    }
    float p = 2.0f * t - 2.0f;
    return 0.5f * (p * p * ((c + 1.0f) * p + c) + 2.0f);
}

// ── Elastic ────────────────────────────────────────────────────────

inline float ease_in_elastic(float t) noexcept {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    constexpr float period = 0.3f;
    float s = period / 4.0f;
    float p = t - 1.0f;
    return -std::pow(2.0f, 10.0f * p) *
           std::sin((p - s) * (2.0f * 3.14159265358979323846f) / period);
}

inline float ease_out_elastic(float t) noexcept {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    constexpr float period = 0.3f;
    float s = period / 4.0f;
    return std::pow(2.0f, -10.0f * t) *
           std::sin((t - s) * (2.0f * 3.14159265358979323846f) / period) + 1.0f;
}

inline float ease_in_out_elastic(float t) noexcept {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    constexpr float period = 0.3f;
    float s = period / 4.0f;
    if (t < 0.5f) {
        float p = 2.0f * t - 1.0f;
        return -0.5f * std::pow(2.0f, 10.0f * p) *
               std::sin((p - s) * (2.0f * 3.14159265358979323846f) / period);
    }
    float p = 2.0f * t - 1.0f;
    return 0.5f * std::pow(2.0f, -10.0f * p) *
               std::sin((p - s) * (2.0f * 3.14159265358979323846f) / period) + 1.0f;
}

// ── Bounce ─────────────────────────────────────────────────────────

inline float ease_out_bounce(float t) noexcept {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    }
    if (t < 2.0f / 2.75f) {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    }
    if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

inline float ease_in_bounce(float t) noexcept {
    return 1.0f - ease_out_bounce(1.0f - t);
}

inline float ease_in_out_bounce(float t) noexcept {
    if (t < 0.5f) {
        return 0.5f * ease_in_bounce(t * 2.0f);
    }
    return 0.5f * ease_out_bounce(t * 2.0f - 1.0f) + 0.5f;
}

} // namespace easing

// ── Convenience aliases at aria:: namespace for brevity ─────────────

using easing::ease_linear;

using easing::ease_in_quad;
using easing::ease_out_quad;
using easing::ease_in_out_quad;

using easing::ease_in_cubic;
using easing::ease_out_cubic;
using easing::ease_in_out_cubic;

using easing::ease_in_quart;
using easing::ease_out_quart;
using easing::ease_in_out_quart;

using easing::ease_in_quint;
using easing::ease_out_quint;
using easing::ease_in_out_quint;

using easing::ease_in_sine;
using easing::ease_out_sine;
using easing::ease_in_out_sine;

using easing::ease_in_expo;
using easing::ease_out_expo;
using easing::ease_in_out_expo;

using easing::ease_in_circ;
using easing::ease_out_circ;
using easing::ease_in_out_circ;

using easing::ease_in_back;
using easing::ease_out_back;
using easing::ease_in_out_back;

using easing::ease_in_elastic;
using easing::ease_out_elastic;
using easing::ease_in_out_elastic;

using easing::ease_in_bounce;
using easing::ease_out_bounce;
using easing::ease_in_out_bounce;

} // namespace aria

#endif // ARIA_EASING_H
