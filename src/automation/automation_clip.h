#ifndef ARIA_AUTOMATION_CLIP_H
#define ARIA_AUTOMATION_CLIP_H

#include "automation_types.h"
#include "interpolation.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace aria {
class AutomationEngine;
}

namespace aria::automation {

/// Base class for automation clips.
///
/// Stores sorted automation points and provides point-based curve evaluation,
/// bulk operations, and serialization stubs. Subclasses override `evaluate()`
/// for specialised behaviour (step sequencer, LFO, envelope).
class AutomationClip {
public:
    virtual ~AutomationClip() = default;

    // ─── Metadata ────────────────────────────────────────────
    void set_id(AutomationClipID id) { id_ = id; }
    AutomationClipID id() const { return id_; }

    void set_name(const std::string& name) { name_ = name; }
    const std::string& name() const { return name_; }

    // ─── Duration / Loop ─────────────────────────────────────
    void set_length(uint64_t ppqn) { length_ppqn_ = ppqn; }
    uint64_t length() const { return length_ppqn_; }

    void set_loop(bool enabled) { loop_enabled_ = enabled; }
    bool loop_enabled() const { return loop_enabled_; }

    void set_loop_range(uint64_t start_ppqn, uint64_t end_ppqn) {
        loop_start_ppqn_ = start_ppqn;
        loop_end_ppqn_ = end_ppqn;
    }

    // ─── Point Management ────────────────────────────────────
    void add_point(const AutomationPoint& point);
    void remove_point(uint64_t ppqn);
    void move_point(uint64_t from_ppqn, uint64_t to_ppqn, double value);
    void clear_points() { points_.clear(); }
    size_t point_count() const { return points_.size(); }

    const std::vector<AutomationPoint>& points() const { return points_; }

    AutomationPoint* find_point(uint64_t ppqn);
    const AutomationPoint* find_point(uint64_t ppqn) const;

    std::vector<AutomationPoint> points_in_range(uint64_t start_ppqn,
                                                  uint64_t end_ppqn) const;

    // ─── Evaluation ──────────────────────────────────────────
    /// Evaluate the automation curve at the given PPQN position.
    /// Respects looping when enabled. Returns the interpolated value.
    virtual double evaluate(uint64_t ppqn) const;

    // ─── Bulk Operations ─────────────────────────────────────
    void quantize_points(uint32_t grid_ppqn);
    void offset_values(double amount);
    void scale_values(double factor);

    // ─── Serialization (stubs) ───────────────────────────────
    virtual std::vector<uint8_t> serialize() const;
    static AutomationClip deserialize(const std::vector<uint8_t>& data);

public:
    AutomationClip() = default;

protected:

    AutomationClipID id_ = 0;
    std::string name_;
    uint64_t length_ppqn_ = 960 * 16;   // 4 bars @ 960 PPQN
    bool loop_enabled_ = false;
    uint64_t loop_start_ppqn_ = 0;
    uint64_t loop_end_ppqn_ = 960 * 16;

    std::vector<AutomationPoint> points_;   ///< Always sorted by ppqn
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_CLIP_H
