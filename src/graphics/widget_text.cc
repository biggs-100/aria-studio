#include "graphics/widget_text.h"
#include "graphics/skia_canvas.h"

namespace aria {

TextWidget::TextWidget() = default;

TextWidget::TextWidget(std::string_view text)
    : text_(text) {}

TextWidget::~TextWidget() = default;

Vec2 TextWidget::measure(float /*available_width*/, float /*available_height*/) {
    float estimated_width = font_.size * static_cast<float>(text_.size()) * 0.6f;
    Vec2 result = {estimated_width, font_.size * 1.2f};
    set_preferred_size(result);
    return result;
}

void TextWidget::render(SkiaCanvas* canvas) {
    if (!canvas || text_.empty()) return;

    Paint paint;
    paint.fill = theme_color_path_.has_value()
        ? resolve_color(*theme_color_path_)
        : color_;
    canvas->drawTextBlob(text_.c_str(), font_,
                         bounds().x, bounds().y + font_.size, paint);
}

} // namespace aria
