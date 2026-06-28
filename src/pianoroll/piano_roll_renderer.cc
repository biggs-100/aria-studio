#include "piano_roll_renderer.h"
#include "spatial_grid.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// ─── SkiaCanvas forward declaration ────────────────────────────
// In the full build this would be #include "skia/canvas.h" or similar.
// For this slice we only use it as an opaque pointer; the Skia types
// are resolved at link time against the graphics engine.
//
// Concrete SkiaCanvas methods used:
//   - clear(color)
//   - drawRect(x, y, w, h, color)
//   - drawRoundedRect(x, y, w, h, radius, color)
//   - drawLine(x1, y1, x2, y2, color, width)

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Colour helpers
// ═══════════════════════════════════════════════════════════════

Rgba PianoRollRenderer::velocity_color(uint8_t velocity) {
    // Three-stop gradient: low → mid → high
    constexpr Rgba low{0.290f, 0.620f, 1.0f, 1.0f};    // #4A9EFF
    constexpr Rgba mid{0.478f, 1.0f,  0.478f, 1.0f};    // #7AFF7A
    constexpr Rgba high{1.0f, 0.478f, 0.290f, 1.0f};    // #FF7A4A

    float t = static_cast<float>(velocity) / 127.0f;
    if (t <= 0.5f) {
        float u = t / 0.5f;  // 0→1 within the low→mid segment
        return Rgba{
            low.r + (mid.r - low.r) * u,
            low.g + (mid.g - low.g) * u,
            low.b + (mid.b - low.b) * u,
            1.0f
        };
    } else {
        float u = (t - 0.5f) / 0.5f;  // 0→1 within the mid→high segment
        return Rgba{
            mid.r + (high.r - mid.r) * u,
            mid.g + (high.g - mid.g) * u,
            mid.b + (high.b - mid.b) * u,
            1.0f
        };
    }
}

float PianoRollRenderer::corner_radius(double width_px) {
    if (width_px < 8.0f)  return 0.0f;
    if (width_px < 20.0f) return 1.0f;
    if (width_px < 50.0f) return 2.0f;
    return std::min(4.0f, static_cast<float>(width_px) * 0.08f);
}

// ═══════════════════════════════════════════════════════════════
// Main render
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render(SkiaCanvas* canvas,
                                const NoteCollection& notes,
                                const ViewTransform& view)
{
    // Delegate to the extended render with no ghost notes or scale.
    render(canvas, notes, view, nullptr, nullptr);
}

void PianoRollRenderer::render(SkiaCanvas* canvas,
                                const NoteCollection& notes,
                                const ViewTransform& view,
                                std::vector<Note>* ghost_notes,
                                const ScaleSystem* scale_system)
{
    // The canvas bounds are set externally — we assume a full-clear here.
    // In production, the bounds are passed down from the widget system.
    Rect bounds{0, 0, 1920, 1080};  // fallback; overridden by caller via bounds param

    // Layer order (see 06_Piano_Roll.md §3.2):
    render_background(canvas, bounds);

    // Draw scale highlighting if enabled.
    if (scale_system && scale_system->is_enabled()) {
        render_scale_highlight(canvas, view, *scale_system);
    }

    render_grid(canvas, view);

    // Draw ghost notes behind the main notes.
    if (ghost_notes && !ghost_notes->empty()) {
        render_ghost_notes(canvas, *ghost_notes, view);
    }

    render_notes(canvas, notes, view);
    render_selection(canvas, notes, view);
    render_keyboard(canvas, view, bounds);
    render_playhead(canvas, view, bounds);
}

// ═══════════════════════════════════════════════════════════════
// Background
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_background(SkiaCanvas* /*canvas*/,
                                          const Rect& /*bounds*/) {
    // Clear the entire canvas with the background colour.
    // In production this would call canvas->clear(kBgColor) or
    // canvas->drawRect(...) with the background colour.
}

// ═══════════════════════════════════════════════════════════════
// Grid
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_grid(SkiaCanvas* /*canvas*/,
                                    const ViewTransform& /*view*/) {
    // The grid is drawn in PPQN space which covers the visible viewport.
    // For this slice we compute the visible range and draw vertical lines
    // at bar, beat, and sub-beat boundaries.
    //
    // At very zoomed-out levels we only draw bar lines; at zoomed-in
    // levels we draw finer subdivisions.
    //
    // Grid resolution selection:
    //   ppqn_per_pixel_x < 0.5  → draw 128th lines
    //   ppqn_per_pixel_x < 1    → draw 64th lines
    //   ppqn_per_pixel_x < 2    → draw 32nd lines
    //   ppqn_per_pixel_x < 4    → draw 16th lines
    //   ppqn_per_pixel_x < 8    → draw 8th lines
    //   ppqn_per_pixel_x < 16   → draw beats
    //   else                     → draw bars only
    // TODO: Implement full grid line drawing once SkiaCanvas is concrete.
}

