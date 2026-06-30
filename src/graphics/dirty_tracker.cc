#include "graphics/dirty_tracker.h"

#include <algorithm>
#include <cmath>

namespace aria {

void DirtyTracker::mark_dirty(WidgetID id, const Rect& bounds) {
    entries_.push_back({id, bounds});
    invalidate_merged();
}

void DirtyTracker::mark_all(const Rect& viewport) {
    entries_.clear();
    entries_.push_back({kInvalidWidgetID, viewport});
    invalidate_merged();
}

bool DirtyTracker::has_dirty() const noexcept {
    return !entries_.empty();
}

const std::vector<Rect>& DirtyTracker::dirty_rects() const noexcept {
    if (!merged_valid_) {
        const_cast<DirtyTracker*>(this)->rebuild_merged();
    }
    return merged_;
}

void DirtyTracker::clear() {
    entries_.clear();
    merged_.clear();
    merged_valid_ = true;
}

Rect DirtyTracker::merged_bounds() const {
    if (entries_.empty()) return {};

    float min_x = entries_[0].bounds.x;
    float min_y = entries_[0].bounds.y;
    float max_x = entries_[0].bounds.right();
    float max_y = entries_[0].bounds.bottom();

    for (const auto& entry : entries_) {
        min_x = std::min(min_x, entry.bounds.x);
        min_y = std::min(min_y, entry.bounds.y);
        max_x = std::max(max_x, entry.bounds.right());
        max_y = std::max(max_y, entry.bounds.bottom());
    }

    return {min_x, min_y, max_x - min_x, max_y - min_y};
}

void DirtyTracker::invalidate_merged() {
    merged_valid_ = false;
}

void DirtyTracker::rebuild_merged() {
    merged_.clear();

    if (entries_.empty()) {
        merged_valid_ = true;
        return;
    }

    // Simple merging: combine all overlapping rects
    std::vector<Rect> rects;
    for (const auto& entry : entries_) {
        rects.push_back(entry.bounds);
    }

    // Merge overlapping rects
    bool merged_any = true;
    while (merged_any) {
        merged_any = false;
        std::vector<Rect> next;

        for (size_t i = 0; i < rects.size(); ++i) {
            bool merged = false;
            for (size_t j = i + 1; j < rects.size(); ++j) {
                if (rects[i].intersects(rects[j])) {
                    // Merge j into i
                    float nx = std::min(rects[i].x, rects[j].x);
                    float ny = std::min(rects[i].y, rects[j].y);
                    float nw = std::max(rects[i].right(), rects[j].right()) - nx;
                    float nh = std::max(rects[i].bottom(), rects[j].bottom()) - ny;
                    rects[i] = {nx, ny, nw, nh};
                    rects.erase(rects.begin() + static_cast<ptrdiff_t>(j));
                    --j;
                    merged = true;
                    merged_any = true;
                }
            }
            if (!merged) {
                next.push_back(rects[i]);
            }
        }
        rects = std::move(next);
    }

    merged_ = std::move(rects);
    merged_valid_ = true;
}

} // namespace aria
