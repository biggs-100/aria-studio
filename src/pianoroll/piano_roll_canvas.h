#ifndef ARIA_PIANOROLL_PIANO_ROLL_CANVAS_H
#define ARIA_PIANOROLL_PIANO_ROLL_CANVAS_H

#include "expression_lane.h"
#include "note_collection.h"
#include "selection_manager.h"
#include "snap_system.h"
#include "spatial_grid.h"
#include "velocity_lane.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace aria {

// Forward declarations from the graphics layer.
class SkiaCanvas;
struct Theme;

// ─── Tool enum ─────────────────────────────────────────────────

/// Available editing tools for the piano roll.
enum class Tool : uint8_t {
    Pencil   = 0,
    Select   = 1,
    Paint    = 2,
    Erase    = 3,
    Cut      = 4,
    Glue     = 5,
    Ramp     = 6,
    Mute     = 7,
    Zoom     = 8,
    Measure  = 9
};

// ─── ViewTransform ─────────────────────────────────────────────

/// Maps between PPQN / key space and screen pixel space.
struct ViewTransform {
    double ppqn_per_pixel_x     = 1.0;    // PPQN per screen pixel (horizontal zoom)
    double pixels_per_semitone_y = 8.0;   // Pixels per semitone (vertical zoom)
    double scroll_ppqn          = 0.0;    // Horizontal scroll offset in PPQN
    double scroll_key           = 0.0;    // Vertical scroll offset in semitones

    // ─── Conversions ───────────────────────────────────────

    double ppqn_to_x(uint64_t ppqn) const {
        return (static_cast<double>(ppqn) - scroll_ppqn) / ppqn_per_pixel_x;
    }

    uint64_t x_to_ppqn(double x) const {
        return static_cast<uint64_t>(std::max(0.0, x * ppqn_per_pixel_x + scroll_ppqn));
    }

    double key_to_y(uint8_t key) const {
        return (static_cast<double>(key) - scroll_key) * pixels_per_semitone_y;
    }

    uint8_t y_to_key(double y) const {
        int k = static_cast<int>(std::round(y / pixels_per_semitone_y + scroll_key));
        return static_cast<uint8_t>(std::clamp(k, 0, 127));
    }

    // ─── Viewport ──────────────────────────────────────────

    /// Return the visible PPQN rectangle given a pixel canvas size.
    Rect viewport_ppqn(double canvas_w, double canvas_h) const {
        return Rect{
            scroll_ppqn,
            scroll_key,
            canvas_w * ppqn_per_pixel_x,
            canvas_h / pixels_per_semitone_y
        };
    }
};

// Forward declaration for ToolManager.
class ToolManager;
struct MouseEvent;

// ─── PianoRollCanvas ───────────────────────────────────────────

/// Top-level piano roll editor canvas.
///
/// Owns the note collection, spatial index, view transform, tool
/// system, selection manager, snap system, velocity lane, and
/// expression lane manager. Delegates rendering to PianoRollRenderer
/// for the note grid, and renders/edits lanes directly.
class PianoRollCanvas {
public:
    PianoRollCanvas();
    ~PianoRollCanvas();

    // ─── Clip binding ──────────────────────────────────────

    /// Bind a MIDI clip to edit. The clip's notes are imported into
    /// the local NoteCollection.
    void set_clip(class MidiClip* clip);

    // ─── Viewport ───────────────────────────────────────────

    void set_viewport(uint64_t start_ppqn, uint64_t end_ppqn,
                      uint8_t key_low, uint8_t key_high);
    void zoom(float factor, uint64_t anchor_ppqn);
    void scroll_to(uint64_t ppqn);

    // ─── Rendering ─────────────────────────────────────────

    /// Render the piano roll note grid, lanes, and overlays.
    /// `bounds` defines the full area available. The note grid area
    /// occupies the top portion; lanes render below it.
    void render(SkiaCanvas* canvas, const Rect& bounds);

    // ─── Input ─────────────────────────────────────────────

    void on_mouse_down(float x, float y, bool alt = false,
                       bool ctrl = false, bool shift = false);
    void on_mouse_move(float x, float y);
    void on_mouse_up(float x, float y);
    void on_double_click(float x, float y);
    void on_scroll(float dx, float dy, bool horizontal);
    void on_key_down(int key_code);

    // ─── Accessors ─────────────────────────────────────────

    NoteCollection&       note_collection()       { return notes_; }
    const NoteCollection& note_collection() const { return notes_; }
    ViewTransform&        view_transform()        { return view_; }
    const ViewTransform&  view_transform()  const { return view_; }
    Tool                  active_tool()     const { return active_tool_; }
    void                  set_active_tool(Tool t);
    SelectionManager&     selection_manager()       { return selection_; }
    const SelectionManager& selection_manager() const { return selection_; }
    SnapSystem&           snap_system()             { return snap_; }
    const SnapSystem&     snap_system()       const { return snap_; }
    ToolManager&          tool_manager()            { return *tool_mgr_; }
    const ToolManager&    tool_manager()      const { return *tool_mgr_; }

    // ─── Lane access ───────────────────────────────────────

    VelocityLane&             velocity_lane()             { return velocity_lane_; }
    const VelocityLane&       velocity_lane()       const { return velocity_lane_; }
    ExpressionLaneManager&    expression_lanes()          { return expression_lanes_; }
    const ExpressionLaneManager& expression_lanes() const { return expression_lanes_; }

    /// Grid height in pixels (total canvas height minus lane space).
    float grid_height() const { return grid_height_; }
    void  set_grid_height(float h) { grid_height_ = h; }

private:
    NoteCollection        notes_;
    SpatialGrid           spatial_;
    ViewTransform         view_;
    Tool                  active_tool_ = Tool::Select;
    SelectionManager      selection_;
    SnapSystem            snap_;
    std::unique_ptr<ToolManager> tool_mgr_;
    class MidiClip*       bound_clip_   = nullptr;

    // ─── Lanes ──────────────────────────────────────────────

    VelocityLane          velocity_lane_;
    ExpressionLaneManager expression_lanes_;
    float                 grid_height_ = 400.0f;  // Default grid area height

    /// Layout helper: compute the lane rect below the grid.
    Rect lane_bounds(const Rect& full_bounds) const;

    // Mouse event helpers
    void dispatch_mouse_down(float x, float y, bool alt, bool ctrl, bool shift);
};

} // namespace aria

#endif // ARIA_PIANOROLL_PIANO_ROLL_CANVAS_H
