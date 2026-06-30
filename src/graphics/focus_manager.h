#ifndef ARIA_FOCUS_MANAGER_H
#define ARIA_FOCUS_MANAGER_H

#include "graphics/graphics_types.h"

#include <functional>
#include <vector>

namespace aria {

class SkiaCanvas;
class Widget;

// ── FocusManager ──────────────────────────────────────────────────

/// Manages keyboard focus traversal across widgets using explicit
/// tab-order indices.
///
/// Provides tab-order-based next/previous navigation, programmatic
/// focus assignment, and focus-ring rendering. Focus/blur events are
/// dispatched as callbacks on the affected widgets.
///
/// Registered via ServiceLocator; called by RenderLoop during the
/// input-processing phase of each frame.
class FocusManager {
public:
    FocusManager() = default;
    ~FocusManager() = default;

    FocusManager(const FocusManager&) = delete;
    FocusManager& operator=(const FocusManager&) = delete;

    // ── Tab-order management ───────────────────────────────────

    /// Rebuild the tab-order list by scanning the widget subtree
    /// rooted at `root`. Widgets are sorted by tab_index ascending;
    /// widgets with the same tab_index retain insertion order.
    /// Non-focusable (disabled, invisible) widgets are excluded.
    void rebuild_list(Widget* root);

    /// Move focus to the next widget in the tab order (wraps around).
    void focus_next();

    /// Move focus to the previous widget in the tab order (wraps around).
    void focus_previous();

    /// Programmatically focus a specific widget by ID.
    /// Dispatches kFocusLost on the previously focused widget and
    /// kFocusGained on the newly focused widget.
    /// No-op if the ID is not in the tab-order list.
    void set_focus(WidgetID id);

    /// Dispatch focus/blur events by calling set_focused() on the
    /// affected widgets.
    void set_focus_widget(Widget* widget);

    /// Returns the currently focused widget ID, or kInvalidWidgetID.
    WidgetID focused_widget() const noexcept { return focused_; }

    /// Returns the currently focused Widget pointer, or nullptr.
    Widget* focused_widget_ptr(Widget* root) const;

    /// Render the focus ring (themed outline) around the focused
    /// widget's bounds. Called after the paint pass.
    void render_focus_ring(SkiaCanvas* canvas, Widget* root) const;

private:
    std::vector<WidgetID> tab_order_;
    WidgetID focused_ = kInvalidWidgetID;
};

} // namespace aria

#endif // ARIA_FOCUS_MANAGER_H
