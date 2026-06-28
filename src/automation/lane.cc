#include "lane.h"

#include <algorithm>

namespace aria::automation {

// ─── Construction ──────────────────────────────────────────

AutomationLane::AutomationLane(ParameterID target)
    : target_(target) {}

// ─── Clip Binding ──────────────────────────────────────────

void AutomationLane::bind_clip(std::shared_ptr<AutomationClip> clip) {
    clip_ = std::move(clip);
}

void AutomationLane::unbind_clip() {
    clip_.reset();
}

AutomationClip* AutomationLane::clip() const {
    return clip_.get();
}

// ─── Arm / Bypass ──────────────────────────────────────────

void AutomationLane::set_armed(bool armed) {
    armed_ = armed;
}

bool AutomationLane::is_armed() const {
    return armed_;
}

void AutomationLane::set_bypassed(bool bypassed) {
    bypassed_ = bypassed;
}

bool AutomationLane::is_bypassed() const {
    return bypassed_;
}

// ─── Visibility / Height ───────────────────────────────────

void AutomationLane::set_visible(bool visible) {
    visible_ = visible;
}

bool AutomationLane::is_visible() const {
    return visible_;
}

void AutomationLane::set_height(float height) {
    height_ = std::clamp(height, 0.0f, 1.0f);
}

float AutomationLane::height() const {
    return height_;
}

// ─── Target / Evaluation ───────────────────────────────────

ParameterID AutomationLane::target() const {
    return target_;
}

float AutomationLane::evaluate(uint64_t ppqn) const {
    if (bypassed_ || !clip_) return 0.0f;
    return static_cast<float>(clip_->evaluate(ppqn));
}

} // namespace aria::automation
