#include "graphics/widget_rect.h"
#include "graphics/skia_canvas.h"

namespace aria {

RectWidget::RectWidget() = default;

RectWidget::RectWidget(const Paint& paint)
    : paint_(paint) {}

RectWidget::~RectWidget() = default;

Vec2 RectWidget::measure(float /*available_width*/, float /*available_height*/) {
    Vec2 result = {bounds().w, bounds().h};
    set_preferred_size(result);
    return result;
}

void RectWidget::render(SkiaCanvas* canvas) {
    if (!canvas) return;

    Paint render_paint = paint_;

    // Resolve theme fill if path is set
    if (theme_fill_path_.has_value()) {
        render_paint.fill = resolve_color(*theme_fill_path_);
    }

    // Resolve theme stroke if path is set
    if (theme_stroke_path_.has_value()) {
        render_paint.stroke = resolve_color(*theme_stroke_path_);
    }

    canvas->drawRect(bounds(), render_paint);
}

} // namespace aria
