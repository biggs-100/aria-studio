#ifndef ARIA_WIDGET_H
#define ARIA_WIDGET_H

#include "graphics/animation_types.h"
#include "graphics/graphics_types.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace aria {

// ── Forward declarations ─────────────────────────────────────────

class SkiaCanvas;
class LayoutEngine;

// ── AccessibleRole ────────────────────────────────────────────────

/// Accessibility role for assistive technology integration.
enum class AccessibleRole {
    kNone,
    kButton,
    kSlider,
    kLabel,
    kPanel,
    kList,
    kTextInput,
    kTabStop,
};

// ── Widget base class ─────────────────────────────────────────────

/// Abstract base for all GPU-rendered UI widgets.
///
/// Widget tree is C++ owned — parents own children via unique_ptr.
/// Children are stored in insertion order; last child is visually
/// topmost for hit-testing.
///
/// Design decisions (from design.md):
///   - Identity via WidgetID (uint64_t), auto-assigned sequentially.
///   - Bounds in float pixels (no sub-pixel for now).
///   - Parent owns children via unique_ptr.
///   - Hit-test is reverse depth-first (last child wins).
class Widget {
public:
    Widget();
    virtual ~Widget();

    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = delete;
    Widget& operator=(Widget&&) = delete;

    // ── Identity ───────────────────────────────────────────────

    WidgetID id() const noexcept { return id_; }
    void set_id(WidgetID id) noexcept { id_ = id; }

    /// Human-readable widget type name (for debugging / profiling).
    virtual const char* type_name() const noexcept = 0;

    // ── Geometry ───────────────────────────────────────────────

    Rect bounds() const noexcept { return bounds_; }
    void set_bounds(const Rect& r) noexcept { bounds_ = r; mark_dirty(); }

    float x() const noexcept { return bounds_.x; }
    float y() const noexcept { return bounds_.y; }
    float width() const noexcept { return bounds_.w; }
    float height() const noexcept { return bounds_.h; }

    // ── Tree ───────────────────────────────────────────────────

    Widget* parent() const noexcept { return parent_; }
    void set_parent(Widget* p) noexcept { parent_ = p; }

    /// Transfer ownership of a child widget. If the child already has
    /// a parent, it is removed from that parent first.
    void add_child(std::unique_ptr<Widget> child);

    /// Remove a child by pointer, transferring ownership to the caller.
    /// Returns nullptr if `child` is not found among children.
    std::unique_ptr<Widget> remove_child(Widget* child);

    /// Number of direct children.
    size_t child_count() const noexcept { return children_.size(); }

    /// Access a child by index (0-based). Returns nullptr if out of range.
    Widget* child_at(size_t index) const noexcept;

    /// Read-only access to children for iteration.
    const std::vector<std::unique_ptr<Widget>>& children() const noexcept {
        return children_;
    }

    /// Recursively search the subtree for a widget by ID.
    /// Returns nullptr if no widget with the given ID is found.
    Widget* find_by_id(WidgetID id) noexcept;
    const Widget* find_by_id(WidgetID id) const noexcept;

    // ── Visual state ───────────────────────────────────────────

    bool is_visible() const noexcept { return visible_; }
    void set_visible(bool v) noexcept { visible_ = v; mark_dirty(); }

    float opacity() const noexcept { return opacity_; }
    void set_opacity(float o) noexcept {
        opacity_ = std::clamp(o, 0.0f, 1.0f);
        mark_dirty();
    }

    bool is_enabled() const noexcept { return enabled_; }
    void set_enabled(bool e) noexcept { enabled_ = e; }

    bool is_hovered() const noexcept { return hovered_; }
    void set_hovered(bool h) noexcept { hovered_ = h; }

    bool is_pressed() const noexcept { return pressed_; }
    void set_pressed(bool p) noexcept { pressed_ = p; }

    // ── Layout ─────────────────────────────────────────────────

    /// Measure phase: compute preferred size given available space.
    /// Returns the widget's preferred size.
    /// Subclasses should call set_preferred_size() to cache the result.
    virtual Vec2 measure(float available_width, float available_height) = 0;

    /// Cached preferred size from the last measure pass.
    Vec2 preferred_size() const noexcept { return preferred_size_; }
    void set_preferred_size(Vec2 s) noexcept { preferred_size_ = s; }

    /// Arrange phase: assign final position and size.
    virtual void arrange(float x, float y, float w, float h);

