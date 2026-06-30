#include "graphics/widget_button.h"
#include "graphics/skia_canvas.h"

namespace aria {

ButtonWidget::ButtonWidget()
    : label_("Button") {}

ButtonWidget::ButtonWidget(std::string_view label)
    : label_(label) {}

ButtonWidget::~ButtonWidget() = default;

void ButtonWidget::set_label(std::string_view text) {
    label_ = text;
    mark_dirty();
}

const std::string& ButtonWidget::label() const noexcept {
    return label_;
}

Color ButtonWidget::resolve_fill_color() const noexcept {
    Color c;

    if (!is_enabled()) {
        c = has_theme_disabled_color()
            ? resolve_color(theme_disabled_color_)
            : disabled_color_;
    } else if (is_pressed()) {
        c = has_theme_pressed_color()
            ? resolve_color(theme_pressed_color_)
            : pressed_color_;
    } else if (is_hovered()) {
        c = has_theme_hover_color()
            ? resolve_color(theme_hover_color_)
            : hover_color_;
    } else {
        c = has_theme_normal_color()
            ? resolve_color(theme_normal_color_)
            : normal_color_;
    }

    return c;
}

Vec2 ButtonWidget::measure(float /*available_width*/, float /*available_height*/) {
    float text_width = 14.0f * static_cast<float>(label_.size()) * 0.6f;
    float padding = 16.0f;
    Vec2 result = {text_width + padding, 30.0f};
    set_preferred_size(result);
    return result;
}

void ButtonWidget::render(SkiaCanvas* canvas) {
    if (!canvas || !is_visible()) return;

    // Draw background rect with state-dependent colour
    Paint bg_paint;
    bg_paint.fill = resolve_fill_color();
    bg_paint.corner_radius = 4.0f;
    canvas->drawRect(bounds(), bg_paint);

    // Draw label text
    if (!label_.empty()) {
        Paint text_paint;
        text_paint.fill = has_theme_text_color()
            ? resolve_color(theme_text_color_)
            : text_color_;
        Font font;
        font.size = 14.0f;
        float text_x = bounds().x + (bounds().w - font.size * label_.size() * 0.6f) / 2.0f;
        float text_y = bounds().y + (bounds().h - font.size) / 2.0f;
        canvas->drawTextBlob(label_.c_str(), font, text_x, text_y, text_paint);
    }
}

} // namespace aria
