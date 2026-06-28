#ifndef ARIA_PIANOROLL_QUANTIZE_ENGINE_H
#define ARIA_PIANOROLL_QUANTIZE_ENGINE_H

#include "note.h"

#include <cstdint>
#include <vector>

namespace aria {

// ─── QuantizeSettings ───────────────────────────────────────────

/// Settings controlling how quantisation is applied to notes.
struct QuantizeSettings {
    uint32_t grid_ppqn          = 240;   // Grid resolution (default: 16th note)
    double   strength           = 1.0;   // 0.0 (none) to 1.0 (full snap)
    bool     snap_start         = true;  // Snap note start position
    bool     snap_end           = false; // Snap note end position
    double   swing              = 0.0;   // 0.0 (straight) to 1.0 (full swing)
    bool     preserve_duration  = true;  // Keep note length when moving start
};

// ─── QuantizeEngine ─────────────────────────────────────────────

/// Quantizes note timing to a musical grid with configurable strength,
/// swing, and duration preservation.
class QuantizeEngine {
public:
    /// Quantize a vector of note pointers in-place.
    void quantize(std::vector<Note*>& notes, const QuantizeSettings& settings);

    /// Quantize a single note and return the result (does not modify input).
    Note quantize_note(const Note& note, const QuantizeSettings& settings);

    /// Preview quantise — returns result vector without modifying originals.
    std::vector<Note> preview(const std::vector<Note*>& notes,
                              const QuantizeSettings& settings);

private:
    /// Snap a PPQN value to the nearest grid position.
    static uint64_t snap_to_grid(uint64_t ppqn, uint32_t grid_ppqn);

    /// Apply swing offset to a PPQN position.
    static double apply_swing(uint64_t ppqn, const QuantizeSettings& settings);
};

} // namespace aria

#endif // ARIA_PIANOROLL_QUANTIZE_ENGINE_H
