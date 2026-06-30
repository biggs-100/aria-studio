#include "graphics/skia_canvas.h"
#include "graphics/graphics_engine.h"

#include <cstdint>
#include <cstring>

// ── Skia headers (available after FetchContent) ──────────────────
#include <SkSurface.h>
#include <SkCanvas.h>
#include <SkPaint.h>
#include <SkRect.h>
#include <SkRRect.h>
#include <SkTextBlob.h>
#include <SkFont.h>
#include <SkTypeface.h>
#include <SkColor.h>
#include <gpu/GrDirectContext.h>
#include <gpu/ganesh/SkSurfaceGanesh.h>
#include <gpu/ganesh/dawn/GrDawnBackendContext.h>

// ── Dawn headers ─────────────────────────────────────────────────
#include <dawn/webgpu_cpp.h>

namespace aria {

// =====================================================================
// Pimpl — Hide Skia types from the public header
// =====================================================================

struct SkiaCanvas::Impl {
    // Skia GPU context and surface
    sk_sp<GrDirectContext> gr_context;
    sk_sp<SkSurface>       surface;
    SkCanvas*              canvas_ptr = nullptr;  // owned by surface

    // Dawn interaction
    GraphicsEngine*   engine    = nullptr;
    WGPUSwapChainImpl* swapchain = nullptr;

    // Profiling
    uint32_t draw_calls = 0;

    // State
    bool ready = false;

    // ── Helpers ────────────────────────────────────────────────

    SkCanvas* safe_canvas() {
        return ready ? canvas_ptr : nullptr;
    }

