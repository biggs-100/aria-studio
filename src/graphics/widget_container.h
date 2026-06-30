#ifndef ARIA_WIDGET_CONTAINER_H
#define ARIA_WIDGET_CONTAINER_H

#include "graphics/graphics_types.h"
#include "graphics/widget_themed.h"

#include <optional>
#include <string>

namespace aria {

/// Container widget that provides clipping, transform stacking, and
/// scrollable content area.
///
/// Clips child content to its bounds. Applies an optional affine
/// transform offset during the paint pass. Supports scroll offsets
/// for viewport-style containment. Inherits from ThemedWidget for
/// theme background support.
class ContainerWidget : public ThemedWidget {
public:
    ContainerWidget();
    ~ContainerWidget() override;

    const char* type_name() const noexcept override { return "ContainerWidget"; }

    // ── Clipping ───────────────────────────────────────────────

    void set_clip_children(bool clip) noexcept { clip_children_ = clip; }
    bool clip_children() const noexcept { return clip_children_; }

    // ── Scroll ─────────────────────────────────────────────────

    void set_scroll_offset(float dx, float dy) noexcept {
        scroll_x_ = dx;
        scroll_y_ = dy;
        mark_dirty();
    }
    float scroll_x() const noexcept { return scroll_x_; }
    float scroll_y() const noexcept { return scroll_y_; }

    /// Scroll by a relative amount.
    void scroll_by(float dx, float dy) noexcept {
        scroll_x_ += dx;
        scroll_y_ += dy;
        mark_dirty();
    }

    // ── Transform ──────────────────────────────────────────────

    void set_transform_offset(float dx, float dy) noexcept {
        transform_dx_ = dx;
        transform_dy_ = dy;
        mark_dirty();
    }
    float transform_dx() const noexcept { return transform_dx_; }
    float transform_dy() const noexcept { return transform_dy_; }

    // ── Layout ─────────────────────────────────────────────────

    Vec2 measure(float available_width, float available_height) override;
    void arrange(float x, float y, float w, float h) override;

    // ── Paint ──────────────────────────────────────────────────

    void render(SkiaCanvas* canvas) override;

    // ── Hit testing ────────────────────────────────────────────

    Widget* hit_test(float px, float py) override;

    // ── Theme background ───────────────────────────────────────

    /// Set a theme path for the background color.
    void set_theme_background(std::string_view path) {
        theme_bg_path_ = std::string(path);
        mark_dirty();
    }

    /// Clear the theme background path.
    void clear_theme_background() {
        theme_bg_path_.reset();
    }

    /// Returns true if a theme background path is set.
    bool has_theme_background() const noexcept { return theme_bg_path_.has_value(); }

private:
    bool clip_children_ = true;
    float scroll_x_ = 0.0f;
    float scroll_y_ = 0.0f;
    float transform_dx_ = 0.0f;
    float transform_dy_ = 0.0f;

    /// Optional theme path for background color.
    std::optional<std::string> theme_bg_path_;
};

} // namespace aria

#endif // ARIA_WIDGET_CONTAINER_H
