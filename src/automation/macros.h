#ifndef ARIA_AUTOMATION_MACROS_H
#define ARIA_AUTOMATION_MACROS_H

#include "modulation_types.h"
#include "modulation_source.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace aria::automation {

/// A single macro-to-parameter mapping with range remapping.
struct MacroMapping {
    ParameterID target = 0;     ///< Target parameter
    double min_range = 0.0;     ///< Mapped minimum (parameter space 0..1)
    double max_range = 1.0;     ///< Mapped maximum (parameter space 0..1)
};

/// User-assigned macro knob modulation source.
///
/// Macros are named knobs (0.0–1.0) settable via UI or MIDI learn.
/// Each macro can drive multiple parameter targets with independent
/// min/max range mappings. The value is stored atomically for lock-free
/// reads from the UI thread.
class MacroSource : public ModulationSource {
public:
    // ─── Value ───────────────────────────────────────────────
    void set_value(double normalized) {
        value_.store(std::clamp(normalized, 0.0, 1.0), std::memory_order_relaxed);
    }

    double value() const {
        return value_.load(std::memory_order_relaxed);
    }

    /// Set value from a MIDI CC (0–127).
    void set_value_by_midi(uint8_t cc_value) {
        set_value(static_cast<double>(cc_value) / 127.0);
    }

    // ─── Mappings ────────────────────────────────────────────
    void add_mapping(ParameterID target,
                     double min_range = 0.0,
                     double max_range = 1.0);
    void remove_mapping(ParameterID target);
    void clear_mappings();
    size_t mapping_count() const { return mappings_.size(); }

    const std::vector<MacroMapping>& mappings() const { return mappings_; }

    // ─── Overrides ───────────────────────────────────────────
    double evaluate(uint64_t ppqn,
                    const ModulationContext& ctx) const override;

private:
    std::atomic<double> value_{0.5};
    std::vector<MacroMapping> mappings_;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_MACROS_H
