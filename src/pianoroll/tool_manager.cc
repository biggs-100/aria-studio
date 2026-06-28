#include "tool_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════

namespace {

/// Find the note at the given screen position. Returns nullptr if none.
Note* hit_test_note(const ViewTransform& view, float sx, float sy,
                    NoteCollection& notes) {
    uint64_t ppqn = view.x_to_ppqn(static_cast<double>(sx));
    uint8_t  key  = view.y_to_key(static_cast<double>(sy));

    // Query a small window around the click point.
    constexpr uint64_t HIT_MARGIN_PPQN = 30;
    auto hits = notes.find_in_range(
        (ppqn > HIT_MARGIN_PPQN) ? ppqn - HIT_MARGIN_PPQN : 0,
        ppqn + HIT_MARGIN_PPQN,
        (key > 2) ? key - 2 : 0,
        std::min(static_cast<int>(key) + 2, 127));

    // Return the most specific match (smallest start_ppqn distance).
    Note* best = nullptr;
    uint64_t best_dist = std::numeric_limits<uint64_t>::max();
    for (auto* n : hits) {
        uint64_t n_end = n->start_ppqn + n->duration_ppqn;
        if (ppqn >= n->start_ppqn && ppqn < n_end && n->key == key) {
            return n;  // Exact position match wins immediately.
        }
        uint64_t dist = (ppqn > n->start_ppqn)
            ? ppqn - n->start_ppqn
            : n->start_ppqn - ppqn;
        if (dist < best_dist) {
            best_dist = dist;
            best = n;
        }
    }
    return best;
}

/// Find all notes overlapping a screen rectangle.
std::vector<Note*> hit_test_rect(const ViewTransform& view,
                                  float sx1, float sy1,
                                  float sx2, float sy2,
                                  NoteCollection& notes) {
    uint64_t ppqn1 = view.x_to_ppqn(std::min(sx1, sx2));
    uint64_t ppqn2 = view.x_to_ppqn(std::max(sx1, sx2));
    uint8_t  key1  = view.y_to_key(std::min(sy1, sy2));
    uint8_t  key2  = view.y_to_key(std::max(sy1, sy2));

    return notes.find_in_range(ppqn1, ppqn2, key1, key2);
}

/// Snap a PPQN value using the snap system (convenience).
uint64_t snap_ppqn(const SnapSystem& snap, uint64_t ppqn) {
    return snap.is_enabled() ? snap.snap(ppqn) : ppqn;
}

/// Screen X to snapped PPQN.
uint64_t sx_to_snapped_ppqn(const ViewTransform& view, const SnapSystem& snap,
                             float sx) {
    uint64_t raw = view.x_to_ppqn(static_cast<double>(sx));
    return snap_ppqn(snap, raw);
}

/// Screen Y to key.
uint8_t sy_to_key(const ViewTransform& view, float sy) {
    return view.y_to_key(static_cast<double>(sy));
}

static constexpr uint64_t MIN_NOTE_DURATION = 60;  // ~1/64 note

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════
// PencilTool
// ═══════════════════════════════════════════════════════════════

class PencilTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Pen; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        dragging_ = false;

        uint64_t ppqn = sx_to_snapped_ppqn(view, snap, event.x);
        uint8_t  key  = sy_to_key(view, event.y);

        if (event.alt) {
            // Alt+Click: duplicate note at cursor.
            Note* hit = hit_test_note(view, event.x, event.y, notes);
            if (hit) {
                Note dup = *hit;
                dup.id = NoteID{};
                dup.start_ppqn = ppqn;
                dup.key = key;
                NoteID new_id = notes.add(dup);
                drag_note_ = new_id;
                drag_start_ppqn_ = ppqn;
                dragging_ = true;
                last_ppqn_ = ppqn;
                return;
            }
        }

