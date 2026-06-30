#ifndef ARIA_WIDGET_BUTTON_H
#define ARIA_WIDGET_BUTTON_H

#include "graphics/graphics_types.h"
#include "graphics/widget_themed.h"

#include <memory>
#include <string>

namespace aria {

/// Interactive button widget with hover, pressed, and disabled states.
///
/// Composed of a background rect (with state-dependent colours)
/// and a text label. State transitions update the visual
/// appearance automatically. Inherits from ThemedWidget for theme
/// token support.
class ButtonWidget : public ThemedWidget {
public:
    ButtonWidget();
    explicit ButtonWidget(std::string_view label);
    ~ButtonWidget() override;

    const char* type_name() const noexcept override { return "ButtonWidget"; }

    // ── Label ──────────────────────────────────────────────────

    void set_label(std::string_view text);
    const std::string& label() const noexcept;

    // ── State colours ──────────────────────────────────────────

    void set_normal_color(const Color& c) noexcept { normal_color_ = c; mark_dirty(); }
    void set_hover_color(const Color& c) noexcept { hover_color_ = c; mark_dirty(); }
    void set_pressed_color(const Color& c) noexcept { pressed_color_ = c; mark_dirty(); }
    void set_disabled_color(const Color& c) noexcept { disabled_color_ = c; mark_dirty(); }
    void set_text_color(const Color& c) noexcept { text_color_ = c; mark_dirty(); }

    const Color& normal_color() const noexcept { return normal_color_; }
    const Color& hover_color() const noexcept { return hover_color_; }
    const Color& pressed_color() const noexcept { return pressed_color_; }
    const Color& disabled_color() const noexcept { return disabled_color_; }
    const Color& text_color() const noexcept { return text_color_; }

    // ── Theme paths (optional — override hardcoded colors) ─────

    void set_theme_normal_color(std::string_view path)   { theme_normal_color_ = std::string(path); mark_dirty(); }
    void set_theme_hover_color(std::string_view path)    { theme_hover_color_ = std::string(path); mark_dirty(); }
    void set_theme_pressed_color(std::string_view path)  { theme_pressed_color_ = std::string(path); mark_dirty(); }
    void set_theme_disabled_color(std::string_view path) { theme_disabled_color_ = std::string(path); mark_dirty(); }
    void set_theme_text_color(std::string_view path)     { theme_text_color_ = std::string(path); mark_dirty(); }

    const std::string& theme_normal_color() const noexcept   { return theme_normal_color_; }
    const std::string& theme_hover_color() const noexcept    { return theme_hover_color_; }
    const std::string& theme_pressed_color() const noexcept  { return theme_pressed_color_; }
    const std::string& theme_disabled_color() const noexcept { return theme_disabled_color_; }
    const std::string& theme_text_color() const noexcept     { return theme_text_color_; }

    bool has_theme_normal_color() const noexcept   { return !theme_normal_color_.empty(); }
    bool has_theme_hover_color() const noexcept    { return !theme_hover_color_.empty(); }
    bool has_theme_pressed_color() const noexcept  { return !theme_pressed_color_.empty(); }
    bool has_theme_disabled_color() const noexcept { return !theme_disabled_color_.empty(); }
    bool has_theme_text_color() const noexcept     { return !theme_text_color_.empty(); }

    // ── Layout ─────────────────────────────────────────────────

    Vec2 measure(float available_width, float available_height) override;

    // ── Paint ──────────────────────────────────────────────────

    void render(SkiaCanvas* canvas) override;

private:
    Color resolve_fill_color() const noexcept;

    std::string label_;
    Color normal_color_   = Color::from_rgba8(60, 60, 60);
    Color hover_color_    = Color::from_rgba8(80, 80, 80);
    Color pressed_color_  = Color::from_rgba8(40, 40, 40);
    Color disabled_color_ = Color::from_rgba8(100, 100, 100);
    Color text_color_     = Color::White;

    // Optional theme token paths
    std::string theme_normal_color_;
    std::string theme_hover_color_;
    std::string theme_pressed_color_;
    std::string theme_disabled_color_;
    std::string theme_text_color_;
};

} // namespace aria

#endif // ARIA_WIDGET_BUTTON_H
