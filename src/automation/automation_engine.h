#ifndef ARIA_AUTOMATION_ENGINE_H
#define ARIA_AUTOMATION_ENGINE_H

#include "automation_clip.h"
#include "automation_types.h"
#include "cache.h"
#include "lane.h"
#include "modulation_matrix.h"
#include "modulation_source.h"
#include "modulation_types.h"
#include "recorder.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace aria {

/// Automation Engine — curves, modulation sources, and parameter automation.
///
/// The engine orchestrates automation clip evaluation, modulation matrix
/// processing, lane management, real-time recording, and the lock-free
/// parameter cache that bridges the control thread to the audio thread.
class AutomationEngine {
public:
    AutomationEngine();
    ~AutomationEngine();

    // ─── Lifecycle ────────────────────────────────────────────
    bool init();
    void shutdown();

    // ─── Clip Management ──────────────────────────────────────
    automation::AutomationClip* create_clip(automation::AutomationClipID id);
    void destroy_clip(automation::AutomationClipID id);
    automation::AutomationClip* get_clip(automation::AutomationClipID id) const;

    // ─── Modulation Sources ───────────────────────────────────
    void register_source(std::unique_ptr<automation::ModulationSource> source);
    automation::ModulationSource* get_source(automation::SourceID id) const;

    // ─── Modulation Matrix ────────────────────────────────────
    automation::ModulationMatrix& matrix();
    const automation::ModulationMatrix& matrix() const;

    // ─── Lane Management ──────────────────────────────────────
    automation::AutomationLane* create_lane(automation::ParameterID target);
    automation::AutomationLane* get_lane(automation::ParameterID target) const;

    // ─── Recording ────────────────────────────────────────────
    automation::AutomationRecorder& recorder();

    // ─── Parameter Cache (for real-time audio thread) ─────────
    automation::ParameterCache& cache();
    const automation::ParameterCache& cache() const;

    // ─── Audio Thread Processing Pipeline ─────────────────────
    /// Evaluate all active lanes and modulation connections for the
    /// given PPQN range, writing the results into the parameter cache.
    /// The cache's swap_buffers() is called so the audio thread sees
    /// a consistent snapshot for the duration of one callback.
    void process_audio_thread(uint64_t start_ppqn, uint64_t end_ppqn);

private:
    std::unordered_map<automation::AutomationClipID,
                       std::unique_ptr<automation::AutomationClip>> clips_;
    std::unordered_map<automation::SourceID,
                       std::unique_ptr<automation::ModulationSource>> sources_;
    automation::ModulationMatrix matrix_;
    std::unordered_map<automation::ParameterID,
                       std::unique_ptr<automation::AutomationLane>> lanes_;
    automation::AutomationRecorder recorder_;
    automation::ParameterCache cache_;
    bool initialized_ = false;
};

} // namespace aria

#endif // ARIA_AUTOMATION_ENGINE_H
