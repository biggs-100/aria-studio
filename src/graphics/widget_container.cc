#include "graphics/widget_container.h"
#include "graphics/skia_canvas.h"

namespace aria {

ContainerWidget::ContainerWidget() = default;
ContainerWidget::~ContainerWidget() = default;

Vec2 ContainerWidget::measure(float available_width, float available_height) {
    float max_w = 0.0f;
    float max_h = 0.0f;

    for (const auto& child : children()) {
        Vec2 pref = child->measure(available_width, available_height);
        max_w = std::max(max_w, pref.x);
        max_h = std::max(max_h, pref.y);
    }

    Vec2 result = {available_width, available_height};
    set_preferred_size(result);
    return result;
}

void ContainerWidget::arrange(float x, float y, float w, float h) {
    Widget::arrange(x, y, w, h);

    // Arrange children within the container (minus scroll offset)
    // Use cached preferred sizes from measure pass — no re-measuring.
    for (auto& child : children()) {
        float child_x = x - scroll_x_;
        float child_y = y - scroll_y_;
        Vec2 pref = child->preferred_size();
        child->arrange(child_x, child_y,
                       pref.x > 0.0f ? pref.x : w,
                       pref.y > 0.0f ? pref.y : h);
    }
}

void ContainerWidget::render(SkiaCanvas* canvas) {
    if (!canvas || !is_visible()) return;

    // Draw themed background if a theme path is set
    if (theme_bg_path_.has_value()) {
        Paint bg;
        bg.fill = resolve_color(*theme_bg_path_);
        canvas->drawRect(bounds(), bg);
    }

    // Push clip rect if clipping is enabled
    if (clip_children_) {
        canvas->save();
        canvas->clipRect(bounds());
    }

    // Apply transform offset
    if (transform_dx_ != 0.0f || transform_dy_ != 0.0f) {
        if (!clip_children_) canvas->save();
        // Apply transform via save and offset
        // (In a full implementation this would use SkMatrix)
        canvas->save();
    }

    // Render children with scroll offset
    if (scroll_x_ != 0.0f || scroll_y_ != 0.0f) {
        // Apply scroll as a translate before rendering children
        // For now, children are positioned with scroll applied during arrange
    }

    render_children(canvas);

    // Restore state
    if (clip_children_) {
        canvas->restore();  // pop clip
    }
    if (transform_dx_ != 0.0f || transform_dy_ != 0.0f) {
        canvas->restore();  // pop transform
    }
}

Widget* ContainerWidget::hit_test(float px, float py) {
    if (!is_visible() || !is_enabled()) return nullptr;
    if (!contains_point(px, py)) return nullptr;

    // Adjust hit point for scroll offset
    float adjusted_px = px + scroll_x_;
    float adjusted_py = py + scroll_y_;

    // Reverse iteration for topmost child
    for (auto it = children().rbegin(); it != children().rend(); ++it) {
        auto* hit = (*it)->hit_test(adjusted_px, adjusted_py);
        if (hit) return hit;
    }

    return this;
}

} // namespace aria
