#include "automation_clip.h"

#include <algorithm>
#include <iterator>
#include <limits>

namespace aria::automation {

// ─── Point Management ─────────────────────────────────────

void AutomationClip::add_point(const AutomationPoint& point) {
    // Insert maintaining sorted order by ppqn
    auto it = std::lower_bound(points_.begin(), points_.end(), point.ppqn,
                               [](const AutomationPoint& p, uint64_t ppqn) {
                                   return p.ppqn < ppqn;
                               });
    // Replace if exact match
    if (it != points_.end() && it->ppqn == point.ppqn) {
        *it = point;
    } else {
        points_.insert(it, point);
    }
}

void AutomationClip::remove_point(uint64_t ppqn) {
    auto it = std::lower_bound(points_.begin(), points_.end(), ppqn,
                               [](const AutomationPoint& p, uint64_t q) {
                                   return p.ppqn < q;
                               });
    if (it != points_.end() && it->ppqn == ppqn) {
        points_.erase(it);
    }
}

void AutomationClip::move_point(uint64_t from_ppqn, uint64_t to_ppqn, double value) {
    // Find and remove the point at from_ppqn
    auto it = std::lower_bound(points_.begin(), points_.end(), from_ppqn,
                               [](const AutomationPoint& p, uint64_t q) {
                                   return p.ppqn < q;
                               });
    if (it == points_.end() || it->ppqn != from_ppqn) return;

    AutomationPoint pt = *it;
    points_.erase(it);

    // Update and re-insert at new position
    pt.ppqn = to_ppqn;
    pt.value = value;
    add_point(pt);
}

AutomationPoint* AutomationClip::find_point(uint64_t ppqn) {
    auto it = std::lower_bound(points_.begin(), points_.end(), ppqn,
                               [](const AutomationPoint& p, uint64_t q) {
                                   return p.ppqn < q;
                               });
    if (it != points_.end() && it->ppqn == ppqn) {
        return &(*it);
    }
    return nullptr;
}

const AutomationPoint* AutomationClip::find_point(uint64_t ppqn) const {
    auto it = std::lower_bound(points_.begin(), points_.end(), ppqn,
                               [](const AutomationPoint& p, uint64_t q) {
                                   return p.ppqn < q;
                               });
    if (it != points_.end() && it->ppqn == ppqn) {
        return &(*it);
    }
    return nullptr;
}

std::vector<AutomationPoint>
AutomationClip::points_in_range(uint64_t start_ppqn, uint64_t end_ppqn) const {
    auto first = std::lower_bound(points_.begin(), points_.end(), start_ppqn,
                                  [](const AutomationPoint& p, uint64_t q) {
                                      return p.ppqn < q;
                                  });
    auto last  = std::upper_bound(points_.begin(), points_.end(), end_ppqn,
                                  [](uint64_t q, const AutomationPoint& p) {
                                      return q < p.ppqn;
                                  });
    return {first, last};
}

// ─── Evaluation ───────────────────────────────────────────

double AutomationClip::evaluate(uint64_t ppqn) const {
    if (points_.empty()) return 0.0;

    // Handle looping
    if (loop_enabled_ && length_ppqn_ > 0) {
        if (ppqn < loop_start_ppqn_ || ppqn > loop_end_ppqn_) {
            // Outside loop range — evaluate the nearest edge
            if (ppqn < loop_start_ppqn_)
                ppqn = loop_start_ppqn_;
            else
                ppqn = loop_end_ppqn_;
        }
        // Wrap into loop range
        if (length_ppqn_ > 0 && ppqn >= loop_start_ppqn_) {
            ppqn = loop_start_ppqn_ + ((ppqn - loop_start_ppqn_) % length_ppqn_);
        }
    }

    // Boundary: before first point
    if (ppqn <= points_.front().ppqn) {
        return points_.front().value;
    }

    // Boundary: after last point
    if (ppqn >= points_.back().ppqn) {
        return points_.back().value;
    }

    // Binary search for the segment containing ppqn
    auto it = std::upper_bound(points_.begin(), points_.end(), ppqn,
                               [](uint64_t q, const AutomationPoint& p) {
                                   return q < p.ppqn;
                               });
    // it now points to the first point with ppqn > query ppqn
    // The segment is [it-1, it]
    const auto& left = *(it - 1);
    const auto& right = *it;

    const uint64_t segment_length = right.ppqn - left.ppqn;
    if (segment_length == 0) return left.value;

    const double t = static_cast<double>(ppqn - left.ppqn)
                   / static_cast<double>(segment_length);

    return evaluate_segment(t, left.value, right.value,
                            left.interpolation, left.bezier);
}

// ─── Bulk Operations ──────────────────────────────────────

void AutomationClip::quantize_points(uint32_t grid_ppqn) {
    if (grid_ppqn == 0) return;
    for (auto& pt : points_) {
        // Snap to nearest grid line
        pt.ppqn = ((pt.ppqn + grid_ppqn / 2) / grid_ppqn) * grid_ppqn;
    }
    // Re-sort after quantization (some points may have moved to same positions)
    std::sort(points_.begin(), points_.end(),
              [](const AutomationPoint& a, const AutomationPoint& b) {
                  return a.ppqn < b.ppqn;
              });
    // Remove duplicates (keep the last value at each PPQN)
    auto last = std::unique(points_.rbegin(), points_.rend(),
                            [](const AutomationPoint& a, const AutomationPoint& b) {
                                return a.ppqn == b.ppqn;
                            }).base();
    points_.erase(points_.begin(), last);
}

void AutomationClip::offset_values(double amount) {
    for (auto& pt : points_) {
        pt.value = std::clamp(pt.value + amount, 0.0, 1.0);
    }
}

void AutomationClip::scale_values(double factor) {
    for (auto& pt : points_) {
        pt.value = std::clamp(pt.value * factor, 0.0, 1.0);
    }
}

// ─── Serialization (stubs) ────────────────────────────────

std::vector<uint8_t> AutomationClip::serialize() const {
    // Stub — returns empty placeholder; full serialisation wired in Phase 4.
    return {};
}

AutomationClip AutomationClip::deserialize(const std::vector<uint8_t>& /*data*/) {
    // Stub — returns default clip; full deserialisation wired in Phase 4.
    return AutomationClip{};
}

} // namespace aria::automation
