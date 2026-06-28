#ifndef ARIA_PIANOROLL_SPATIAL_GRID_H
#define ARIA_PIANOROLL_SPATIAL_GRID_H

#include "note.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace aria {

/// Grid-based spatial index for fast note lookup during rendering and hit-testing.
///
/// The grid divides the piano roll into cells of 1 bar × 1 octave.
/// Notes are assigned to every cell they overlap, so a long note spanning
/// multiple bars appears in all intersecting cells.
class SpatialGrid {
public:
    /// A single cell covering a time × pitch region.
    struct Cell {
        uint64_t start_ppqn = 0;
        uint64_t end_ppqn   = 0;
        uint8_t  key_low    = 0;
        uint8_t  key_high   = 0;
        std::vector<Note*> notes;
    };

    /// Rebuild the entire spatial index from the given notes.
    void rebuild(const std::vector<Note>& notes);

    /// Query all notes intersecting the given PPQN-space rectangle.
    /// Results are deduplicated — each note appears at most once.
    std::vector<Note*> query(const Rect& ppqn_rect) const;

    /// Remove all cells.
    void clear();

    /// Return the total number of allocated cells (for debugging / metrics).
    size_t cell_count() const { return cells_.size(); }

private:
    static constexpr uint32_t CELL_W_PPQN  = 3840;    // 1 bar
    static constexpr uint8_t  CELL_H_KEYS  = 12;      // 1 octave

    /// Pack (col, row) into a single uint64_t key.
    static uint64_t cell_key(uint32_t col, uint32_t row) {
        return (static_cast<uint64_t>(col) << 32) | static_cast<uint64_t>(row);
    }

    std::unordered_map<uint64_t, Cell> cells_;
};

} // namespace aria

#endif // ARIA_PIANOROLL_SPATIAL_GRID_H