        // Check if clicking on an existing note → move it.
        Note* hit = hit_test_note(view, event.x, event.y, notes);
        if (hit) {
            drag_note_ = hit->id;
            drag_start_ppqn_ = hit->start_ppqn;
            drag_origin_ppqn_ = ppqn;
            drag_origin_key_ = key;
            dragging_ = true;
            last_ppqn_ = ppqn;
            return;
        }

        // Create new note.
        Note n;
        n.start_ppqn    = ppqn;
        n.duration_ppqn = snap.is_enabled() ? snap.resolution() : MIN_NOTE_DURATION;
        n.key           = key;
        n.velocity      = 100;
        NoteID id = notes.add(n);
        drag_note_ = id;
        drag_start_ppqn_ = ppqn;
        dragging_ = true;

        // Give it zero duration initially — will extend on drag.
        if (auto* np = notes.find(id)) {
            np->duration_ppqn = MIN_NOTE_DURATION;
        }
        last_ppqn_ = ppqn;
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        if (!dragging_) return;

        uint64_t ppqn = sx_to_snapped_ppqn(view, snap, event.x);
        uint8_t  key  = sy_to_key(view, event.y);

        Note* np = notes.find(drag_note_);
        if (!np) { dragging_ = false; return; }

        // If we clicked on an existing note, move it.
        // Otherwise, resize the new note.
        if (ppqn >= drag_start_ppqn_) {
            // Extend duration (creating new note).
            np->start_ppqn = drag_start_ppqn_;
            uint64_t dur = ppqn - drag_start_ppqn_;
            np->duration_ppqn = std::max(MIN_NOTE_DURATION, dur);
        } else {
            // Dragging left: start moves left, keep end stable.
            np->start_ppqn = ppqn;
            uint64_t end = drag_start_ppqn_ + np->duration_ppqn;
            if (end > ppqn) {
                np->duration_ppqn = end - ppqn;
            }
        }

        // Allow pitch change on drag for existing notes.
        int dk = static_cast<int>(key) - static_cast<int>(drag_origin_key_);
        if (dk != 0) {
            np->key = static_cast<uint8_t>(
                std::clamp(static_cast<int>(np->key) + dk, 0, 127));
            drag_origin_key_ = key;
        }

        last_ppqn_ = ppqn;
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        dragging_ = false;
    }

    void on_double_click(const MouseEvent& event,
                         const ViewTransform& view,
                         NoteCollection& notes,
                         SelectionManager& selection,
                         SnapSystem& snap) override
    {
        (void)view;
        (void)selection;
        (void)snap;
        // Double-click on a note could open a label editor.
        Note* hit = hit_test_note(view, event.x, event.y, notes);
        if (hit) {
            hit->locked = !hit->locked;  // Toggle lock as a simple action.
        }
    }

private:
    NoteID   drag_note_;
    uint64_t drag_start_ppqn_ = 0;
    uint64_t drag_origin_ppqn_ = 0;
    uint8_t  drag_origin_key_ = 60;
    uint64_t last_ppqn_ = 0;
    bool     dragging_ = false;
};

// ═══════════════════════════════════════════════════════════════
// SelectTool
// ═══════════════════════════════════════════════════════════════

class SelectTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Pointer; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)snap;

        // Hit-test handles first (if any note is selected).
        if (selection.count() > 0) {
            auto sel_ids = selection.selected();
            for (const auto& sid : sel_ids) {
                Note* sn = notes.find(sid);
                if (!sn) continue;
                SelectionManager::Handle h = selection.hit_test(
                    *sn, event.x, event.y, view);
                if (h != SelectionManager::Handle::None) {
                    // Start handle drag.
                    drag_mode_ = DragMode::Handle;
                    handle_type_ = h;
                    handle_note_ = sid;
                    drag_anchor_ppqn_ = sn->start_ppqn;
                    drag_anchor_duration_ = sn->duration_ppqn;
                    drag_anchor_key_ = sn->key;
                    drag_start_x_ = event.x;
                    drag_start_y_ = event.y;
                    return;
                }
            }
        }

        // Check if clicking on a note.
        Note* hit = hit_test_note(view, event.x, event.y, notes);
        if (hit) {
            if (event.shift) {
                selection.toggle_note(hit->id);
            } else if (!selection.is_selected(hit->id)) {
                selection.clear();
                selection.select_note(hit->id);
            }

            // Start drag-move of selection.
            drag_mode_ = DragMode::Move;
            drag_start_x_ = event.x;
            drag_start_y_ = event.y;
            drag_anchor_ppqn_ = view.x_to_ppqn(static_cast<double>(event.x));
            drag_anchor_key_ = view.y_to_key(static_cast<double>(event.y));
            should_deselect_on_click_ = selection.is_selected(hit->id);
            return;
        }

        // Empty space: start marquee.
        if (!event.shift) {
            selection.clear();
        }
        drag_mode_ = DragMode::Marquee;
        marquee_origin_x_ = event.x;
        marquee_origin_y_ = event.y;
        marquee_current_x_ = event.x;
        marquee_current_y_ = event.y;
        should_deselect_on_click_ = false;
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)snap;

        if (drag_mode_ == DragMode::None) return;

        if (drag_mode_ == DragMode::Marquee) {
            marquee_current_x_ = event.x;
            marquee_current_y_ = event.y;
            return;
        }

        if (drag_mode_ == DragMode::Move) {
            // Calculate delta in PPQN/key space from drag anchor.
            uint64_t current_ppqn = view.x_to_ppqn(static_cast<double>(event.x));
            uint8_t  current_key  = view.y_to_key(static_cast<double>(event.y));

            int64_t  delta_ppqn = static_cast<int64_t>(current_ppqn) -
                                   static_cast<int64_t>(drag_anchor_ppqn_);
            int8_t   delta_key  = static_cast<int8_t>(current_key) -
                                   static_cast<int8_t>(drag_anchor_key_);

            selection.start_drag(view);
            selection.update_drag(delta_ppqn, delta_key);
            return;
        }

        if (drag_mode_ == DragMode::Handle) {
            handle_resize(event, view, notes, selection, snap);
        }
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)notes;
        (void)snap;

        if (drag_mode_ == DragMode::Marquee) {
            // Finalize marquee selection.
            float x1 = std::min(marquee_origin_x_, marquee_current_x_);
            float y1 = std::min(marquee_origin_y_, marquee_current_y_);
            float x2 = std::max(marquee_origin_x_, marquee_current_x_);
            float y2 = std::max(marquee_origin_y_, marquee_current_y_);

            uint64_t ppqn1 = view.x_to_ppqn(static_cast<double>(x1));
            uint64_t ppqn2 = view.x_to_ppqn(static_cast<double>(x2));
            uint8_t  key1  = view.y_to_key(static_cast<double>(y1));
            uint8_t  key2  = view.y_to_key(static_cast<double>(y2));

            selection.select_range(ppqn1, ppqn2, key1, key2, notes);
            drag_mode_ = DragMode::None;
            return;
        }

        if (drag_mode_ == DragMode::Move) {
            selection.end_drag();
            drag_mode_ = DragMode::None;
            return;
        }

        if (drag_mode_ == DragMode::Handle) {
            selection.end_drag();
            drag_mode_ = DragMode::None;
            handle_type_ = SelectionManager::Handle::None;
            return;
        }

        // Simple click on selected note with no drag → deselect.
        if (should_deselect_on_click_ && drag_mode_ == DragMode::None) {
            Note* hit = hit_test_note(view, event.x, event.y, notes);
            if (hit) {
                selection.clear();
            }
        }
        drag_mode_ = DragMode::None;
    }

    void on_double_click(const MouseEvent& event,
                         const ViewTransform& view,
                         NoteCollection& notes,
                         SelectionManager& selection,
                         SnapSystem& snap) override
    {
        (void)view;
        (void)snap;
        // Select all notes at the same pitch.
        Note* hit = hit_test_note(view, event.x, event.y, notes);
        if (hit) {
            selection.clear();
            // Find all notes at same pitch.
            for (const auto& n : notes.notes()) {
                if (n.key == hit->key) {
                    selection.select_note(n.id);
                }
            }
        }
    }

