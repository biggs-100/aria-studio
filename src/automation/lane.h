#ifndef ARIA_AUTOMATION_LANE_H
#define ARIA_AUTOMATION_LANE_H

#include "automation_clip.h"
#include "automation_types.h"

#include <cstdint>
#include <memory>

namespace aria::automation {

/// An automation lane binds one parameter target to one clip.
///
/// Lanes support arm/disarm (for recording), bypass, visibility
/// toggling, and configurable height. The `evaluate()` method
/// returns the clip's value at a given PPQN position, or 0.0
/// when the lane is bypassed or has no clip.
class AutomationLane {
public:
    explicit AutomationLane(ParameterID target);

    // ─── Clip Binding ─────────────────────────────────────────

    /// Bind an automation clip to this lane.
    void bind_clip(std::shared_ptr<AutomationClip> clip);
    void unbind_clip();

    /// Get the bound clip (nullptr if none).
    AutomationClip* clip() const;

    // ─── Arm / Bypass ─────────────────────────────────────────

    void set_armed(bool armed);
    bool is_armed() const;

    void set_bypassed(bool bypassed);
    bool is_bypassed() const;

    // ─── Visibility / Height ──────────────────────────────────

    void set_visible(bool visible);
    bool is_visible() const;

    /// Set lane height as a fraction of available space (0..1).
    /// Values outside [0, 1] are clamped.
    void set_height(float height);
    float height() const;

    // ─── Target / Evaluation ──────────────────────────────────

    ParameterID target() const;

    /// Evaluate the lane at a given PPQN position.
    /// Returns 0.0 if bypassed or no clip is bound;
    /// otherwise delegates to the clip's evaluate().
    float evaluate(uint64_t ppqn) const;

private:
    ParameterID target_;
    std::shared_ptr<AutomationClip> clip_;
    bool armed_ = false;
    bool bypassed_ = false;
    bool visible_ = true;
    float height_ = 0.2f;   ///< Normalized 0..1, default 20 %
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_LANE_H