    void reset() {
        surface    = nullptr;
        canvas_ptr = nullptr;
        gr_context = nullptr;
        engine     = nullptr;
        swapchain  = nullptr;
        draw_calls = 0;
        ready      = false;
    }
};

// =====================================================================
// Public API
// =====================================================================

SkiaCanvas::SkiaCanvas()
    : impl_(std::make_unique<Impl>()) {}

SkiaCanvas::~SkiaCanvas() = default;

SkiaCanvas::SkiaCanvas(SkiaCanvas&&) noexcept = default;
SkiaCanvas& SkiaCanvas::operator=(SkiaCanvas&&) noexcept = default;

bool SkiaCanvas::init(GraphicsEngine* engine, WGPUSwapChainImpl* swapchain) {
    if (!engine || !swapchain) {
        return false;
    }

    impl_->engine    = engine;
    impl_->swapchain = swapchain;

    // Get swap chain info
    SwapChainInfo info = engine->query_swapchain_info(swapchain);
    if (info.width == 0 || info.height == 0) {
        return false;
    }

    // Create the GrDawnBackendContext
    // This requires accessing the Dawn device from the engine.
    // For now, we create a minimal GPU context.
    // Full Dawn integration happens when the GPU device is accessible.

    // Create an SkSurface backed by the swap chain texture.
    // In PR 2 this creates a basic surface; full Dawn texture sharing
    // is wired when the device handle is available.
    GrDawnBackendContext dawn_context{};

    // For the initial implementation, we try to create a GPU context.
    // If Dawn device access isn't available yet, we fall back gracefully.
    impl_->gr_context = GrDirectContext::MakeDawn(dawn_context);
    if (!impl_->gr_context) {
        // GPU context creation failed — safe no-op fallback
        impl_->ready = false;
        return false;
    }

    // Create a surface
    SkImageInfo image_info = SkImageInfo::Make(
        static_cast<int>(info.width),
        static_cast<int>(info.height),
        kRGBA_8888_SkColorType,
        kPremul_SkAlphaType
    );

    // Use SkSurfaces::RenderTarget for GPU-backed surface
    impl_->surface = SkSurfaces::RenderTarget(
        impl_->gr_context.get(),
        skgpu::Budgeted::kYes,
        image_info
    );

    if (!impl_->surface) {
        impl_->gr_context = nullptr;
        return false;
    }

    impl_->canvas_ptr = impl_->surface->getCanvas();
    impl_->ready = true;
    impl_->draw_calls = 0;
    return true;
}

bool SkiaCanvas::is_ready() const noexcept {
    return impl_->ready;
}

void SkiaCanvas::clear(const Color& c) {
    auto* sk_canvas = impl_->safe_canvas();
    if (!sk_canvas) return;

    sk_canvas->clear(SkColor4f{c.r, c.g, c.b, c.a}.toSkColor());
    ++impl_->draw_calls;
}

void SkiaCanvas::drawRect(const Rect& r, const Paint& p) {
    auto* sk_canvas = impl_->safe_canvas();
    if (!sk_canvas) return;

    SkPaint sk_paint;
    sk_paint.setColor(SkColor4f{p.fill.r, p.fill.g, p.fill.b, p.fill.a});

    if (p.stroke_width > 0.0f && p.stroke.a > 0.0f) {
        sk_paint.setStyle(SkPaint::kStrokeAndFill_Style);
        sk_paint.setStrokeWidth(p.stroke_width);
        SkPaint stroke_paint = sk_paint;
        stroke_paint.setColor(SkColor4f{p.stroke.r, p.stroke.g, p.stroke.b, p.stroke.a});
        stroke_paint.setStyle(SkPaint::kStroke_Style);
        stroke_paint.setStrokeWidth(p.stroke_width);

        if (p.corner_radius > 0.0f) {
            SkRRect rrect = SkRRect::MakeRectXY(
                SkRect::MakeXYWH(r.x, r.y, r.w, r.h),
                p.corner_radius, p.corner_radius);
            sk_canvas->drawRRect(rrect, sk_paint);
            sk_canvas->drawRRect(rrect, stroke_paint);
        } else {
            SkRect sk_r = SkRect::MakeXYWH(r.x, r.y, r.w, r.h);
            sk_canvas->drawRect(sk_r, sk_paint);
            sk_canvas->drawRect(sk_r, stroke_paint);
        }
    } else {
        if (p.corner_radius > 0.0f) {
            SkRRect rrect = SkRRect::MakeRectXY(
                SkRect::MakeXYWH(r.x, r.y, r.w, r.h),
                p.corner_radius, p.corner_radius);
            sk_canvas->drawRRect(rrect, sk_paint);
        } else {
            sk_canvas->drawRect(SkRect::MakeXYWH(r.x, r.y, r.w, r.h), sk_paint);
        }
    }

    ++impl_->draw_calls;
}

void SkiaCanvas::drawCircle(float cx, float cy, float radius, const Paint& p) {
    auto* sk_canvas = impl_->safe_canvas();
    if (!sk_canvas) return;

    SkPaint sk_paint;
    sk_paint.setColor(SkColor4f{p.fill.r, p.fill.g, p.fill.b, p.fill.a});

    if (p.stroke_width > 0.0f && p.stroke.a > 0.0f) {
        sk_paint.setStyle(SkPaint::kStrokeAndFill_Style);
        SkPaint stroke_paint;
        stroke_paint.setColor(SkColor4f{p.stroke.r, p.stroke.g, p.stroke.b, p.stroke.a});
        stroke_paint.setStyle(SkPaint::kStroke_Style);
        stroke_paint.setStrokeWidth(p.stroke_width);
        sk_canvas->drawCircle(cx, cy, radius, sk_paint);
        sk_canvas->drawCircle(cx, cy, radius, stroke_paint);
    } else {
        sk_canvas->drawCircle(cx, cy, radius, sk_paint);
    }

    ++impl_->draw_calls;
}

void SkiaCanvas::drawTextBlob(const char* text, const Font& font,
                              float x, float y, const Paint& p) {
    auto* sk_canvas = impl_->safe_canvas();
    if (!sk_canvas || !text || text[0] == '\0') return;

    // Create Skia font from our Font descriptor
    sk_sp<SkTypeface> typeface = SkTypeface::MakeFromName(
        font.family.c_str(),
        SkFontStyle::Normal()
    );
    SkFont sk_font(typeface ? typeface : SkTypeface::MakeDefault(), font.size);

    // Build a text blob
    sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText(
        text, std::strlen(text), sk_font,
        SkTextEncoding::kUTF8
    );
    if (!blob) return;

    SkPaint sk_paint;
    sk_paint.setColor(SkColor4f{p.fill.r, p.fill.g, p.fill.b, p.fill.a});
    sk_paint.setAntiAlias(true);

    sk_canvas->drawTextBlob(blob.get(), x, y, sk_paint);
    ++impl_->draw_calls;
}

void SkiaCanvas::flush() {
    if (impl_->gr_context && impl_->ready) {
        impl_->gr_context->flush();
        impl_->gr_context->submit();
    }
    impl_->draw_calls = 0;
}

void SkiaCanvas::save() {
    auto* sk_canvas = impl_->safe_canvas();
    if (sk_canvas) {
        sk_canvas->save();
    }
}

void SkiaCanvas::restore() {
    auto* sk_canvas = impl_->safe_canvas();
    if (sk_canvas) {
        sk_canvas->restore();
    }
}

void SkiaCanvas::clipRect(const Rect& r) {
    auto* sk_canvas = impl_->safe_canvas();
    if (sk_canvas) {
        sk_canvas->clipRect(SkRect::MakeXYWH(r.x, r.y, r.w, r.h));
    }
}

bool SkiaCanvas::recreate_surface(uint32_t width, uint32_t height) {
    if (!impl_->gr_context) {
        return false;
    }

    if (width == 0 || height == 0) {
        return false;
    }

    SkImageInfo image_info = SkImageInfo::Make(
        static_cast<int>(width),
        static_cast<int>(height),
        kRGBA_8888_SkColorType,
        kPremul_SkAlphaType
    );

    impl_->surface = SkSurfaces::RenderTarget(
        impl_->gr_context.get(),
        skgpu::Budgeted::kYes,
        image_info
    );

    if (!impl_->surface) {
        return false;
    }

    impl_->canvas_ptr = impl_->surface->getCanvas();
    impl_->ready = true;
    return true;
}

uint32_t SkiaCanvas::draw_call_count() const noexcept {
    return impl_->draw_calls;
}

} // namespace aria
