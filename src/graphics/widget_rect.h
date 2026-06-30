#ifndef ARIA_WIDGET_RECT_H
#define ARIA_WIDGET_RECT_H

#include "graphics/graphics_types.h"
#include "graphics/widget_themed.h"

#include <optional>
#include <string>

namespace aria {

/// Simple rectangular widget with fill, border, and shadow.
///
/// Renders a (possibly rounded) rectangle using SkiaCanvas::drawRect
/// during the paint pass. Supports fill colour, stroke border, and
/// corner radius. Inherits from ThemedWidget for theme token support.
class RectWidget : public ThemedWidget {
public:
    RectWidget();
    explicit RectWidget(const Paint& paint);
    ~RectWidget() override;

    const char* type_name() const noexcept override { return "RectWidget"; }

    // ── Paint properties ───────────────────────────────────────

    void set_paint(const Paint& p) noexcept { paint_ = p; mark_dirty(); }
    const Paint& paint() const noexcept { return paint_; }

    void set_fill_color(const Color& c) noexcept { paint_.fill = c; mark_dirty(); }
    void set_stroke_color(const Color& c) noexcept { paint_.stroke = c; mark_dirty(); }
    void set_stroke_width(float w) noexcept { paint_.stroke_width = w; mark_dirty(); }
    void set_corner_radius(float r) noexcept { paint_.corner_radius = r; mark_dirty(); }

    // ── Layout ─────────────────────────────────────────────────

    Vec2 measure(float available_width, float available_height) override;

    // ── Paint ──────────────────────────────────────────────────

    void render(SkiaCanvas* canvas) override;

    // ── Theme tokens (optional) ────────────────────────────────

    /// Set a theme path for the fill color (e.g. "colors.bg.primary").
    /// When set, render() resolves the fill from theme instead of paint_.
    void set_theme_fill(std::string_view path) {
        theme_fill_path_ = std::string(path);
        mark_dirty();
    }

    /// Set the theme fill path directly (without marking dirty — for init).
    void set_theme_fill_path(std::string_view path) {
        theme_fill_path_ = std::string(path);
    }

    /// Clear the theme fill path, falling back to paint_.fill.
    void clear_theme_fill() {
        theme_fill_path_.reset();
    }

    /// Returns true if a theme fill path is set.
    bool has_theme_fill() const noexcept { return theme_fill_path_.has_value(); }

    /// Get the current theme fill path.
    const std::string& theme_fill_path() const { return *theme_fill_path_; }

    /// Set a theme path for the stroke color.
    void set_theme_stroke(std::string_view path) {
        theme_stroke_path_ = std::string(path);
        mark_dirty();
    }

    /// Clear the theme stroke path.
    void clear_theme_stroke() {
        theme_stroke_path_.reset();
    }

private:
    Paint paint_;

    /// Optional theme path for resolving fill color.
    std::optional<std::string> theme_fill_path_;

    /// Optional theme path for resolving stroke color.
    std::optional<std::string> theme_stroke_path_;
};

} // namespace aria

#endif // ARIA_WIDGET_RECT_H
