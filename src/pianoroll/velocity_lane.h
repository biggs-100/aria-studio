#ifndef ARIA_PIANOROLL_VELOCITY_LANE_H
#define ARIA_PIANOROLL_VELOCITY_LANE_H

#include "note.h"
#include "note_collection.h"

#include <cstdint>
#include <vector>

namespace aria {

// Forward declarations.
struct ViewTransform;
class SkiaCanvas;
struct Theme;

/// Velocity lane displayed below the piano roll grid.
///
/// Shows velocity bars for every visible note and provides editing
/// operations: set, ramp, scale, randomize, reverse, and curve application.
class VelocityLane {
public:
    /// Curve types for the apply_curve operation.
    enum class CurveType : uint8_t {
        Linear,
        Exponential,
        Logarithmic,
        SCurve
    };

    // ─── Data binding ─────────────────────────────────────────

    void set_notes(NoteCollection* notes) { notes_ = notes; }
    NoteCollection* notes() const { return notes_; }

    // ─── Rendering ────────────────────────────────────────────

    /// Render velocity bars within the given bounds rectangle.
    /// Iterates over visible notes and draws a bar for each.
    void render(SkiaCanvas* canvas, const Rect& bounds,
                const ViewTransform& view);

    // ─── Individual editing ───────────────────────────────────

    /// Set the velocity of a single note.
    void set_velocity(NoteID id, uint8_t velocity);

    // ─── Bulk editing ─────────────────────────────────────────

    /// Ramp velocity from start_vel to end_vel across the selected notes.
    void ramp_selection(const std::vector<Note*>& notes,
                        uint8_t start_vel, uint8_t end_vel);

    /// Scale all velocities in the selection by a factor (0.0–2.0).
    void scale_selection(const std::vector<Note*>& notes, double factor);

    /// Randomize velocities within [min, max] for the selected notes.
    void randomize_selection(const std::vector<Note*>& notes,
                            uint8_t min, uint8_t max);

    /// Reverse the velocity order of the selected notes.
    void reverse_selection(const std::vector<Note*>& notes);

    /// Apply a curve shape from start to end velocity across the selection.
    void apply_curve(const std::vector<Note*>& notes,
                     uint8_t start, uint8_t end, CurveType curve);

    // ─── Mouse input ──────────────────────────────────────────

    void on_mouse_down(float x, float y, const ViewTransform& view);
    void on_mouse_move(float x, float y, const ViewTransform& view);
    void on_mouse_up();

    // ─── Layout ───────────────────────────────────────────────

    float height() const { return height_; }
    void  set_height(float h) { height_ = h; }

private:
    NoteCollection* notes_ = nullptr;
    float height_ = 80.0f;

    // ─── Drag state ──────────────────────────────────────────
    struct DragState {
        bool     active   = false;
        NoteID   note     = {};
        uint8_t  velocity = 100;
    };
    DragState drag_;

    // ─── Helpers ──────────────────────────────────────────────

    /// Compute a velocity-bar colour (blue→green→red gradient).
    static Rgba velocity_color(uint8_t velocity);

    /// Apply the normalised curve factor t (0.0–1.0) using the given shape.
    static double curve_factor(double t, CurveType type);
};

} // namespace aria

#endif // ARIA_PIANOROLL_VELOCITY_LANE_H
