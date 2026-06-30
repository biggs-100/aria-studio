#include "graphics/focus_manager.h"
#include "graphics/skia_canvas.h"
#include "graphics/widget.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace aria {

// =====================================================================
// Tab-order management
// =====================================================================

namespace {

/// Recursively collect focusable widgets from the subtree into `out`.
/// A widget is focusable if it is visible, enabled, and has a non-zero
/// tab_index OR has the kTabStop role.
void collect_focusable(Widget* widget, std::vector<Widget*>& out) {
    if (!widget) return;
    if (widget->is_visible() && widget->is_enabled()) {
        // Only include widgets that opt into tab navigation
        if (widget->tab_index() > 0 ||
            widget->accessible_role() == AccessibleRole::kTabStop) {
            out.push_back(widget);
        }
    }
    for (auto& child : widget->children()) {
        collect_focusable(child.get(), out);
    }
}

} // anonymous namespace

void FocusManager::rebuild_list(Widget* root) {
    if (!root) {
        tab_order_.clear();
        return;
    }

    // Collect all focusable widgets
    std::vector<Widget*> focusable;
    collect_focusable(root, focusable);

    // Sort by tab_index ascending; stable sort preserves insertion order
    // for equal indices (already in tree-traversal order from collect).
    std::stable_sort(focusable.begin(), focusable.end(),
        [](const Widget* a, const Widget* b) {
            return a->tab_index() < b->tab_index();
        });

    // Extract WidgetIDs
    tab_order_.clear();
    tab_order_.reserve(focusable.size());
    for (const auto* w : focusable) {
        tab_order_.push_back(w->id());
    }
}

void FocusManager::focus_next() {
    if (tab_order_.empty()) return;

    auto it = std::find(tab_order_.begin(), tab_order_.end(), focused_);
    if (it == tab_order_.end() || std::next(it) == tab_order_.end()) {
        // Not found or at end — wrap to first
        set_focus(tab_order_.front());
    } else {
        set_focus(*std::next(it));
    }
}

void FocusManager::focus_previous() {
    if (tab_order_.empty()) return;

    auto it = std::find(tab_order_.begin(), tab_order_.end(), focused_);
    if (it == tab_order_.begin() || it == tab_order_.end()) {
        // At beginning or not found — wrap to last
        set_focus(tab_order_.back());
    } else {
        set_focus(*std::prev(it));
    }
}

void FocusManager::set_focus(WidgetID id) {
    // Verify the ID is in the tab order
    auto it = std::find(tab_order_.begin(), tab_order_.end(), id);
    if (it == tab_order_.end()) return;

    focused_ = id;
}

void FocusManager::set_focus_widget(Widget* widget) {
    if (!widget) return;
    set_focus(widget->id());
}

Widget* FocusManager::focused_widget_ptr(Widget* root) const {
    if (focused_ == kInvalidWidgetID || !root) return nullptr;
    return root->find_by_id(focused_);
}

void FocusManager::render_focus_ring(SkiaCanvas* canvas, Widget* root) const {
    if (!canvas || !root || focused_ == kInvalidWidgetID) return;

    Widget* focused = root->find_by_id(focused_);
    if (!focused || !focused->is_visible()) return;

    // Draw a themed focus ring: a 2px outlined rect slightly expanded
    // beyond the focused widget's bounds.
    Rect b = focused->bounds();
    constexpr float kFocusRingPadding = 2.0f;
    constexpr float kFocusRingWidth = 2.0f;

    Paint ring_paint;
    ring_paint.fill = Color::Transparent;
    ring_paint.stroke = Color::from_rgba8(100, 150, 255);  // blue accent
    ring_paint.stroke_width = kFocusRingWidth;
    ring_paint.corner_radius = 2.0f;

    // Expand bounds slightly so the ring is visible outside the widget
    Rect ring_bounds{
        b.x - kFocusRingPadding,
        b.y - kFocusRingPadding,
        b.w + 2.0f * kFocusRingPadding,
        b.h + 2.0f * kFocusRingPadding,
    };

    canvas->drawRect(ring_bounds, ring_paint);
}

} // namespace aria
