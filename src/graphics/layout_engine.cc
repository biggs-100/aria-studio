#include "graphics/layout_engine.h"
#include "graphics/widget.h"

namespace aria {

void LayoutEngine::compute(Widget* root, Vec2 viewport) {
    if (!root) return;

    // Phase 1: Measure — traverse depth-first, compute preferred sizes
    measure_pass(root, viewport.x, viewport.y);

    // Phase 2: Arrange — assign final positions
    arrange_pass(root, 0.0f, 0.0f, viewport.x, viewport.y);
}

void LayoutEngine::measure_pass(Widget* widget, float available_w, float available_h) {
    if (!widget || !widget->is_visible()) return;

    // Measure this widget
    widget->measure(available_w, available_h);

    // Recursively measure children
    for (const auto& child : widget->children()) {
        measure_pass(child.get(), available_w, available_h);
    }
}

void LayoutEngine::arrange_pass(Widget* widget, float x, float y, float w, float h) {
    if (!widget || !widget->is_visible()) return;

    // Arrange this widget
    widget->arrange(x, y, w, h);

    // Recursively arrange children with their own space
    // Use the widget's measured preferred size from the measure pass
    // instead of re-measuring (which would duplicate work).
    for (const auto& child : widget->children()) {
        Vec2 pref = child->preferred_size();
        float cw = pref.x > 0.0f ? pref.x : w;
        float ch = pref.y > 0.0f ? pref.y : h;
        arrange_pass(child.get(), x, y, cw, ch);
    }
}

} // namespace aria
