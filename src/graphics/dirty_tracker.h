#ifndef ARIA_DIRTY_TRACKER_H
#define ARIA_DIRTY_TRACKER_H

#include "graphics/graphics_types.h"

#include <vector>

namespace aria {

/// Tracks dirty rectangles per widget and merges overlapping regions.
///
/// After the paint pass, dirty rects are merged into a minimal set
/// of clip regions for efficient re-rendering. Overlapping rects are
/// merged into a single encompassing rect. The tracker is cleared
/// after each frame's paint pass completes.
///
/// Design decisions (from design.md):
///   - Per-widget dirty rect tracking.
///   - Merge overlapping rects into enclosing bounds.
///   - Clear after paint pass.
class DirtyTracker {
public:
    DirtyTracker() = default;
    ~DirtyTracker() = default;

    /// Mark a widget's bounds as dirty.
    void mark_dirty(WidgetID id, const Rect& bounds);

    /// Mark the entire viewport as dirty (full repaint).
    void mark_all(const Rect& viewport);

    /// Return true if the tracker has any dirty rects.
    bool has_dirty() const noexcept;

    /// Return the merged set of dirty rects.
    const std::vector<Rect>& dirty_rects() const noexcept;

    /// Clear all dirty state (call after paint pass).
    void clear();

    /// Return the combined bounding rect of all dirty regions.
    /// Returns an empty Rect if nothing is dirty.
    Rect merged_bounds() const;

private:
    struct DirtyEntry {
        WidgetID id;
        Rect bounds;
    };

    std::vector<DirtyEntry> entries_;
    std::vector<Rect> merged_;
    bool merged_valid_ = false;

    void invalidate_merged();
    void rebuild_merged();
};

} // namespace aria

#endif // ARIA_DIRTY_TRACKER_H
