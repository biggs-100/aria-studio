#ifndef ARIA_MODEL_AUTOMATION_CLIP_WRAPPER_H
#define ARIA_MODEL_AUTOMATION_CLIP_WRAPPER_H

#include "model/clip.h"
#include "automation/automation_clip.h"

#include <cstdint>
#include <string>

namespace aria {

/// Thin wrapper that adapts an existing AutomationClip for use
/// as a track clip. Holds a reference to an AutomationClip and
/// implements the Clip interface by delegating length/name to
/// the inner clip while owning position/fade/gain/mute locally.
class AutomationClipWrapper : public Clip {
public:
    /// Construct a wrapper around the given AutomationClip.
    /// The referenced clip must outlive this wrapper.
    explicit AutomationClipWrapper(automation::AutomationClip& clip);

    // ─── Clip interface overrides ─────────────────────────────
    uint64_t length() const override { return clip_.length(); }
    void set_length(uint64_t ppqn) override { clip_.set_length(ppqn); }

    std::string name() const override { return clip_.name(); }
    void set_name(const std::string& n) override { clip_.set_name(n); }

    // ─── Automation-specific ─────────────────────────────────
    /// Evaluate the automation curve at the given PPQN position,
    /// delegating to the inner AutomationClip.
    double evaluate(uint64_t ppqn) const { return clip_.evaluate(ppqn); }

private:
    automation::AutomationClip& clip_;
};

} // namespace aria

#endif // ARIA_MODEL_AUTOMATION_CLIP_WRAPPER_H
