#ifndef ARIA_ANIMATOR_H
#define ARIA_ANIMATOR_H

#include "graphics/animation_types.h"
#include "graphics/graphics_types.h"

#include <vector>

namespace aria {

/// Manages the lifecycle of active animations.
///
/// Owned by the application layer; RenderLoop calls `tick(dt)` once
/// per frame. Completed animations collect their WidgetIDs for the
/// caller to mark widgets dirty.
class Animator {
public:
    Animator() = default;
    ~Animator() = default;

    Animator(const Animator&) = delete;
    Animator& operator=(const Animator&) = delete;
    Animator(Animator&&) = delete;
    Animator& operator=(Animator&&) = delete;

    /// Start a new animation. Takes ownership via move.
    void start(Animation anim);

    /// Advance all active animations by `dt` seconds.
    /// Calls `on_complete` for animations reaching t >= 1.0.
    /// Removes completed animations from the active list.
    void tick(float dt);

    /// Remove all active animations for the given widget.
    void cancel(WidgetID id);

    /// Returns true if any animation is active for the given widget.
    bool is_animating(WidgetID id) const noexcept;

    /// Returns the number of active animations.
    size_t active_count() const noexcept;

    /// Returns the set of widgets whose animations completed during
    /// the most recent tick(). Cleared at the start of the next tick().
    const std::vector<WidgetID>& completed() const noexcept;

    /// Returns the current interpolated value for an animated property.
    /// Returns 0.0f if no animation is or was active for the
    /// given widget/property in the most recent tick.
    float current_value(WidgetID id, AnimatableProperty prop) const noexcept;

private:
    struct CompletedEntry {
        WidgetID id;
        AnimatableProperty prop;
        float final_value;
    };

    std::vector<Animation> animations_;
    std::vector<WidgetID> completed_;
    std::vector<CompletedEntry> completed_cache_;
};

} // namespace aria

#endif // ARIA_ANIMATOR_H
