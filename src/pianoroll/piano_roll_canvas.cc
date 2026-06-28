#include "piano_roll_canvas.h"
#include "piano_roll_renderer.h"
#include "tool_manager.h"
#include "midi/midi_clip.h"

#include <algorithm>
#include <cmath>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════

PianoRollCanvas::PianoRollCanvas()
    : tool_mgr_(std::make_unique<ToolManager>())
{
    // Wire the velocity lane to the note collection.
    velocity_lane_.set_notes(&notes_);
}

PianoRollCanvas::~PianoRollCanvas() = default;

// ═══════════════════════════════════════════════════════════════
// Clip binding
// ═══════════════════════════════════════════════════════════════

void PianoRollCanvas::set_clip(MidiClip* clip) {
    bound_clip_ = clip;
    notes_.clear();

    if (!clip) return;

    // Import notes from the MIDI clip.
    for (const auto& mn : clip->notes()) {
        Note n;
        n.start_ppqn       = mn.start_ppqn;
        n.duration_ppqn    = mn.duration_ppqn;
        n.key              = mn.note;
        n.velocity         = mn.velocity;
        n.channel          = mn.channel;
        n.release_velocity = mn.release_velocity;
        n.mpe              = mn.mpe;
        // Preserve expression events from the MIDI note.
        for (const auto& ev : mn.note_events) {
            NoteExpressionEvent nee;
            nee.type      = static_cast<uint8_t>(ev.type);
            nee.cc_number = ev.data1;
            nee.value     = ev.data2;
            n.expressions.push_back(nee);
        }
        notes_.add(n);
    }

    notes_.rebuild_spatial_index();
}

// ═══════════════════════════════════════════════════════════════
// Viewport
// ═══════════════════════════════════════════════════════════════

void PianoRollCanvas::set_viewport(uint64_t start_ppqn, uint64_t /*end_ppqn*/,
                                    uint8_t key_low, uint8_t /*key_high*/)
{
    view_.scroll_ppqn = static_cast<double>(start_ppqn);
    view_.scroll_key  = static_cast<double>(key_low);
}

void PianoRollCanvas::zoom(float factor, uint64_t anchor_ppqn) {
    double anchor_x = view_.ppqn_to_x(anchor_ppqn);
    view_.ppqn_per_pixel_x = std::clamp(view_.ppqn_per_pixel_x / factor,
                                         0.03125, 64.0);
    // Keep the anchor point stable.
    view_.scroll_ppqn = static_cast<double>(anchor_ppqn) -
                        anchor_x * view_.ppqn_per_pixel_x;
}

void PianoRollCanvas::scroll_to(uint64_t ppqn) {
    view_.scroll_ppqn = static_cast<double>(ppqn);
}

// ═══════════════════════════════════════════════════════════════
// Tool switching
// ═══════════════════════════════════════════════════════════════

void PianoRollCanvas::set_active_tool(Tool t) {
    active_tool_ = t;
    tool_mgr_->set_active(t);
}

// ═══════════════════════════════════════════════════════════════
// Layout helpers
// ═══════════════════════════════════════════════════════════════

Rect PianoRollCanvas::lane_bounds(const Rect& full_bounds) const {
    // Lanes occupy the area below the note grid.
    float lane_area_top = static_cast<float>(full_bounds.y) + grid_height_;
    float lane_total_h  = velocity_lane_.height() +
                          expression_lanes_.total_height();

    return Rect{
        full_bounds.x,
        static_cast<double>(lane_area_top),
        full_bounds.width,
        std::min(static_cast<double>(lane_total_h),
                 full_bounds.y + full_bounds.height - static_cast<double>(lane_area_top))
    };
}

// ═══════════════════════════════════════════════════════════════
// Rendering
// ═══════════════════════════════════════════════════════════════

void PianoRollCanvas::render(SkiaCanvas* canvas, const Rect& bounds) {
    // ── 1. Note grid ─────────────────────────────────────────
    PianoRollRenderer renderer;
    renderer.render(canvas, notes_, view_);

    // ── 2. Grid area rect for the note grid ───────────────────
    Rect grid_area = bounds;
    grid_area.height = static_cast<double>(grid_height_);

    // ── 3. Velocity lane ─────────────────────────────────────
    Rect vp_bounds = lane_bounds(bounds);
    Rect vel_bounds;
    vel_bounds.x      = vp_bounds.x;
    vel_bounds.y      = vp_bounds.y;
    vel_bounds.width  = vp_bounds.width;
    vel_bounds.height = static_cast<double>(velocity_lane_.height());

    velocity_lane_.render(canvas, vel_bounds, view_);

    // ── 4. Expression lanes ──────────────────────────────────
    Rect expr_bounds;
    expr_bounds.x      = vp_bounds.x;
    expr_bounds.y      = vp_bounds.y + vel_bounds.height;
    expr_bounds.width  = vp_bounds.width;
    expr_bounds.height = static_cast<double>(
        expression_lanes_.total_height());

    // Theme is forward-declared — nullptr for now.
    // In production, the active theme is provided by the widget system.
    expression_lanes_.render_all(canvas, expr_bounds, view_, nullptr);
}

