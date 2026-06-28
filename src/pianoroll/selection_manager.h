#ifndef ARIA_PIANOROLL_SELECTION_MANAGER_H
#define ARIA_PIANOROLL_SELECTION_MANAGER_H

#include "note.h"
#include "note_collection.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace aria {

// Forward declarations.
struct ViewTransform;

/// Manages note selection, handles, and copy/paste operations.
///
/// Works alongside NoteCollection — reads/writes notes but maintains
/// its own selection set and drag state.
class SelectionManager {
public:
    // ─── Handle types ────────────────────────────────────────

    enum class Handle : uint8_t {
        None,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    // ─── Selection management ────────────────────────────────

    void select_note(NoteID id);
    void deselect_note(NoteID id);
    void toggle_note(NoteID id);
    void select_all(NoteCollection& notes);
    void clear();
    void select_marquee(const Rect& ppqn_rect, NoteCollection& notes);
    void select_range(uint64_t start_ppqn, uint64_t end_ppqn,
                      uint8_t key_low, uint8_t key_high,
                      NoteCollection& notes);
    bool is_selected(NoteID id) const;
    const std::unordered_set<NoteID>& selected() const { return selected_; }
    size_t count() const { return selected_.size(); }

    // ─── Hit testing ─────────────────────────────────────────

    /// Test if the mouse is over a resize handle on the given note.
    Handle hit_test(const Note& note, float sx, float sy,
                    const ViewTransform& view) const;

    // ─── Drag operations ─────────────────────────────────────

    /// Start a drag operation, saving initial positions.
    void start_drag(const ViewTransform& view);

    /// Update all selected notes during drag.
    void update_drag(int64_t delta_ppqn, int8_t delta_key);

    /// Finalize drag and rebuild spatial index.
    void end_drag();

    // ─── Copy / paste ────────────────────────────────────────

    /// Copy selected notes into a vector for external storage.
    std::vector<Note> copy_selection(const NoteCollection& notes) const;

    /// Paste notes from a clipboard, offset by position and key.
    void paste_notes(uint64_t position, uint8_t base_key,
                     NoteCollection& notes);

    // ─── Bulk edit ───────────────────────────────────────────

    void move_selection(int64_t delta_ppqn, int8_t delta_key,
                        NoteCollection& notes);
    void delete_selection(NoteCollection& notes);
    void duplicate_selection(uint64_t offset_ppqn,
                              NoteCollection& notes);
    void multiply_velocity_selection(double factor,
                                      NoteCollection& notes);
    void transpose_selection(int8_t semitones,
                              NoteCollection& notes);

private:
    std::unordered_set<NoteID> selected_;

    // ─── Drag state ──────────────────────────────────────────
    struct DragState {
        bool active = false;
        // Per-note initial positions captured at drag start.
        struct NoteOrig {
            uint64_t start_ppqn;
            uint64_t duration_ppqn;
            uint8_t  key;
        };
        std::vector<std::pair<NoteID, NoteOrig>> origins;
    };
    DragState drag_;
};

} // namespace aria

#endif // ARIA_PIANOROLL_SELECTION_MANAGER_H