    // ── Paint ──────────────────────────────────────────────────

    /// Render this widget and its children onto the GPU canvas.
    /// Called during the paint pass for visible widgets only.
    virtual void render(SkiaCanvas* canvas) = 0;

    /// Walk the visible subtree and render each widget.
    /// Calls render() on this widget, then render_children().
    void paint(SkiaCanvas* canvas);

    /// Render all visible children in order.
    void render_children(SkiaCanvas* canvas);

    // ── Animation ─────────────────────────────────────────────

    /// Store an animated interpolated value for a property.
    /// Calls mark_dirty() when the value changes.
    void set_animated_property(AnimatableProperty prop, float value) noexcept;

    /// Retrieve the last stored animated value for a property.
    /// Returns 0.0f if never set.
    float animated_value(AnimatableProperty prop) const noexcept;

    // ── Hit testing ────────────────────────────────────────────

    /// Resolve a screen point to the frontmost visible, enabled widget.
    /// Returns nullptr if no widget contains the point.
    Widget* hit_test(float px, float py);

    // ── Dirty tracking ─────────────────────────────────────────

    bool is_dirty() const noexcept { return dirty_; }
    void mark_dirty() noexcept { dirty_ = true; }
    void clear_dirty() noexcept { dirty_ = false; }

    // ── Transform (optional) ───────────────────────────────────

    void set_transform_offset(float dx, float dy) noexcept {
        transform_dx_ = dx;
        transform_dy_ = dy;
    }
    float transform_dx() const noexcept { return transform_dx_; }
    float transform_dy() const noexcept { return transform_dy_; }

    // ── Tab order (FocusManager integration) ───────────────────

    /// Tab-order index for FocusManager navigation.
    /// Lower values come first; default 0 defers to insertion order.
    void set_tab_index(int idx) noexcept { tab_index_ = idx; }
    int tab_index() const noexcept { return tab_index_; }

    // ── Accessibility ──────────────────────────────────────────

    /// Human-readable name for assistive technology (e.g. "Play Button").
    void set_accessible_name(std::string_view name) { accessible_name_ = std::string(name); }
    const std::string& accessible_name() const noexcept { return accessible_name_; }

    /// Semantic role for assistive technology.
    void set_accessible_role(AccessibleRole role) noexcept { accessible_role_ = role; }
    AccessibleRole accessible_role() const noexcept { return accessible_role_; }

    /// Longer description for assistive technology (optional).
    void set_accessible_description(std::string_view desc) { accessible_description_ = std::string(desc); }
    const std::string& accessible_description() const noexcept { return accessible_description_; }

    /// Resolve the effective accessible role by walking the parent chain.
    /// Returns this widget's role if set; otherwise inherits from parent.
    /// Returns kNone if no role is set on this widget or any ancestor.
    AccessibleRole effective_role() const;

    // ── Focus state (FocusManager integration) ─────────────────

    /// Whether this widget currently has keyboard focus.
    bool is_focused() const noexcept { return is_focused_; }

    /// Set or clear keyboard focus. When true, the on_focus callback is
    /// invoked; when false, the on_blur callback is invoked.
    void set_focused(bool focused);

    /// Register a callback invoked when this widget gains focus.
    void on_focus(std::function<void()> cb) { on_focus_callback_ = std::move(cb); }

    /// Register a callback invoked when this widget loses focus.
    void on_blur(std::function<void()> cb) { on_blur_callback_ = std::move(cb); }

protected:
    // Internal hit-test: check if this widget (not children) contains point.
    virtual bool contains_point(float px, float py) const;

private:
    WidgetID id_ = kInvalidWidgetID;
    Rect bounds_;
    Widget* parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;

    bool visible_ = true;
    float opacity_ = 1.0f;
    bool enabled_ = true;
    bool hovered_ = false;
    bool pressed_ = false;
    bool dirty_ = true;

    float transform_dx_ = 0.0f;
    float transform_dy_ = 0.0f;

    // Animated property storage (8 properties × float)
    std::array<float, 8> animated_values_{};

    // Accessibility and focus fields
    int tab_index_ = 0;
    std::string accessible_name_;
    AccessibleRole accessible_role_ = AccessibleRole::kNone;
    std::string accessible_description_;
    bool is_focused_ = false;
    std::function<void()> on_focus_callback_;
    std::function<void()> on_blur_callback_;

    static WidgetID next_id();
};

} // namespace aria

#endif // ARIA_WIDGET_H
