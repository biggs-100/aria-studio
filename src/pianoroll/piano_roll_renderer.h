#ifndef ARIA_PIANOROLL_PIANO_ROLL_RENDERER_H
#define ARIA_PIANOROLL_PIANO_ROLL_RENDERER_H

#include "note.h"
#include "note_collection.h"
#include "ghost_notes.h"
#include "scale_system.h"
#include "piano_roll_canvas.h"  // For ViewTransform, Tool

#include <cstdint>
#include <vector>

namespace aria {

// Forward declarations from the graphics layer.
class SkiaCanvas;
struct Theme;

/// GPU-accelerated renderer for the piano roll.
///
/// Renders the background grid, notes (as rounded rectangles via Skia),
/// selection overlays, the keyboard, and the playhead.
///
/// When the full WebGPU pipeline is integrated, this will switch to
/// instanced vertex buffers. For now it uses Skia draw calls.
class PianoRollRenderer {
public:
    /// Main render entry point — draws every layer.
    void render(SkiaCanvas* canvas, const NoteCollection& notes,
                const ViewTransform& view);

    /// Extended render with ghost notes and scale system.
    void render(SkiaCanvas* canvas, const NoteCollection& notes,
                const ViewTransform& view,
                std::vector<Note>* ghost_notes,
                const ScaleSystem* scale_system);

    // ─── Per-pass rendering (public for testability) ────────

    void render_background(SkiaCanvas* canvas, const Rect& bounds);
    void render_grid(SkiaCanvas* canvas, const ViewTransform& view);
    void render_notes(SkiaCanvas* canvas, const NoteCollection& notes,
                      const ViewTransform& view);
    void render_selection(SkiaCanvas* canvas, const NoteCollection& notes,
                          const ViewTransform& view);
    void render_keyboard(SkiaCanvas* canvas, const ViewTransform& view,
                         const Rect& bounds);
    void render_playhead(SkiaCanvas* canvas, const ViewTransform& view,
                         const Rect& bounds);

    // ─── Theme colours (default dark theme) ─────────────────

    static constexpr uint32_t kBgColor      = 0xFF1A1A1A;
    static constexpr uint32_t kGridLine     = 0xFF2A2A2A;
    static constexpr uint32_t kBeatLine     = 0xFF333333;
    static constexpr uint32_t kBarLine      = 0xFF3D3D3D;
    static constexpr uint32_t kPlayhead     = 0xFFFF7A00;
    static constexpr uint32_t kNoteLowVel   = 0xFF4A9EFF;
    static constexpr uint32_t kNoteMidVel   = 0xFF7AFF7A;
    static constexpr uint32_t kNoteHighVel  = 0xFFFF7A4A;
    static constexpr uint32_t kKeyWhite     = 0xFFFFFFFF;
    static constexpr uint32_t kKeyBlack     = 0xFF333333;
    static constexpr uint32_t kSelOutline   = 0xFFFFFFFF;

private:
    /// Render ghost notes (semi-transparent reference notes).
    void render_ghost_notes(SkiaCanvas* canvas,
                            const std::vector<Note>& ghost_notes,
                            const ViewTransform& view);

    /// Render scale highlighting (tinted grid rows for scale tones).
    void render_scale_highlight(SkiaCanvas* canvas,
                                const ViewTransform& view,
                                const ScaleSystem& scale);

    /// Blend a velocity-based colour from the gradient.
    static Rgba velocity_color(uint8_t velocity);

    /// Compute the corner radius for a note based on its screen width.
    static float corner_radius(double width_px);
};

} // namespace aria

#endif // ARIA_PIANOROLL_PIANO_ROLL_RENDERER_H
