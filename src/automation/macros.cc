#include "macros.h"

#include <algorithm>

namespace aria::automation {

void MacroSource::add_mapping(ParameterID target,
                              double min_range,
                              double max_range)
{
    // Replace if mapping already exists for this target
    auto it = std::find_if(mappings_.begin(), mappings_.end(),
                           [target](const MacroMapping& m) {
                               return m.target == target;
                           });
    if (it != mappings_.end()) {
        it->min_range = min_range;
        it->max_range = max_range;
        return;
    }
    mappings_.push_back({target, min_range, max_range});
}

void MacroSource::remove_mapping(ParameterID target) {
    auto it = std::find_if(mappings_.begin(), mappings_.end(),
                           [target](const MacroMapping& m) {
                               return m.target == target;
                           });
    if (it != mappings_.end()) {
        mappings_.erase(it);
    }
}

void MacroSource::clear_mappings() {
    mappings_.clear();
}

double MacroSource::evaluate(uint64_t /*ppqn*/,
                             const ModulationContext& /*ctx*/) const
{
    double raw = value_.load(std::memory_order_relaxed);

    // Evaluate returns the raw value. The matrix applies per-target
    // range mapping through the ModulationEntry.
    return raw;
}

} // namespace aria::automation
