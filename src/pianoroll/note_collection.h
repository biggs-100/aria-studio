#ifndef ARIA_PIANOROLL_NOTE_COLLECTION_H
#define ARIA_PIANOROLL_NOTE_COLLECTION_H

#include "note.h"
#include "spatial_grid.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aria {

/// Manages the full set of notes in a piano roll clip.
///
/// Provides CRUD operations, spatial queries via an integrated SpatialGrid,
/// selection management, and bulk transformation (transpose, shift, velocity).
///
/// Thread safety: not guaranteed — all operations must be called from the
/// main / UI thread.
class NoteCollection {
public:
    // ─── CRUD ─────────────────────────────────────────────────

    /// Add a note. An ID is assigned if `note.id` is zero.
    /// @return The assigned NoteID.
    NoteID add(const Note& note);

    /// Remove a note by ID.
    void remove(NoteID id);

    /// Replace the note at `id` with new data.
    void update(NoteID id, const Note& note);

    /// Remove all notes.
    void clear();

    /// Pre-allocate storage for `capacity` notes.
    void reserve(size_t capacity);

    // ─── Queries ──────────────────────────────────────────────

    /// Find a note by ID (nullptr if not found).
    Note* find(NoteID id);

    /// Const overload.
    const Note* find(NoteID id) const;

    /// Find notes overlapping a time × pitch rectangle.
    std::vector<Note*> find_in_range(uint64_t start_ppqn, uint64_t end_ppqn,
                                     uint8_t key_low = 0, uint8_t key_high = 127);

    /// Find all notes at a given key.
    std::vector<Note*> find_by_key(uint8_t key);

    // ─── Spatial index ────────────────────────────────────────

    /// Rebuild the spatial index from current notes.
    void rebuild_spatial_index();

    /// Query notes visible in the given PPQN viewport rectangle.
    std::vector<Note*> query_rect(const Rect& viewport_ppqn);

    // ─── Selection ────────────────────────────────────────────

    void select(NoteID id);
    void deselect(NoteID id);
    void select_all();
    void clear_selection();
    void toggle_selection(NoteID id);
    const std::unordered_set<NoteID>& selected() const { return selected_; }

    // ─── Bulk operations ──────────────────────────────────────

    void transpose(int8_t semitones);
    void shift_horizontal(int64_t ppqn);
    void multiply_velocity(double factor);
    void sort_by_start();

    // ─── Capacity ─────────────────────────────────────────────

    size_t size() const { return notes_.size(); }
    bool   empty() const { return notes_.empty(); }

    // ─── Raw access (for rendering / serialisation) ───────────

    const std::vector<Note>& notes() const { return notes_; }
    std::vector<Note>&       notes()       { return notes_; }

private:
    std::vector<Note>                     notes_;
    std::unordered_map<uint64_t, size_t>  id_index_;  // NoteID.value → vector index
    std::unordered_set<NoteID>            selected_;
    SpatialGrid                           spatial_;

    /// Generate a unique NoteID.
    NoteID next_id();

    /// Current ID generation state.
    uint64_t id_counter_ = 1;

    /// Rebuild the id_index_ map from scratch.
    void rebuild_index();

    /// Mark spatial index as needing rebuild.
    bool spatial_dirty_ = true;
};

} // namespace aria

#endif // ARIA_PIANOROLL_NOTE_COLLECTION_H
