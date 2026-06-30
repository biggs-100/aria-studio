#include "graphics/ffi/bridge.h"
#include "graphics/ffi/command_types.h"
#include "graphics/skia_canvas.h"

#include <algorithm>
#include <cmath>

namespace aria {
namespace ffi {

// =====================================================================
// Construction
// =====================================================================

Bridge::Bridge(SkiaCanvas* canvas)
    : canvas_(canvas) {}

// =====================================================================
// HiDPI
// =====================================================================

void Bridge::set_device_pixel_ratio(float ratio) {
    dpr_ = std::max(ratio, 0.1f);
}

float Bridge::device_pixel_ratio() const noexcept {
    return dpr_;
}

void Bridge::apply_dpr(DrawCommand& cmd) const {
    if (dpr_ == 1.0f) return;

    // Scale all position/size coordinates by DPR
    cmd.params.x *= dpr_;
    cmd.params.y *= dpr_;
    cmd.params.w *= dpr_;
    cmd.params.h *= dpr_;
    cmd.params.cx *= dpr_;
    cmd.params.cy *= dpr_;
    cmd.params.radius *= dpr_;
    cmd.params.font_size *= dpr_;
    cmd.params.corner_radius *= dpr_;
    cmd.params.stroke_width *= dpr_;
}

// =====================================================================
// Command dispatch
// =====================================================================

void Bridge::dispatch(const DrawCommand& cmd) {
    if (!canvas_) return;

    switch (cmd.type) {
        case DrawCommandType::kClear: {
            Color color{cmd.params.r, cmd.params.g, cmd.params.b, cmd.params.a};
            canvas_->clear(color);
            break;
        }

        case DrawCommandType::kDrawRect: {
            Rect rect{cmd.params.x, cmd.params.y, cmd.params.w, cmd.params.h};
            Paint paint;
            paint.fill          = {cmd.params.r, cmd.params.g, cmd.params.b, cmd.params.a};
            paint.stroke        = {cmd.params.sr, cmd.params.sg, cmd.params.sb, cmd.params.sa};
            paint.stroke_width  = cmd.params.stroke_width;
            paint.corner_radius = cmd.params.corner_radius;
            canvas_->drawRect(rect, paint);
            break;
        }

        case DrawCommandType::kDrawCircle: {
            Paint paint;
            paint.fill          = {cmd.params.r, cmd.params.g, cmd.params.b, cmd.params.a};
            paint.stroke        = {cmd.params.sr, cmd.params.sg, cmd.params.sb, cmd.params.sa};
            paint.stroke_width  = cmd.params.stroke_width;
            canvas_->drawCircle(cmd.params.cx, cmd.params.cy, cmd.params.radius, paint);
            break;
        }

        case DrawCommandType::kDrawText: {
            Font font;
            font.family = cmd.params.font_family;
            font.size   = cmd.params.font_size;

            Paint paint;
            paint.fill = {cmd.params.r, cmd.params.g, cmd.params.b, cmd.params.a};
            canvas_->drawTextBlob(cmd.params.text.c_str(), font,
                                  cmd.params.x, cmd.params.y, paint);
            break;
        }

        case DrawCommandType::kFlush: {
            canvas_->flush();
            break;
        }

        case DrawCommandType::kSave: {
            canvas_->save();
            break;
        }

        case DrawCommandType::kRestore: {
            canvas_->restore();
            break;
        }

        case DrawCommandType::kClipRect: {
            Rect clip{cmd.params.x, cmd.params.y, cmd.params.w, cmd.params.h};
            canvas_->clipRect(clip);
            break;
        }
    }
}

// =====================================================================
// Execute command buffer
// =====================================================================

void Bridge::execute(const std::string& json_buffer) {
    if (json_buffer.empty()) return;

    // Parse the JSON command buffer
    auto commands = parse_commands(json_buffer);
    if (commands.empty()) return;

    // Apply HiDPI scaling to every command
    for (auto& cmd : commands) {
        apply_dpr(cmd);
    }

    // Dispatch each command in order
    for (const auto& cmd : commands) {
        dispatch(cmd);
    }

    // Auto-flush if the last command was not a flush
    if (!commands.empty() && commands.back().type != DrawCommandType::kFlush) {
        if (canvas_) canvas_->flush();
    }
}

} // namespace ffi
} // namespace aria
