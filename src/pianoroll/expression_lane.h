#ifndef ARIA_PIANOROLL_EXPRESSION_LANE_H
#define ARIA_PIANOROLL_EXPRESSION_LANE_H

#include "note.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aria {

// Forward declarations from the graphics layer and piano roll.
class SkiaCanvas;
struct Theme;
struct ViewTransform;

// ─── Vec2 ─────────────────────────────────────────────────────────

/// Simple 2D vector used for line-strip / curve rendering.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

// ─── InterpolationType ────────────────────────────────────────────

/// How a curve segment transitions from one point to the next.
enum class InterpolationType : uint8_t {
    Step,        // Instant change at point (square wave)
    Linear,      // Straight line between points
    Bezier,      // Cubic bezier curve
    Smooth,      // Smooth Catmull-Rom spline
    EaseIn,      // Quadratic ease in
    EaseOut,     // Quadratic ease out
    EaseInOut,   // Smooth ease
    Hold         // Hold value until next point
};

// ─── ExpressionCurve ──────────────────────────────────────────────

/// A single curve used by an expression lane — stores editable points
/// and can evaluate/interpolate at any PPQN position.
class ExpressionCurve {
public:
    // ─── Point ──────────────────────────────────────────────────

    /// A control point on the expression curve.
    struct Point {
        uint64_t         ppqn          = 0;
        double           value         = 0.0;   // Normalized 0.0–1.0
        InterpolationType interpolation = InterpolationType::Linear;

        // Bezier control points (only used when interpolation == Bezier)
        double control_x1 = 0.0, control_y1 = 0.0;
        double control_x2 = 0.0, control_y2 = 0.0;
    };

    // ─── Mutators ─────────────────────────────────────────────

    /// Add a point. If a point already exists at the given PPQN it is
    /// replaced (last-write-wins).
    void add_point(uint64_t ppqn, double value,
                   InterpolationType interp = InterpolationType::Linear);

    /// Remove the point closest to the given PPQN (within a tolerance).
    bool remove_point(uint64_t ppqn, uint64_t tolerance_ppqn = 48);

    /// Move an existing point to a new PPQN / value.
    bool move_point(uint64_t old_ppqn, uint64_t new_ppqn, double new_value);

    /// Set the interpolation type for the point at the given PPQN.
    bool set_interpolation(uint64_t ppqn, InterpolationType type);

    /// Remove all points.
    void clear();

    // ─── Evaluation ───────────────────────────────────────────

    /// Evaluate the curve at an arbitrary PPQN position.
    /// Returns 0.0 if there are no points.
    double evaluate(uint64_t ppqn) const;

    /// Generate screen-space line strip for rendering the curve within
    /// the given PPQN range.
    /// @param ppqn_per_px  PPQN per screen pixel (i.e. ViewTransform::ppqn_per_pixel_x).
    /// @param val_per_px   Pixels per unit of value (i.e. lane_height / 1.0).
    std::vector<Vec2> generate_line_strip(uint64_t start_ppqn,
                                          uint64_t end_ppqn,
                                          double ppqn_per_px,
                                          double val_per_px) const;

    // ─── Accessors ─────────────────────────────────────────────

    const std::vector<Point>& points() const { return points_; }
    bool empty() const { return points_.empty(); }
    size_t size() const { return points_.size(); }

private:
    std::vector<Point> points_;

    /// Find the index of the last point with ppqn <= target.
    /// Returns -1 if no such point exists.
    int find_prev_index(uint64_t ppqn) const;
};

// ─── ExpressionLane ──────────────────────────────────────────────

/// A single expression lane displayed below the note grid.
///
/// Each lane shows a curve that can be edited with mouse interactions.
/// Different lane types interpret the curve data differently (pitch bend
/// values, CC values, pressure, etc.).
class ExpressionLane {
public:
    /// Lane data-source type.
    enum Type : uint8_t {
        PitchBend      = 0,
        CC             = 1,
        ChannelPressure = 2,
        PolyPressure   = 3,
        MPETimbre      = 4,
        MPEPressure    = 5,
        Automation     = 6,
        Custom         = 7
    };

    // ─── Configuration ────────────────────────────────────────

    void set_type(Type type, uint8_t cc_number = 0);
    Type type() const { return type_; }
    uint8_t cc_number() const { return cc_number_; }

    /// Human-readable label for this lane (e.g. "Pitch Bend", "CC 74").
    std::string label() const;

    // ─── Curve access ─────────────────────────────────────────

    ExpressionCurve&       curve()       { return curve_; }
    const ExpressionCurve& curve() const { return curve_; }

    // ─── Rendering ────────────────────────────────────────────

    /// Render this lane into the given bounds rectangle.
    void render(SkiaCanvas* canvas, const Rect& bounds,
                const ViewTransform& view, const Theme* theme);

    // ─── Mouse editing ────────────────────────────────────────

    void on_mouse_down(float x, float y, const ViewTransform& view);
    void on_mouse_move(float x, float y, const ViewTransform& view);
    void on_mouse_up();

    // ─── Layout ───────────────────────────────────────────────

    float height() const { return height_; }
    void  set_height(float h) { height_ = h; }

private:
    Type   type_      = PitchBend;
    uint8_t cc_number_ = 0;
    float  height_    = 60.0f;

    ExpressionCurve curve_;

    // ─── Mouse drag state ────────────────────────────────────
    struct DragState {
        bool    active          = false;
        bool    dragging_point  = false;
        uint64_t point_ppqn     = 0;
        double   start_value    = 0.0;
        float    start_y        = 0.0f;
        bool    creating_point  = false;
    };
    DragState drag_;
};

// ─── ExpressionLaneManager ────────────────────────────────────────

/// Manages a list of expression lanes rendered below the piano roll grid.
class ExpressionLaneManager {
public:
    // ─── Lane management ──────────────────────────────────────

    void add_lane(ExpressionLane::Type type, uint8_t cc = 0);
    void remove_lane(uint32_t index);
    void clear();
    ExpressionLane* get_lane(uint32_t index);
    const ExpressionLane* get_lane(uint32_t index) const;
    uint32_t lane_count() const { return static_cast<uint32_t>(lanes_.size()); }

    // ─── Rendering ────────────────────────────────────────────

    /// Render all lanes stacked vertically, starting at `bounds.y`.
    /// Each lane occupies its own height and draws into a sub-rect.
    /// Returns the total height consumed by all lanes.
    float render_all(SkiaCanvas* canvas, const Rect& bounds,
                     const ViewTransform& view, const Theme* theme);

    // ─── Input routing ────────────────────────────────────────

    /// Route a mouse-down event to the lane at the given absolute
    /// y-offset. Returns the lane index that handled the event, or -1.
    int handle_mouse_down(float x, float y, const ViewTransform& view);

    /// Route a mouse-move event to the currently active lane.
    void handle_mouse_move(float x, float y, const ViewTransform& view);

    /// Route a mouse-up event to the currently active lane and reset state.
    void handle_mouse_up();

    // ─── Layout ───────────────────────────────────────────────

    /// Total height consumed by all lanes combined.
    float total_height() const;

private:
    std::vector<std::unique_ptr<ExpressionLane>> lanes_;

    // ─── Input state ─────────────────────────────────────────
    int  active_lane_  = -1;
    bool drag_active_  = false;
    float active_lane_y_offset_ = 0.0f;
};

} // namespace aria

#endif // ARIA_PIANOROLL_EXPRESSION_LANE_H
