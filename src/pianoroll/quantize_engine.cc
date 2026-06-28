#include "quantize_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════

uint64_t QuantizeEngine::snap_to_grid(uint64_t ppqn, uint32_t grid_ppqn) {
    if (grid_ppqn == 0) return ppqn;
    uint64_t half = grid_ppqn / 2;
    return ((ppqn + half) / grid_ppqn) * grid_ppqn;
}

double QuantizeEngine::apply_swing(uint64_t ppqn, const QuantizeSettings& s) {
    if (s.swing <= 0.0 || s.grid_ppqn == 0) {
        return static_cast<double>(ppqn);
    }

    // Swing shifts every other grid division by a percentage.
    uint64_t half_grid = s.grid_ppqn / 2;
    uint64_t grid_pos = ppqn / s.grid_ppqn;
    uint64_t offset   = ppqn % s.grid_ppqn;

    // Apply swing to even-numbered divisions (off-beat).
    double result = static_cast<double>(grid_pos * s.grid_ppqn);
    if (grid_pos % 2 == 1) {
        // Off-beat: shift forward by swing amount.
        result += static_cast<double>(half_grid) * (1.0 + s.swing);
        // Preserve intra-grid offset proportionally.
        if (s.preserve_duration) {
            result += static_cast<double>(offset) * (1.0 + s.swing * 0.5);
        }
    } else {
        result += static_cast<double>(offset);
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
// Quantize
// ═══════════════════════════════════════════════════════════════

Note QuantizeEngine::quantize_note(const Note& note,
                                   const QuantizeSettings& settings)
{
    Note result = note;

    // Apply swing first.
    double swung_start = apply_swing(note.start_ppqn, settings);

    // Snap start position.
    if (settings.snap_start) {
        double snapped = snap_to_grid(static_cast<uint64_t>(std::round(swung_start)),
                                       settings.grid_ppqn);
        // Blend with strength.
        result.start_ppqn = static_cast<uint64_t>(std::round(
            static_cast<double>(note.start_ppqn) * (1.0 - settings.strength) +
            snapped * settings.strength));
    }

    // Snap end position.
    if (settings.snap_end) {
        uint64_t end_ppqn = note.start_ppqn + note.duration_ppqn;
        double swung_end = apply_swing(end_ppqn, settings);
        uint64_t snapped_end = snap_to_grid(
            static_cast<uint64_t>(std::round(swung_end)), settings.grid_ppqn);
        uint64_t blended_end = static_cast<uint64_t>(std::round(
            static_cast<double>(end_ppqn) * (1.0 - settings.strength) +
            static_cast<double>(snapped_end) * settings.strength));

        if (settings.preserve_duration && settings.snap_start) {
            // Duration = snapped end - snapped start, but preserve if requested.
            result.duration_ppqn = blended_end - result.start_ppqn;
        } else {
            result.duration_ppqn = blended_end - note.start_ppqn;
            // Clamp to minimum positive duration.
            if (result.duration_ppqn > blended_end - note.start_ppqn) {
                result.duration_ppqn = blended_end - note.start_ppqn;
            }
        }
    } else if (settings.preserve_duration && settings.snap_start) {
        // When not snapping end, adjust start but keep same end position.
        uint64_t old_end = note.start_ppqn + note.duration_ppqn;
        if (result.start_ppqn < old_end) {
            result.duration_ppqn = old_end - result.start_ppqn;
        }
    }

    // Ensure minimum duration.
    if (result.duration_ppqn < 1) result.duration_ppqn = 1;

    return result;
}

void QuantizeEngine::quantize(std::vector<Note*>& notes,
                               const QuantizeSettings& settings)
{
    for (Note* n : notes) {
        if (n) {
            *n = quantize_note(*n, settings);
        }
    }
}

std::vector<Note> QuantizeEngine::preview(const std::vector<Note*>& notes,
                                           const QuantizeSettings& settings)
{
    std::vector<Note> result;
    result.reserve(notes.size());
    for (const Note* n : notes) {
        if (n) {
            result.push_back(quantize_note(*n, settings));
        }
    }
    return result;
}

} // namespace aria