private:
    enum class DragMode { None, Move, Marquee, Handle };
    DragMode drag_mode_ = DragMode::None;

    // Marquee state
    float marquee_origin_x_ = 0;
    float marquee_origin_y_ = 0;
    float marquee_current_x_ = 0;
    float marquee_current_y_ = 0;

    // Move / Handle state
    float drag_start_x_ = 0;
    float drag_start_y_ = 0;
    uint64_t drag_anchor_ppqn_ = 0;
    uint8_t  drag_anchor_key_ = 60;
    bool     should_deselect_on_click_ = false;

    // Handle state
    SelectionManager::Handle handle_type_ = SelectionManager::Handle::None;
    NoteID handle_note_;
    uint64_t drag_anchor_duration_ = 0;

    void handle_resize(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap)
    {
        Note* np = notes.find(handle_note_);
        if (!np) return;

        uint64_t current_ppqn = sx_to_snapped_ppqn(view, snap, event.x);
        uint8_t  current_key  = sy_to_key(view, event.y);
        int      dk = static_cast<int>(current_key) -
                      static_cast<int>(drag_anchor_key_);

        (void)np;
        // Delegate to selection for bulk handle operations.
        int64_t dppqn = static_cast<int64_t>(current_ppqn) -
                        static_cast<int64_t>(drag_anchor_ppqn_);

        selection.start_drag(view);

        switch (handle_type_) {
        case SelectionManager::Handle::Left:
            // Move start: shift all selected notes' start by dppqn.
            selection.update_drag(dppqn, 0);
            break;
        case SelectionManager::Handle::Right:
            // Resize end: adjust duration for selected notes.
            selection.update_drag(0, 0);
            break;
        case SelectionManager::Handle::Top:
        case SelectionManager::Handle::TopLeft:
        case SelectionManager::Handle::TopRight:
            selection.update_drag(0, static_cast<int8_t>(dk));
            break;
        case SelectionManager::Handle::Bottom:
        case SelectionManager::Handle::BottomLeft:
        case SelectionManager::Handle::BottomRight:
            selection.update_drag(0, static_cast<int8_t>(dk));
            break;
        default:
            break;
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// PaintTool
// ═══════════════════════════════════════════════════════════════

class PaintTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Paint; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)selection;
        painting_ = true;
        last_grid_ppqn_ = sx_to_snapped_ppqn(view, snap, event.x);

        Note n;
        n.start_ppqn    = last_grid_ppqn_;
        n.duration_ppqn = snap.is_enabled() ? snap.resolution() : MIN_NOTE_DURATION;
        n.key           = sy_to_key(view, event.y);
        n.velocity      = 100;
        notes.add(n);
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)selection;
        if (!painting_) return;

        uint64_t current_grid = sx_to_snapped_ppqn(view, snap, event.x);
        uint8_t  key = sy_to_key(view, event.y);

        if (current_grid != last_grid_ppqn_) {
            Note n;
            n.start_ppqn    = current_grid;
            n.duration_ppqn = snap.is_enabled() ? snap.resolution() : MIN_NOTE_DURATION;
            n.key           = key;
            n.velocity      = 100;
            notes.add(n);
            last_grid_ppqn_ = current_grid;
        }
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        painting_ = false;
    }

private:
    bool painting_ = false;
    uint64_t last_grid_ppqn_ = 0;
};

// ═══════════════════════════════════════════════════════════════
// EraseTool
// ═══════════════════════════════════════════════════════════════

class EraseTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Eraser; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)selection;
        (void)snap;
        erase_rect_.emplace();
        erase_rect_->x = event.x;
        erase_rect_->y = event.y;
        erase_rect_->width = 0;
        erase_rect_->height = 0;
        erasing_ = true;

        // Delete note under cursor immediately.
        Note* hit = hit_test_note(view, event.x, event.y, notes);
        if (hit) {
            notes.remove(hit->id);
        }
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)selection;
        (void)snap;
        if (!erasing_ || !erase_rect_) return;

        // Update erase rect.
        float x1 = erase_rect_->x;
        float y1 = erase_rect_->y;
        float x2 = event.x;
        float y2 = event.y;

        // Delete notes in the region.
        auto hits = hit_test_rect(view, x1, y1, x2, y2, notes);
        for (auto* n : hits) {
            notes.remove(n->id);
        }

        // Update rect to new extent.
        erase_rect_->x = std::min(x1, x2);
        erase_rect_->y = std::min(y1, y2);
        erase_rect_->width = std::abs(x2 - x1);
        erase_rect_->height = std::abs(y2 - y1);
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        erasing_ = false;
        erase_rect_.reset();
    }

private:
    bool erasing_ = false;
    std::optional<Rect> erase_rect_;
};

// ═══════════════════════════════════════════════════════════════
// CutTool
// ═══════════════════════════════════════════════════════════════

class CutTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Cut; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)selection;
        uint64_t cut_ppqn = sx_to_snapped_ppqn(view, snap, event.x);

        // Find all notes at this position and split them.
        auto hits = notes.find_in_range(
            (cut_ppqn > 1) ? cut_ppqn - 1 : 0,
            cut_ppqn + 1,
            0, 127);

        for (auto* n : hits) {
            uint64_t n_end = n->start_ppqn + n->duration_ppqn;
            if (cut_ppqn > n->start_ppqn && cut_ppqn < n_end) {
                // Split: create a new note from cut point to end.
                Note second = *n;
                uint64_t orig_end = n->start_ppqn + n->duration_ppqn;
                n->duration_ppqn = cut_ppqn - n->start_ppqn;
                second.id = NoteID{};
                second.start_ppqn = cut_ppqn;
                second.duration_ppqn = orig_end - cut_ppqn;
                notes.add(second);
            }
        }
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        // Cut tool doesn't need move actions.
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
    }
};

// ═══════════════════════════════════════════════════════════════
// GlueTool
// ═══════════════════════════════════════════════════════════════

class GlueTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Glue; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)selection;
        (void)snap;

        Note* hit = hit_test_note(view, event.x, event.y, notes);
        if (!hit) return;

        // Find overlapping or adjacent notes at the same pitch.
        uint64_t hit_end = hit->start_ppqn + hit->duration_ppqn;

        for (const auto& candidate : notes.notes()) {
            if (candidate.id == hit->id) continue;
            if (candidate.key != hit->key) continue;
            if (candidate.muted != hit->muted) continue;

            uint64_t cand_end = candidate.start_ppqn +
                                candidate.duration_ppqn;

            // Check overlapping or adjacent (within 1 PPQN tolerance).
            bool overlap = (candidate.start_ppqn < hit_end &&
                            cand_end > hit->start_ppqn);
            bool adjacent = (candidate.start_ppqn == hit_end ||
                             cand_end == hit->start_ppqn);

            if (overlap || adjacent) {
                // Merge into hit note.
                uint64_t new_start = std::min(hit->start_ppqn,
                                              candidate.start_ppqn);
                uint64_t new_end = std::max(hit_end, cand_end);
                hit->start_ppqn = new_start;
                hit->duration_ppqn = new_end - new_start;
                // Merge velocity: average weighted by duration.
                float ratio = static_cast<float>(candidate.duration_ppqn) /
                              static_cast<float>(new_end - new_start);
                hit->velocity = static_cast<uint8_t>(
                    std::clamp(
                        static_cast<int>(
                            static_cast<float>(hit->velocity) * (1.0f - ratio) +
                            static_cast<float>(candidate.velocity) * ratio),
                        0, 127));
                notes.remove(candidate.id);
                // Since we removed while iterating, start fresh.
                // For safety, only merge first found.
                break;
            }
        }
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
    }
};

