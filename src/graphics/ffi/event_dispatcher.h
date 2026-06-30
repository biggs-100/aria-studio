#ifndef ARIA_FFI_EVENT_DISPATCHER_H
#define ARIA_FFI_EVENT_DISPATCHER_H

#include <algorithm>
#include <cstdint>
#include <functional>

namespace aria {
namespace ffi {

// ── EventType ─────────────────────────────────────────────────────

/// Types of input events forwarded from OS → TS.
enum class EventType {
    kMouseMove,
    kMouseDown,
    kMouseUp,
    kMouseDrag,
    kKeyDown,
    kKeyUp,
    kTouchDown,
    kTouchMove,
    kTouchUp,
    kPenDown,
    kPenMove,
    kPenUp,
};

// ── InputEvent ────────────────────────────────────────────────────

/// An input event dispatched to the TypeScript callback.
/// Coordinates are in LOGICAL pixels (HiDPI-scaled).
struct InputEvent {
    EventType type;
    float     x       = 0.0f;  ///< Logical pixel X
    float     y       = 0.0f;  ///< Logical pixel Y
    int       button  = 0;     ///< 0=left, 1=middle, 2=right
    int       key_code = 0;    ///< Platform key code
    int       touch_id = -1;   ///< Touch/pen point ID (-1 if not touch/pen)

    // Touch/pen extension data
    float pressure    = 0.0f;  ///< 0.0–1.0 (pen only)
    float tilt_x      = 0.0f;  ///< Tilt angle degrees (pen only)
    float tilt_y      = 0.0f;  ///< Tilt angle degrees (pen only)
    bool  barrel_button = false;  ///< Pen barrel button

    // Modifier state
    bool shift = false;
    bool ctrl  = false;
    bool alt   = false;
    bool meta  = false;

    /// Convenience flag: true for touch or pen events.
    bool is_touch_or_pen = false;
};

// ── EventCallback ─────────────────────────────────────────────────

/// Callback type for TypeScript event handlers.
using EventCallback = std::function<void(const InputEvent&)>;

// ── EventDispatcher ───────────────────────────────────────────────

/// Receives OS input events in physical pixels, applies HiDPI scaling,
/// and forwards them as logical-pixel InputEvents to the TS callback.
///
/// Design decisions:
///   - Coordinates are divided by device_pixel_ratio before dispatch.
///   - Modifier state is extracted from platform modifier bitmask.
///   - Events with no registered callback are silently dropped.
class EventDispatcher {
public:
    EventDispatcher();

    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // ── Callback registration ─────────────────────────────────

    /// Register the TypeScript event handler.
    void on_event(EventCallback cb);

    // ── Event dispatch ────────────────────────────────────────

    /// Dispatch a mouse-move event from OS physical coordinates.
    void dispatch_mouse_move(float phys_x, float phys_y);

    /// Dispatch a mouse-down event from OS physical coordinates.
    void dispatch_mouse_down(float phys_x, float phys_y, int button);

    /// Dispatch a mouse-up event from OS physical coordinates.
    void dispatch_mouse_up(float phys_x, float phys_y, int button);

    /// Dispatch a key-down event.
    void dispatch_key_down(int key_code, int modifiers);

    /// Dispatch a key-up event.
    void dispatch_key_up(int key_code, int modifiers);

    // ── Touch events ────────────────────────────────────────────

    /// Dispatch a touch-down event from OS physical coordinates.
    void dispatch_touch_down(float phys_x, float phys_y, int touch_id);

    /// Dispatch a touch-move event from OS physical coordinates.
    void dispatch_touch_move(float phys_x, float phys_y, int touch_id);

    /// Dispatch a touch-up event from OS physical coordinates.
    void dispatch_touch_up(float phys_x, float phys_y, int touch_id);

    // ── Pen events ──────────────────────────────────────────────

    /// Dispatch a pen-down event from OS physical coordinates.
    void dispatch_pen_down(float phys_x, float phys_y,
                           float pressure, float tilt_x, float tilt_y,
                           bool barrel_button);

    /// Dispatch a pen-move event from OS physical coordinates.
    void dispatch_pen_move(float phys_x, float phys_y,
                           float pressure, float tilt_x, float tilt_y,
                           bool barrel_button);

    /// Dispatch a pen-up event from OS physical coordinates.
    void dispatch_pen_up(float phys_x, float phys_y,
                         float pressure, float tilt_x, float tilt_y,
                         bool barrel_button);

    // ── HiDPI ─────────────────────────────────────────────────

    void set_device_pixel_ratio(float ratio);

    float device_pixel_ratio() const noexcept;

private:
    EventCallback callback_;
    float dpr_ = 1.0f;

    // Last mouse position (for drag tracking)
    float last_phys_x_ = 0.0f;
    float last_phys_y_ = 0.0f;
    bool  dragging_    = false;

    /// Convert physical coordinates to logical.
    float to_logical(float phys) const;

    /// Emit an event to the TS callback (if registered).
    void emit(const InputEvent& ev);
};

} // namespace ffi
} // namespace aria

#endif // ARIA_FFI_EVENT_DISPATCHER_H
