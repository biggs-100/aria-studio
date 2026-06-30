#ifndef ARIA_SKIA_CANVAS_H
#define ARIA_SKIA_CANVAS_H

#include "graphics/graphics_types.h"

#include <cstdint>
#include <memory>
#include <string>

// ── Skia forward declarations ────────────────────────────────────
class SkSurface;
class SkCanvas;
class GrDirectContext;

struct WGPUSwapChainImpl;  // Opaque handle from GraphicsEngine

namespace aria {

class GraphicsEngine;

// ── Font ─────────────────────────────────────────────────────────

/// Font properties for text rendering via Skia textblobs.
struct Font {
    std::string family = "sans-serif";
    float size         = 14.0f;
};

// ── SkiaCanvas ───────────────────────────────────────────────────

/// GPU-accelerated 2D canvas backed by Skia Ganesh + Dawn WebGPU.
///
/// Wraps a `GrDawnBackendContext` and `SkSurface` so that all drawing
/// commands go through Skia's GPU pipeline. The surface is backed by
/// a Dawn `WGPUTexture` from the swap chain.
///
/// Design decisions (from design.md):
///   - Uses Skia Ganesh (GPU) backend, not software raster.
///   - Surface is recreated on swap chain resize.
///   - All operations are safe to call on an uninitialised canvas
///     (they become no-ops).
class SkiaCanvas {
public:
    SkiaCanvas();
    ~SkiaCanvas();

    SkiaCanvas(const SkiaCanvas&) = delete;
    SkiaCanvas& operator=(const SkiaCanvas&) = delete;
    SkiaCanvas(SkiaCanvas&&) noexcept;
    SkiaCanvas& operator=(SkiaCanvas&&) noexcept;

    /// Initialise with a GraphicsEngine and a swap chain handle.
    /// Returns true if the Skia GPU context and surface were created.
    bool init(GraphicsEngine* engine, WGPUSwapChainImpl* swapchain);

    /// True after a successful init().
    bool is_ready() const noexcept;

    // ── Drawing ─────────────────────────────────────────────────

    /// Fill the entire surface with a solid colour.
    void clear(const Color& c);

    /// Draw a (possibly rounded) rectangle.
    void drawRect(const Rect& r, const Paint& p);

    /// Draw a circle at (cx, cy) with the given radius.
    void drawCircle(float cx, float cy, float radius, const Paint& p);

    /// Draw a text blob.
    void drawTextBlob(const char* text, const Font& font,
                      float x, float y, const Paint& p);

    /// Flush all pending Skia GPU commands to Dawn for presentation.
    void flush();

    // ── State stack ─────────────────────────────────────────────

    void save();
    void restore();
    void clipRect(const Rect& r);

    // ── Surface management ─────────────────────────────────────

    /// Recreate the GPU surface after swap chain resize.
    /// Returns true on success.
    bool recreate_surface(uint32_t width, uint32_t height);

    // ── Profiling ──────────────────────────────────────────────

    /// Number of draw calls since the last flush.
    uint32_t draw_call_count() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aria

#endif // ARIA_SKIA_CANVAS_H