// ═══════════════════════════════════════════════════════════════
// RampTool
// ═══════════════════════════════════════════════════════════════

class RampTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Ramp; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)snap;

        ramp_active_ = true;
        ramp_start_x_ = event.x;
        ramp_start_y_ = event.y;
        ramp_current_x_ = event.x;
        ramp_current_y_ = event.y;

        // Collect notes in the selection (or under cursor if none selected).
        if (selection.count() == 0) {
            Note* hit = hit_test_note(view, event.x, event.y, notes);
            if (hit) {
                ramp_notes_.clear();
                ramp_notes_.push_back(hit->id);
            }
        } else {
            ramp_notes_.clear();
            for (const auto& id : selection.selected()) {
                ramp_notes_.push_back(id);
            }
        }
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)view;
        (void)selection;
        (void)snap;
        if (!ramp_active_) return;
        ramp_current_x_ = event.x;
        ramp_current_y_ = event.y;
        apply_ramp(event.y, notes);
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)view;
        (void)selection;
        (void)snap;
        if (!ramp_active_) return;
        ramp_active_ = false;
        apply_ramp(event.y, notes);
        ramp_notes_.clear();
    }

private:
    bool ramp_active_ = false;
    float ramp_start_x_ = 0;
    float ramp_start_y_ = 0;
    float ramp_current_x_ = 0;
    float ramp_current_y_ = 0;
    std::vector<NoteID> ramp_notes_;

    void apply_ramp(float mouse_y, NoteCollection& notes) {
        if (ramp_notes_.size() < 2) return;

        // Sort notes by start_ppqn.
        std::vector<Note*> sorted;
        for (const auto& id : ramp_notes_) {
            Note* n = notes.find(id);
            if (n) sorted.push_back(n);
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const Note* a, const Note* b) {
                      return a->start_ppqn < b->start_ppqn;
                  });
        if (sorted.size() < 2) return;

        // Map mouse Y to velocity (0-127). screen top = high velocity.
        float start_vel_f = std::clamp(127.0f - ramp_start_y_ * 0.5f, 0.0f, 127.0f);
        float end_vel_f   = std::clamp(127.0f - mouse_y * 0.5f, 0.0f, 127.0f);

        uint8_t start_vel = static_cast<uint8_t>(start_vel_f);
        uint8_t end_vel   = static_cast<uint8_t>(end_vel_f);

        size_t count = sorted.size();
        for (size_t i = 0; i < count; ++i) {
            float t = (count > 1) ? static_cast<float>(i) /
                                     static_cast<float>(count - 1) : 0.0f;
            uint8_t vel = static_cast<uint8_t>(
                std::clamp(
                    static_cast<int>(
                        static_cast<float>(start_vel) * (1.0f - t) +
                        static_cast<float>(end_vel) * t),
                    0, 127));
            sorted[i]->velocity = vel;
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// MuteTool
// ═══════════════════════════════════════════════════════════════

class MuteTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Mute; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)selection;
        (void)snap;
        Note* hit = hit_test_note(view, event.x, event.y, notes);
        if (hit) {
            hit->muted = !hit->muted;
        }
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)event;
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
    }
};

// ═══════════════════════════════════════════════════════════════
// ZoomTool
// ═══════════════════════════════════════════════════════════════

class ZoomTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::ZoomIn; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        zoom_origin_x_ = event.x;
        zoom_origin_y_ = event.y;
        zooming_ = true;
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        if (zooming_) {
            zoom_current_x_ = event.x;
            zoom_current_y_ = event.y;
        }
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        (void)event;
        zooming_ = false;
        // The canvas handles zoom-to-rect externally.
    }

private:
    bool zooming_ = false;
    float zoom_origin_x_ = 0;
    float zoom_origin_y_ = 0;
    float zoom_current_x_ = 0;
    float zoom_current_y_ = 0;
};

// ═══════════════════════════════════════════════════════════════
// MeasureTool
// ═══════════════════════════════════════════════════════════════

