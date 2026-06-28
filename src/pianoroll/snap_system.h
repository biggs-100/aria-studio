#ifndef ARIA_PIANOROLL_SNAP_SYSTEM_H
#define ARIA_PIANOROLL_SNAP_SYSTEM_H

#include "note.h"

#include <cstdint>
#include <vector>

namespace aria {

// Forward declarations.
struct ViewTransform;

// ─── GridLine ──────────────────────────────────────────────────

/// A single vertical grid line for rendering.
struct GridLine {
    uint64_t ppqn = 0;
    uint8_t  type = 0;  // 0=bar, 1=beat, 2=16th, 3=32nd, etc.
};

// ─── SnapSystem ────────────────────────────────────────────────

/// Controls grid snapping for note placement and editing.
///
/// Supports multiple snap modes, configurable resolution, and
/// adaptive zoom-based level selection.
class SnapSystem {
public:
    enum SnapMode : uint8_t {
        None,       // No snap — free placement.
        Grid,       // Snap to nearest grid line.
        Relative,   // Snap relative to existing notes.
        Magnetic,   // Attract to nearby grid lines (magnetic strength).
        Adaptive    // Auto-adjust grid based on zoom level.
    };

    // ─── Configuration ───────────────────────────────────────

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    void set_mode(SnapMode mode) { mode_ = mode; }
    SnapMode mode() const { return mode_; }

    void set_resolution(uint32_t ppqn) { resolution_ = ppqn; }
    uint32_t resolution() const { return resolution_; }

    // ─── Snapping ─────────────────────────────────────────────

    /// Snap a PPQN position to the nearest grid line.
    uint64_t snap(uint64_t ppqn) const;

    /// Snap with a strength factor (0.0 = no snap, 1.0 = hard snap).
    uint64_t snap(uint64_t ppqn, double strength) const;

    /// Cycle through standard resolutions.
    void cycle_resolution();

    // ─── Grid lines for rendering ─────────────────────────────

    /// Generate grid lines for the visible PPQN range.
    std::vector<GridLine> get_lines(const ViewTransform& view,
                                     uint64_t duration_ppqn) const;

    /// Select the best resolution for the current zoom level.
    uint32_t adaptive_resolution(double ppqn_per_pixel_x) const;

    // ─── Resolution levels (PPQN per grid cell) ───────────────

    static constexpr uint32_t LEVELS[8] = {
        3840,  // 0: Bar (whole note)
        960,   // 1: Beat (quarter note)
        480,   // 2: Eighth note
        240,   // 3: Sixteenth note
        120,   // 4: 32nd note
        60,    // 5: 64th note
        30,    // 6: 128th note
        15     // 7: 256th note (max)
    };

    /// Get the index (0-7) into LEVELS for a given resolution.
    static int resolution_index(uint32_t res);

private:
    bool     enabled_    = true;
    SnapMode mode_       = Grid;
    uint32_t resolution_ = 240;  // Sixteenth notes default

    /// Find the nearest grid position using the current resolution.
    uint64_t snap_to_grid(uint64_t ppqn, uint32_t res) const;
};

} // namespace aria

#endif // ARIA_PIANOROLL_SNAP_SYSTEM_H
