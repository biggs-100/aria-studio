#include "graphics/ffi/event_dispatcher.h"

#include <algorithm>

namespace aria {
namespace ffi {

// =====================================================================
// Construction
// =====================================================================

EventDispatcher::EventDispatcher() = default;

// =====================================================================
// Callback registration
// =====================================================================

void EventDispatcher::on_event(EventCallback cb) {
    callback_ = std::move(cb);
}

// =====================================================================
// HiDPI
// =====================================================================

void EventDispatcher::set_device_pixel_ratio(float ratio) {
    dpr_ = std::max(ratio, 0.1f);
}

float EventDispatcher::device_pixel_ratio() const noexcept {
    return dpr_;
}

float EventDispatcher::to_logical(float phys) const {
    return phys / dpr_;
}

// =====================================================================
// Event emission
// =====================================================================

void EventDispatcher::emit(const InputEvent& ev) {
    if (callback_) {
        callback_(ev);
    }
}

// =====================================================================
// Mouse events
// =====================================================================

void EventDispatcher::dispatch_mouse_move(float phys_x, float phys_y) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type = EventType::kMouseMove;
    ev.x    = log_x;
    ev.y    = log_y;
    ev.button = 0;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
}

void EventDispatcher::dispatch_mouse_down(float phys_x, float phys_y, int button) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type   = EventType::kMouseDown;
    ev.x      = log_x;
    ev.y      = log_y;
    ev.button = button;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
    dragging_ = true;
}

void EventDispatcher::dispatch_mouse_up(float phys_x, float phys_y, int button) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type   = EventType::kMouseUp;
    ev.x      = log_x;
    ev.y      = log_y;
    ev.button = button;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
    dragging_ = false;
}

// =====================================================================
// Keyboard events
// =====================================================================

void EventDispatcher::dispatch_key_down(int key_code, int modifiers) {
    InputEvent ev;
    ev.type     = EventType::kKeyDown;
    ev.key_code = key_code;
    ev.shift    = (modifiers & 1) != 0;
    ev.ctrl     = (modifiers & 2) != 0;
    ev.alt      = (modifiers & 4) != 0;
    ev.meta     = (modifiers & 8) != 0;

    emit(ev);
}

void EventDispatcher::dispatch_key_up(int key_code, int modifiers) {
    InputEvent ev;
    ev.type     = EventType::kKeyUp;
    ev.key_code = key_code;
    ev.shift    = (modifiers & 1) != 0;
    ev.ctrl     = (modifiers & 2) != 0;
    ev.alt      = (modifiers & 4) != 0;
    ev.meta     = (modifiers & 8) != 0;

    emit(ev);
}

// =====================================================================
// Touch events
// =====================================================================

void EventDispatcher::dispatch_touch_down(float phys_x, float phys_y, int touch_id) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type          = EventType::kTouchDown;
    ev.x             = log_x;
    ev.y             = log_y;
    ev.touch_id      = touch_id;
    ev.is_touch_or_pen = true;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
}

void EventDispatcher::dispatch_touch_move(float phys_x, float phys_y, int touch_id) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type          = EventType::kTouchMove;
    ev.x             = log_x;
    ev.y             = log_y;
    ev.touch_id      = touch_id;
    ev.is_touch_or_pen = true;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
}

void EventDispatcher::dispatch_touch_up(float phys_x, float phys_y, int touch_id) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type          = EventType::kTouchUp;
    ev.x             = log_x;
    ev.y             = log_y;
    ev.touch_id      = touch_id;
    ev.is_touch_or_pen = true;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
}

// =====================================================================
// Pen events
// =====================================================================

void EventDispatcher::dispatch_pen_down(float phys_x, float phys_y,
                                         float pressure, float tilt_x, float tilt_y,
                                         bool barrel_button) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type          = EventType::kPenDown;
    ev.x             = log_x;
    ev.y             = log_y;
    ev.pressure      = pressure;
    ev.tilt_x        = tilt_x;
    ev.tilt_y        = tilt_y;
    ev.barrel_button = barrel_button;
    ev.is_touch_or_pen = true;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
}

void EventDispatcher::dispatch_pen_move(float phys_x, float phys_y,
                                         float pressure, float tilt_x, float tilt_y,
                                         bool barrel_button) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type          = EventType::kPenMove;
    ev.x             = log_x;
    ev.y             = log_y;
    ev.pressure      = pressure;
    ev.tilt_x        = tilt_x;
    ev.tilt_y        = tilt_y;
    ev.barrel_button = barrel_button;
    ev.is_touch_or_pen = true;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
}

void EventDispatcher::dispatch_pen_up(float phys_x, float phys_y,
                                       float pressure, float tilt_x, float tilt_y,
                                       bool barrel_button) {
    float log_x = to_logical(phys_x);
    float log_y = to_logical(phys_y);

    InputEvent ev;
    ev.type          = EventType::kPenUp;
    ev.x             = log_x;
    ev.y             = log_y;
    ev.pressure      = pressure;
    ev.tilt_x        = tilt_x;
    ev.tilt_y        = tilt_y;
    ev.barrel_button = barrel_button;
    ev.is_touch_or_pen = true;

    emit(ev);

    last_phys_x_ = phys_x;
    last_phys_y_ = phys_y;
}

} // namespace ffi
} // namespace aria
