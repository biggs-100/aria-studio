#ifndef ARIA_GRAPHICS_TYPES_H
#define ARIA_GRAPHICS_TYPES_H

#include <cstdint>
#include <string>

namespace aria {

// ── Geometry ─────────────────────────────────────────────────────

/// 2D rectangle with position and size.
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float left()   const noexcept { return x; }
    float top()    const noexcept { return y; }
    float right()  const noexcept { return x + w; }
    float bottom() const noexcept { return y + h; }

    bool contains(float px, float py) const noexcept {
        return px >= left() && px < right() && py >= top() && py < bottom();
    }

    bool intersects(const Rect& other) const noexcept {
        return left() < other.right() && right() > other.left() &&
               top() < other.bottom() && bottom() > other.top();
    }
};

// ── Color ────────────────────────────────────────────────────────

/// RGBA colour with components in [0, 1].
struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    inline static const Color Black       = {0.0f, 0.0f, 0.0f, 1.0f};
    inline static const Color White       = {1.0f, 1.0f, 1.0f, 1.0f};
    inline static const Color Transparent = {0.0f, 0.0f, 0.0f, 0.0f};
    inline static const Color Red         = {1.0f, 0.0f, 0.0f, 1.0f};
    inline static const Color Blue        = {0.0f, 0.0f, 1.0f, 1.0f};

    static Color from_rgba8(uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa = 255) {
        return {
            rr / 255.0f,
            gg / 255.0f,
            bb / 255.0f,
            aa / 255.0f,
        };
    }
};

// ── Paint ────────────────────────────────────────────────────────

/// Describes how a shape is filled and stroked.
struct Paint {
    Color fill        = Color::White;
    Color stroke      = Color::Transparent;
    float stroke_width = 0.0f;
    float corner_radius = 0.0f;  ///< For rounded rects; 0 = sharp.
};

// ── Vec2 ─────────────────────────────────────────────────────────

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// ── WidgetID ─────────────────────────────────────────────────────

using WidgetID = uint64_t;

inline constexpr WidgetID kInvalidWidgetID = 0;

} // namespace aria

#endif // ARIA_GRAPHICS_TYPES_H
