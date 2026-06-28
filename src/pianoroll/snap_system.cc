#include "snap_system.h"
#include "piano_roll_canvas.h"  // For ViewTransform full definition

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Snap implementation
// ═══════════════════════════════════════════════════════════════

uint64_t SnapSystem::snap(uint64_t ppqn) const {
    if (!enabled_ || mode_ == None) return ppqn;

    switch (mode_) {
    case None:
        return ppqn;
    case Grid:
        return snap_to_grid(ppqn, resolution_);
    case Relative:
        // In relative mode we still snap to grid but allow notes
        // to preserve their relative positions.
        return snap_to_grid(ppqn, resolution_);
    case Magnetic:
        // Magnetic: use a smaller virtual resolution for attraction.
        return snap_to_grid(ppqn, std::min(resolution_, 120u));
    case Adaptive:
        // Adaptive uses a fixed grid snap — the resolution is
        // set externally based on zoom level.
        return snap_to_grid(ppqn, resolution_);
    default:
        return ppqn;
    }
}

uint64_t SnapSystem::snap(uint64_t ppqn, double strength) const {
    if (!enabled_ || mode_ == None || strength <= 0.0) return ppqn;
    if (strength >= 1.0) return snap(ppqn);

    uint64_t snapped = snap_to_grid(ppqn, resolution_);
    // Blend between original and snapped position.
    double blended = static_cast<double>(ppqn) * (1.0 - strength) +
                     static_cast<double>(snapped) * strength;
    return static_cast<uint64_t>(std::round(blended));
}

uint64_t SnapSystem::snap_to_grid(uint64_t ppqn, uint32_t res) const {
    if (res == 0) return ppqn;
    uint64_t half = res / 2;
    return ((ppqn + half) / res) * res;
}

// ═══════════════════════════════════════════════════════════════
// Resolution cycling
// ═══════════════════════════════════════════════════════════════

void SnapSystem::cycle_resolution() {
    int idx = resolution_index(resolution_);
    idx = (idx + 1) % 8;
    resolution_ = LEVELS[idx];
}

int SnapSystem::resolution_index(uint32_t res) {
    for (int i = 0; i < 8; ++i) {
        if (LEVELS[i] == res) return i;
    }
    // Default to sixteenth (index 3).
    return 3;
}

// ═══════════════════════════════════════════════════════════════
// Adaptive resolution
// ═══════════════════════════════════════════════════════════════

uint32_t SnapSystem::adaptive_resolution(double ppqn_per_pixel_x) const {
    // At very zoomed-out levels, use coarser grids.
    // At zoomed-in levels, use finer grids.
    if (ppqn_per_pixel_x >= 32.0) return LEVELS[0];  // Bar
    if (ppqn_per_pixel_x >= 16.0) return LEVELS[1];  // Beat
    if (ppqn_per_pixel_x >= 8.0)  return LEVELS[2];  // Eighth
    if (ppqn_per_pixel_x >= 4.0)  return LEVELS[3];  // Sixteenth
    if (ppqn_per_pixel_x >= 2.0)  return LEVELS[4];  // 32nd
    if (ppqn_per_pixel_x >= 1.0)  return LEVELS[5];  // 64th
    if (ppqn_per_pixel_x >= 0.5)  return LEVELS[6];  // 128th
    return LEVELS[7];                                 // 256th
}

// ═══════════════════════════════════════════════════════════════
// Grid lines
// ═══════════════════════════════════════════════════════════════

std::vector<GridLine> SnapSystem::get_lines(const ViewTransform& view,
                                            uint64_t duration_ppqn) const
{
    std::vector<GridLine> lines;

    // Determine visible PPQN range.
    // Use a generous canvas size estimate for grid coverage.
    double canvas_w = 1920.0;
    Rect vp = view.viewport_ppqn(canvas_w, duration_ppqn
        ? static_cast<double>(duration_ppqn)
        : canvas_w * view.ppqn_per_pixel_x + view.scroll_ppqn);

    uint64_t start = static_cast<uint64_t>(std::max(0.0, vp.left()));
    uint64_t end   = static_cast<uint64_t>(vp.right()) + 1;

    // Choose resolutions based on viewport or adaptive mode.
    uint32_t bar_res  = LEVELS[0];  // 3840 (whole note / bar)
    uint32_t beat_res = LEVELS[1];  // 960  (quarter note)
    uint32_t sub_res  = resolution_;

    if (mode_ == Adaptive) {
        sub_res = adaptive_resolution(view.ppqn_per_pixel_x);
        // At coarse zoom, don't draw beats separately.
    }

    // We generate lines at the finest resolution, tagged by type.
    // Type: 0 = bar, 1 = beat, 2 = subdivision
    uint64_t bar_start  = (start / bar_res) * bar_res;
    uint64_t beat_start = (start / beat_res) * beat_res;
    uint64_t sub_start  = (start / sub_res) * sub_res;

    // Determine what grid is visible.
    // At the chosen resolution, tag each grid line.
    for (uint64_t ppqn = sub_start; ppqn <= end; ppqn += sub_res) {
        uint8_t type = 2;  // subdivision
        if (ppqn % bar_res == 0) {
            type = 0;  // bar line
        } else if (ppqn % beat_res == 0) {
            type = 1;  // beat line
        }
        lines.push_back({ppqn, type});
    }

    return lines;
}

} // namespace aria
