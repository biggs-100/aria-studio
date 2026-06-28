#include "spatial_grid.h"

#include <algorithm>
#include <unordered_set>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// SpatialGrid — rebuild
// ═══════════════════════════════════════════════════════════════

void SpatialGrid::rebuild(const std::vector<Note>& notes) {
    cells_.clear();

    for (const auto& note : notes) {
        // Determine which cells this note touches.
        // Use half-open interval: the note occupies columns where
        // start <= col_start * CELL_W and (start + duration - 1) >= col * CELL_W.
        uint32_t col_start = static_cast<uint32_t>(note.start_ppqn / CELL_W_PPQN);
        uint32_t col_end   = (note.duration_ppqn > 0)
            ? static_cast<uint32_t>((note.start_ppqn + note.duration_ppqn - 1) / CELL_W_PPQN)
            : col_start;

        uint32_t row_start = note.key / CELL_H_KEYS;

        for (uint32_t col = col_start; col <= col_end; ++col) {
            uint64_t key = cell_key(col, row_start);
            auto it = cells_.find(key);
            if (it == cells_.end()) {
                Cell cell;
                cell.start_ppqn = static_cast<uint64_t>(col) * CELL_W_PPQN;
                cell.end_ppqn   = cell.start_ppqn + CELL_W_PPQN;
                cell.key_low    = static_cast<uint8_t>(row_start * CELL_H_KEYS);
                cell.key_high   = static_cast<uint8_t>(std::min(
                    static_cast<int>(cell.key_low) + CELL_H_KEYS - 1, 127));
                auto result = cells_.emplace(key, cell);
                it = result.first;
            }
            it->second.notes.push_back(const_cast<Note*>(&note));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// SpatialGrid — query
// ═══════════════════════════════════════════════════════════════

std::vector<Note*> SpatialGrid::query(const Rect& ppqn_rect) const {
    // Convert the query rectangle to cell coordinates.
    uint32_t col_start = static_cast<uint32_t>(
        std::max(0.0, ppqn_rect.left()) / CELL_W_PPQN);
    uint32_t col_end = static_cast<uint32_t>(
        std::max(0.0, ppqn_rect.right()) / CELL_W_PPQN);

    uint32_t row_start = static_cast<uint32_t>(
        std::max(0.0, ppqn_rect.top()) / CELL_H_KEYS);
    uint32_t row_end = static_cast<uint32_t>(
        std::max(0.0, ppqn_rect.bottom()) / CELL_H_KEYS);

    // Collect candidate notes, deduplicated.
    std::unordered_set<Note*> seen;
    std::vector<Note*> result;

    for (uint32_t col = col_start; col <= col_end; ++col) {
        for (uint32_t row = row_start; row <= row_end; ++row) {
            auto it = cells_.find(cell_key(col, row));
            if (it == cells_.end()) continue;

            for (Note* note : it->second.notes) {
                if (seen.insert(note).second) {
                    // Optional: refine with exact overlap test.
                    double note_end = static_cast<double>(note->start_ppqn) +
                                      static_cast<double>(note->duration_ppqn);
                    // Half-open interval: [left, right) × [top, bottom)
                    if (note_end > ppqn_rect.left() &&
                        static_cast<double>(note->start_ppqn) < ppqn_rect.right() &&
                        static_cast<double>(note->key) >= ppqn_rect.top() &&
                        static_cast<double>(note->key) < ppqn_rect.bottom()) {
                        result.push_back(note);
                    }
                }
            }
        }
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
// SpatialGrid — clear
// ═══════════════════════════════════════════════════════════════

void SpatialGrid::clear() {
    cells_.clear();
}

} // namespace aria
