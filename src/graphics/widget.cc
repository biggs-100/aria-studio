#include "graphics/widget.h"
#include "graphics/skia_canvas.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <utility>

namespace aria {

// ── Static ID generator ──────────────────────────────────────────

namespace {
    std::atomic<WidgetID> g_next_widget_id{kInvalidWidgetID + 1};
}

WidgetID Widget::next_id() {
    return g_next_widget_id.fetch_add(1, std::memory_order_relaxed);
}

// =====================================================================
// Widget — Lifecycle
// =====================================================================

Widget::Widget()
    : id_(next_id()) {}

Widget::~Widget() = default;

// =====================================================================
// Widget — Tree
// =====================================================================

void Widget::add_child(std::unique_ptr<Widget> child) {
    if (!child) return;

    // If child already has a parent, remove it first
    if (child->parent_) {
        // Find and release from current parent
        auto& siblings = child->parent_->children_;
        auto it = std::find_if(siblings.begin(), siblings.end(),
            [ptr = child.get()](const auto& c) { return c.get() == ptr; });
        if (it != siblings.end()) {
            // Release ownership without destroying
            child->parent_ = nullptr;
            // Move unique_ptr out, then put back into this
            auto released = std::move(*it);
            siblings.erase(it);
            child = std::move(released);
        }
    }

    child->parent_ = this;
    mark_dirty();
    children_.push_back(std::move(child));
}

std::unique_ptr<Widget> Widget::remove_child(Widget* child) {
    if (!child) return nullptr;

    auto it = std::find_if(children_.begin(), children_.end(),
        [child](const auto& c) { return c.get() == child; });

    if (it == children_.end()) return nullptr;

    auto released = std::move(*it);
    released->parent_ = nullptr;
    children_.erase(it);
    mark_dirty();
    return released;
}

Widget* Widget::child_at(size_t index) const noexcept {
    if (index >= children_.size()) return nullptr;
    return children_[index].get();
}

// =====================================================================
// Widget — Layout
// =====================================================================

void Widget::arrange(float x, float y, float w, float h) {
    bounds_.x = x;
    bounds_.y = y;
    bounds_.w = w;
    bounds_.h = h;
    mark_dirty();
}

// =====================================================================
// Widget — Paint
// =====================================================================

void Widget::paint(SkiaCanvas* canvas) {
    if (!canvas || !visible_) return;

    render(canvas);
    render_children(canvas);
    dirty_ = false;
}

void Widget::render_children(SkiaCanvas* canvas) {
    if (!canvas) return;

    for (auto& child : children_) {
        if (child->visible_) {
            child->paint(canvas);
        }
    }
}

// =====================================================================
// Widget — Hit testing
// =====================================================================

Widget* Widget::hit_test(float px, float py) {
    if (!visible_ || !enabled_) return nullptr;
    if (!contains_point(px, py)) return nullptr;

    // Reverse iteration: last child is visually topmost
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        auto* hit = (*it)->hit_test(px, py);
        if (hit) return hit;
    }

    return this;
}

bool Widget::contains_point(float px, float py) const {
    return bounds_.contains(px, py);
}

// =====================================================================
// Widget — Tree traversal
// =====================================================================

Widget* Widget::find_by_id(WidgetID id) noexcept {
    if (id_ == id) return this;
    for (auto& child : children_) {
        auto* found = child->find_by_id(id);
        if (found) return found;
    }
    return nullptr;
}

const Widget* Widget::find_by_id(WidgetID id) const noexcept {
    if (id_ == id) return this;
    for (const auto& child : children_) {
        auto* found = child->find_by_id(id);
        if (found) return found;
    }
    return nullptr;
}

// =====================================================================
// Widget — Animation
// =====================================================================

void Widget::set_animated_property(AnimatableProperty prop, float value) noexcept {
    auto idx = static_cast<size_t>(prop);
    if (idx < animated_values_.size()) {
        animated_values_[idx] = value;
        mark_dirty();
    }
}

float Widget::animated_value(AnimatableProperty prop) const noexcept {
    auto idx = static_cast<size_t>(prop);
    if (idx < animated_values_.size()) {
        return animated_values_[idx];
    }
    return 0.0f;
}

// =====================================================================
// Widget — Accessibility
// =====================================================================

AccessibleRole Widget::effective_role() const {
    if (accessible_role_ != AccessibleRole::kNone) {
        return accessible_role_;
    }
    if (parent_) {
        return parent_->effective_role();
    }
    return AccessibleRole::kNone;
}

// =====================================================================
// Widget — Focus
// =====================================================================

void Widget::set_focused(bool focused) {
    if (is_focused_ == focused) return;
    is_focused_ = focused;
    if (focused) {
        if (on_focus_callback_) on_focus_callback_();
    } else {
        if (on_blur_callback_) on_blur_callback_();
    }
}

} // namespace aria