// ═══════════════════════════════════════════════════════════════
// Input dispatch
// ═══════════════════════════════════════════════════════════════

void PianoRollCanvas::on_mouse_down(float x, float y, bool alt,
                                     bool ctrl, bool shift)
{
    dispatch_mouse_down(x, y, alt, ctrl, shift);
}

void PianoRollCanvas::on_mouse_move(float x, float y) {
    // Check if we're in the lane area (below the grid).
    float lane_top = grid_height_;
    if (y >= lane_top) {
        float lane_y = y - lane_top;  // offset from top of lane area

        // Route to velocity lane first.
        if (lane_y < velocity_lane_.height()) {
            velocity_lane_.on_mouse_move(x, lane_y, view_);
            return;
        }

        // Route to expression lanes.
        float expr_y = lane_y - velocity_lane_.height();
        expression_lanes_.handle_mouse_move(x, expr_y, view_);
        return;
    }

    // ── Grid routing ─────────────────────────────────────────
    MouseEvent event;
    event.x = x;
    event.y = y;

    tool_mgr_->on_mouse_move(event, view_, notes_, selection_, snap_);
}

void PianoRollCanvas::on_mouse_up(float x, float y) {
    // Finalise any lane drag.
    expression_lanes_.handle_mouse_up();
    velocity_lane_.on_mouse_up();

    MouseEvent event;
    event.x = x;
    event.y = y;

    tool_mgr_->on_mouse_up(event, view_, notes_, selection_, snap_);

    // Rebuild spatial index after editing operations.
    notes_.rebuild_spatial_index();
}

void PianoRollCanvas::on_double_click(float x, float y) {
    MouseEvent event;
    event.x = x;
    event.y = y;
    event.double_click = true;

    tool_mgr_->on_double_click(event, view_, notes_, selection_, snap_);
}

void PianoRollCanvas::on_scroll(float dx, float dy, bool horizontal) {
    if (horizontal) {
        view_.scroll_ppqn += static_cast<double>(dx) * view_.ppqn_per_pixel_x * 3.0;
        view_.scroll_ppqn = std::max(0.0, view_.scroll_ppqn);
    } else {
        view_.scroll_key += static_cast<double>(dy);
        view_.scroll_key = std::max(0.0, view_.scroll_key);
    }
}

void PianoRollCanvas::on_key_down(int key_code) {
    // Keyboard shortcuts for tool switching and editing actions.
    switch (key_code) {
    case '1': set_active_tool(Tool::Pencil);  break;
    case '2': set_active_tool(Tool::Select);  break;
    case '3': set_active_tool(Tool::Paint);   break;
    case '4': set_active_tool(Tool::Erase);   break;
    case '5': set_active_tool(Tool::Cut);     break;
    case '6': set_active_tool(Tool::Glue);    break;
    case '7': set_active_tool(Tool::Ramp);    break;
    case '8': set_active_tool(Tool::Mute);    break;
    case '9': set_active_tool(Tool::Zoom);    break;
    case '0': set_active_tool(Tool::Measure); break;
    case 0x7F:  // Delete
    case 0x08:  // Backspace
        selection_.delete_selection(notes_);
        notes_.rebuild_spatial_index();
        break;
    case 'A': case 'a':
        selection_.select_all(notes_);
        break;
    case 'D': case 'd':
        selection_.clear();
        break;
    case 'M': case 'm':
        // Toggle mute on selected notes.
        for (const auto& id : selection_.selected()) {
            Note* n = notes_.find(id);
            if (n) n->muted = !n->muted;
        }
        break;
    default:
        break;
    }
}

void PianoRollCanvas::dispatch_mouse_down(float x, float y, bool alt,
                                           bool ctrl, bool shift)
{
    // ── Lane routing ─────────────────────────────────────────
    // If the click falls within the lane area, route to the velocity
    // lane or expression lanes based on y position.
    float lane_top = grid_height_;
    if (y >= lane_top) {
        float lane_y = y - lane_top;  // y relative to lane area start

        // Check velocity lane first.
        if (lane_y < velocity_lane_.height()) {
            velocity_lane_.on_mouse_down(x, lane_y, view_);
            return;
        }

        // Check expression lanes (y relative to expression lanes area).
        float expr_y = lane_y - velocity_lane_.height();
        int handled = expression_lanes_.handle_mouse_down(x, expr_y, view_);
        if (handled >= 0) return;

        // Not handled by any lane — fall through to default.
    }

    // ── Grid routing ─────────────────────────────────────────
    MouseEvent event;
    event.x     = x;
    event.y     = y;
    event.alt   = alt;
    event.ctrl  = ctrl;
    event.shift = shift;
    event.left  = true;

    tool_mgr_->on_mouse_down(event, view_, notes_, selection_, snap_);
}

} // namespace aria
