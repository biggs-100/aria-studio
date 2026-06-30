#ifndef ARIA_INPUT_TYPES_H
#define ARIA_INPUT_TYPES_H

#include <cstdint>

namespace aria {

// ── TouchEvent ────────────────────────────────────────────────────

/// Multi-touch event data.
struct TouchEvent {
    float x       = 0.0f;  ///< Logical pixel X
    float y       = 0.0f;  ///< Logical pixel Y
    int   touch_id = 0;    ///< Unique touch-point identifier for multi-touch tracking
};

// ── PenEvent ──────────────────────────────────────────────────────

/// Stylus / pen event data.
struct PenEvent {
    float x           = 0.0f;  ///< Logical pixel X
    float y           = 0.0f;  ///< Logical pixel Y
    float pressure    = 0.0f;  ///< 0.0–1.0
    float tilt_x      = 0.0f;  ///< Tilt angle in degrees
    float tilt_y      = 0.0f;  ///< Tilt angle in degrees
    bool  barrel_button = false;
};

} // namespace aria

#endif // ARIA_INPUT_TYPES_H