class MeasureTool : public ToolHandler {
public:
    Cursor cursor() const override { return Cursor::Measure; }

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        measure_origin_x_ = event.x;
        measure_origin_y_ = event.y;
        measuring_ = true;
    }

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap) override
    {
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        if (measuring_) {
            measure_current_x_ = event.x;
            measure_current_y_ = event.y;
        }
    }

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap) override
    {
        (void)view;
        (void)notes;
        (void)selection;
        (void)snap;
        (void)event;
        measuring_ = false;
    }

    bool measuring() const { return measuring_; }
    float origin_x() const { return measure_origin_x_; }
    float origin_y() const { return measure_origin_y_; }
    float current_x() const { return measure_current_x_; }
    float current_y() const { return measure_current_y_; }

private:
    bool measuring_ = false;
    float measure_origin_x_ = 0;
    float measure_origin_y_ = 0;
    float measure_current_x_ = 0;
    float measure_current_y_ = 0;
};

// ═══════════════════════════════════════════════════════════════
// ToolManager
// ═══════════════════════════════════════════════════════════════

ToolManager::ToolManager() {
    init_handlers();
}

size_t ToolManager::tool_index(Tool t) const {
    return static_cast<size_t>(t);
}

void ToolManager::init_handlers() {
    handlers_[tool_index(Tool::Pencil)]  = std::make_unique<PencilTool>();
    handlers_[tool_index(Tool::Select)]  = std::make_unique<SelectTool>();
    handlers_[tool_index(Tool::Paint)]   = std::make_unique<PaintTool>();
    handlers_[tool_index(Tool::Erase)]   = std::make_unique<EraseTool>();
    handlers_[tool_index(Tool::Cut)]     = std::make_unique<CutTool>();
    handlers_[tool_index(Tool::Glue)]    = std::make_unique<GlueTool>();
    handlers_[tool_index(Tool::Ramp)]    = std::make_unique<RampTool>();
    handlers_[tool_index(Tool::Mute)]    = std::make_unique<MuteTool>();
    handlers_[tool_index(Tool::Zoom)]    = std::make_unique<ZoomTool>();
    handlers_[tool_index(Tool::Measure)] = std::make_unique<MeasureTool>();
}

void ToolManager::set_active(Tool tool) {
    if (tool == active_) return;
    handlers_[tool_index(active_)]->on_deactivate();
    active_ = tool;
    handlers_[tool_index(active_)]->on_activate();
}

Cursor ToolManager::cursor() const {
    return handlers_[tool_index(active_)]->cursor();
}

ToolHandler* ToolManager::handler_for(Tool tool) {
    return handlers_[tool_index(tool)].get();
}

void ToolManager::on_mouse_down(const MouseEvent& event,
                                 const ViewTransform& view,
                                 NoteCollection& notes,
                                 SelectionManager& selection,
                                 SnapSystem& snap)
{
    handlers_[tool_index(active_)]->on_mouse_down(event, view, notes, selection, snap);
}

void ToolManager::on_mouse_move(const MouseEvent& event,
                                 const ViewTransform& view,
                                 NoteCollection& notes,
                                 SelectionManager& selection,
                                 SnapSystem& snap)
{
    handlers_[tool_index(active_)]->on_mouse_move(event, view, notes, selection, snap);
}

void ToolManager::on_mouse_up(const MouseEvent& event,
                               const ViewTransform& view,
                               NoteCollection& notes,
                               SelectionManager& selection,
                               SnapSystem& snap)
{
    handlers_[tool_index(active_)]->on_mouse_up(event, view, notes, selection, snap);
}

void ToolManager::on_double_click(const MouseEvent& event,
                                   const ViewTransform& view,
                                   NoteCollection& notes,
                                   SelectionManager& selection,
                                   SnapSystem& snap)
{
    handlers_[tool_index(active_)]->on_double_click(event, view, notes, selection, snap);
}

} // namespace aria
