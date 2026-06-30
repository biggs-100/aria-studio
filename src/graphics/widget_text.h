#ifndef ARIA_WIDGET_TEXT_H
#define ARIA_WIDGET_TEXT_H

#include "graphics/graphics_types.h"
#include "graphics/skia_canvas.h"  // for Font
#include "graphics/widget_themed.h"

#include <optional>
#include <string>

namespace aria {

/// Widget that renders a text string via Skia textblobs.
///
/// Text is rendered using SkiaCanvas::drawTextBlob during the paint
/// pass. Font properties (family, size) are configurable.
/// Inherits from ThemedWidget for theme color support.
class TextWidget : public ThemedWidget {
public:
    TextWidget();
    explicit TextWidget(std::string_view text);
    ~TextWidget() override;

    const char* type_name() const noexcept override { return "TextWidget"; }

    // ── Text content ───────────────────────────────────────────

    void set_text(std::string_view t) { text_ = t; mark_dirty(); }
    const std::string& text() const noexcept { return text_; }

    // ── Font ───────────────────────────────────────────────────

    void set_font(const Font& f) noexcept { font_ = f; mark_dirty(); }
    const Font& font() const noexcept { return font_; }

    // ── Text colour ────────────────────────────────────────────

    void set_color(const Color& c) noexcept { color_ = c; mark_dirty(); }
    const Color& color() const noexcept { return color_; }

    // ── Layout ─────────────────────────────────────────────────

    Vec2 measure(float available_width, float available_height) override;

    // ── Paint ──────────────────────────────────────────────────

    void render(SkiaCanvas* canvas) override;

    // ── Theme token (optional) ──────────────────────────────────

    /// Set a theme path for the text color (e.g. "colors.text.primary").
    void set_theme_color(std::string_view path) {
        theme_color_path_ = std::string(path);
        mark_dirty();
    }

    /// Clear the theme color path, falling back to color_.
    void clear_theme_color() {
        theme_color_path_.reset();
    }

    /// Returns true if a theme color path is set.
    bool has_theme_color() const noexcept { return theme_color_path_.has_value(); }

    /// Get the current theme color path.
    const std::string& theme_color_path() const { return *theme_color_path_; }

private:
    std::string text_;
    Font font_;
    Color color_ = Color::Black;

    /// Optional theme path for resolving text color.
    std::optional<std::string> theme_color_path_;
};

} // namespace aria

#endif // ARIA_WIDGET_TEXT_H