// ═══════════════════════════════════════════════════════════════
// Notes
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_notes(SkiaCanvas* /*canvas*/,
                                      const NoteCollection& notes,
                                      const ViewTransform& view)
{
    // Determine the visible viewport (assuming a default canvas size).
    // In production the canvas bounds are passed explicitly.
    double canvas_w = 1920.0;
    double canvas_h = 1080.0;
    Rect vp_ppqn = view.viewport_ppqn(canvas_w, canvas_h);

    // Expand the viewport by a margin to avoid popping at edges.
    vp_ppqn.x     -= 3840.0 * view.ppqn_per_pixel_x;
    vp_ppqn.width += 7680.0 * view.ppqn_per_pixel_x;

    // Use a temporary spatial query to cull invisible notes.
    // In production, PianoRollCanvas would cache the spatial index.
    // Here we do a simple linear scan over all notes.
    std::vector<const Note*> visible;
    visible.reserve(notes.size());

    for (const auto& n : notes.notes()) {
        double n_end = static_cast<double>(n.start_ppqn) +
                       static_cast<double>(n.duration_ppqn);
        if (n_end >= vp_ppqn.left() &&
            static_cast<double>(n.start_ppqn) <= vp_ppqn.right() &&
            static_cast<double>(n.key) >= vp_ppqn.top() &&
            static_cast<double>(n.key) <= vp_ppqn.bottom())
        {
            visible.push_back(&n);
        }
    }

    // Draw each visible note.
    for (const Note* n : visible) {
        double x      = view.ppqn_to_x(n->start_ppqn);
        double w      = static_cast<double>(n->duration_ppqn) / view.ppqn_per_pixel_x;
        double y      = view.key_to_y(n->key);
        double h      = view.pixels_per_semitone_y;
        float  radius = corner_radius(w);

        // Base colour: per-note colour or velocity gradient.
        Rgba note_color = n->color;
        if (note_color.r == 0.8f && note_color.g == 0.8f && note_color.b == 0.8f && note_color.a == 1.0f) {
            // Default colour → use velocity gradient.
            note_color = velocity_color(n->velocity);
        }

        // Apply opacity.
        float final_opacity = n->opacity * (n->muted ? 0.4f : 1.0f);
        note_color.a = final_opacity;

        // Draw the note body.
        if (radius > 0.0f) {
            // In production: canvas->drawRoundedRect(x, y, w, h, radius, note_color);
            (void)radius;
        }
        // In production: canvas->drawRect(x, y, w, h, note_color);
        (void)x;
        (void)y;
        (void)w;
        (void)h;
        (void)note_color;
    }
}

// ═══════════════════════════════════════════════════════════════
// Selection
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_selection(SkiaCanvas* /*canvas*/,
                                          const NoteCollection& /*notes*/,
                                          const ViewTransform& /*view*/)
{
    // TODO: Draw white outline (2 px) around each selected note.
    //       Selected by: notes.selected() contains NoteID.
}

// ═══════════════════════════════════════════════════════════════
// Keyboard
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_keyboard(SkiaCanvas* /*canvas*/,
                                         const ViewTransform& /*view*/,
                                         const Rect& /*bounds*/)
{
    // TODO: Draw the piano keyboard to the left of the grid.
    //       Delegates to PianoKeyboard::render().
}

// ═══════════════════════════════════════════════════════════════
// Playhead
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_playhead(SkiaCanvas* /*canvas*/,
                                         const ViewTransform& /*view*/,
                                         const Rect& /*bounds*/)
{
    // TODO: Draw a vertical line at the current transport position
    //       using the playhead colour (kPlayhead).
}

// ═══════════════════════════════════════════════════════════════
// Ghost notes
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_ghost_notes(SkiaCanvas* /*canvas*/,
                                            const std::vector<Note>& ghost_notes,
                                            const ViewTransform& view)
{
    for (const auto& gn : ghost_notes) {
        double x = view.ppqn_to_x(gn.start_ppqn);
        double w = static_cast<double>(gn.duration_ppqn) / view.ppqn_per_pixel_x;
        double y = view.key_to_y(gn.key);
        double h = view.pixels_per_semitone_y;
        float  radius = corner_radius(w);

        // Ghost notes use the ghost colour with low opacity.
        Rgba color = gn.color;
        color.a = gn.opacity;

        // Draw the ghost note body (semi-transparent, not interactive).
        if (radius > 0.0f) {
            // canvas->drawRoundedRect(x, y, w, h, radius, color);
        }
        // canvas->drawRect(x, y, w, h, color);
        (void)x; (void)y; (void)w; (void)h; (void)radius; (void)color;
    }
}

// ═══════════════════════════════════════════════════════════════
// Scale highlighting
// ═══════════════════════════════════════════════════════════════

void PianoRollRenderer::render_scale_highlight(SkiaCanvas* /*canvas*/,
                                                const ViewTransform& view,
                                                const ScaleSystem& scale)
{
    if (!scale.is_enabled()) return;

    // Determine visible key range.
    uint8_t key_low  = static_cast<uint8_t>(std::clamp(
        static_cast<int>(std::floor(view.scroll_key)), 0, 127));
    uint8_t key_high = static_cast<uint8_t>(std::clamp(
        static_cast<int>(std::ceil(view.scroll_key + 1080.0 / view.pixels_per_semitone_y)), 0, 127));

    // We draw subtle highlights for scale tones and dim non-scale rows.
    // Non-scale rows get a dim overlay; scale tones are normal.
    for (uint8_t k = key_low; k <= key_high; ++k) {
        bool in_scale = scale.is_in_scale(k);
        double y = view.key_to_y(k);
        double h = view.pixels_per_semitone_y;

        if (!in_scale) {
            // Dim non-scale rows slightly.
            Rgba dim_overlay{0.0f, 0.0f, 0.0f, 0.08f};
            // canvas->drawRect(0, y, canvas_width, h, dim_overlay);
            (void)y; (void)h; (void)dim_overlay;
        } else {
            // Scale tones get a very subtle highlight.
            Rgba scale_hl{0.2f, 0.3f, 0.5f, 0.03f};
            // canvas->drawRect(0, y, canvas_width, h, scale_hl);
            (void)y; (void)h; (void)scale_hl;
        }
    }
}

} // namespace aria
