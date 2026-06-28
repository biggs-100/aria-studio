#include "automation_engine.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace aria {

// ─── Lifecycle ─────────────────────────────────────────────

AutomationEngine::AutomationEngine() = default;

AutomationEngine::~AutomationEngine() { shutdown(); }

bool AutomationEngine::init() {
    if (initialized_) return true;
    initialized_ = true;
    return true;
}

void AutomationEngine::shutdown() {
    if (!initialized_) return;

    clips_.clear();
    sources_.clear();
    matrix_.clear_all();
    lanes_.clear();
    recorder_.stop_recording();
    cache_.clear();

    initialized_ = false;
}

// ─── Clip Management ───────────────────────────────────────

automation::AutomationClip* AutomationEngine::create_clip(
    automation::AutomationClipID id)
{
    if (id == 0) return nullptr;

    // If clip already exists with this ID, return it
    auto it = clips_.find(id);
    if (it != clips_.end()) {
        return it->second.get();
    }

    auto clip = std::make_unique<automation::AutomationClip>();
    clip->set_id(id);
    auto* raw = clip.get();
    clips_.emplace(id, std::move(clip));
    return raw;
}

void AutomationEngine::destroy_clip(automation::AutomationClipID id) {
    clips_.erase(id);
}

automation::AutomationClip* AutomationEngine::get_clip(
    automation::AutomationClipID id) const
{
    auto it = clips_.find(id);
    return (it != clips_.end()) ? it->second.get() : nullptr;
}

// ─── Modulation Sources ────────────────────────────────────

void AutomationEngine::register_source(
    std::unique_ptr<automation::ModulationSource> source)
{
    if (!source) return;
    automation::SourceID id = source->id();
    if (id == 0) return;
    sources_[id] = std::move(source);
}

automation::ModulationSource* AutomationEngine::get_source(
    automation::SourceID id) const
{
    auto it = sources_.find(id);
    return (it != sources_.end()) ? it->second.get() : nullptr;
}

// ─── Modulation Matrix ─────────────────────────────────────

automation::ModulationMatrix& AutomationEngine::matrix() {
    return matrix_;
}

const automation::ModulationMatrix& AutomationEngine::matrix() const {
    return matrix_;
}

// ─── Lane Management ───────────────────────────────────────

automation::AutomationLane* AutomationEngine::create_lane(
    automation::ParameterID target)
{
    if (target == 0) return nullptr;

    // If lane already exists for this target, return it
    auto it = lanes_.find(target);
    if (it != lanes_.end()) {
        return it->second.get();
    }

    auto lane = std::make_unique<automation::AutomationLane>(target);
    auto* raw = lane.get();
    lanes_.emplace(target, std::move(lane));
    return raw;
}

automation::AutomationLane* AutomationEngine::get_lane(
    automation::ParameterID target) const
{
    auto it = lanes_.find(target);
    return (it != lanes_.end()) ? it->second.get() : nullptr;
}

// ─── Recording ─────────────────────────────────────────────

automation::AutomationRecorder& AutomationEngine::recorder() {
    return recorder_;
}

// ─── Parameter Cache ───────────────────────────────────────

automation::ParameterCache& AutomationEngine::cache() {
    return cache_;
}

const automation::ParameterCache& AutomationEngine::cache() const {
    return cache_;
}

// ─── Audio Thread Processing Pipeline ──────────────────────

void AutomationEngine::process_audio_thread(uint64_t start_ppqn,
                                             uint64_t end_ppqn)
{
    if (!initialized_) return;

    // Build evaluation context
    automation::ModulationContext ctx{};
    ctx.engine = this;

    // Use the midpoint of the block for sample-accurate evaluation
    uint64_t mid_ppqn = start_ppqn + (end_ppqn - start_ppqn) / 2;

    // Phase 1: Evaluate all lane clips and write base values to cache
    for (const auto& [param_id, lane] : lanes_) {
        if (lane->is_bypassed()) continue;

        double base_value = lane->evaluate(mid_ppqn);
        cache_.update_value(param_id, base_value);

        // Feed armed lanes into the recorder
        if (lane->is_armed()) {
            recorder_.record_value(mid_ppqn, base_value);
        }
    }

    // Phase 2: Apply all active modulation connections
    for (const auto& [param_id, lane] : lanes_) {
        if (lane->is_bypassed()) continue;

        double result = cache_.read_value(param_id);

        auto connections = matrix_.connections_for(param_id);
        if (connections.empty()) continue;

        for (const auto& entry : connections) {
            if (entry.bypassed) continue;

            // Look up the source by ID
            auto src_it = sources_.find(entry.source_id);
            if (src_it == sources_.end()) continue;

            // Evaluate the modulation source
            double mod_value = src_it->second->evaluate(mid_ppqn, ctx);

            // Apply modulation amount according to polarity
            double amount = static_cast<double>(entry.amount);
            if (entry.polarity == automation::ModulationPolarity::Bipolar) {
                // Bipolar: mod_value is [-1, 1], applied directly
                result += mod_value * amount;
            } else {
                // Unipolar: mod_value is [0, 1], centre around 0.5
                result += (mod_value - 0.5) * amount * 2.0;
            }
        }

        // Clamp to valid range and write back
        result = std::clamp(result, 0.0, 1.0);
        cache_.update_value(param_id, result);
    }

    // Phase 3: Swap buffers so the audio thread sees a consistent snapshot
    cache_.swap_buffers();
}

} // namespace aria
