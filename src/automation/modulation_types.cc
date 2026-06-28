#include "modulation_types.h"
#include "modulation_source.h"

#include <algorithm>

namespace aria::automation {

// ─── ModulationStack ──────────────────────────────────────

void ModulationStack::add_entry(ModulationSource* source,
                                float amount,
                                ModulationPolarity polarity)
{
    entries_.push_back({source, amount, polarity, false});
}

void ModulationStack::remove_entry(size_t index) {
    if (index < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(index));
    }
}

void ModulationStack::clear() {
    entries_.clear();
}

void ModulationStack::set_bypass(size_t index, bool bypassed) {
    if (index < entries_.size()) {
        entries_[index].bypassed = bypassed;
    }
}

void ModulationStack::set_amount(size_t index, float amount) {
    if (index < entries_.size()) {
        entries_[index].amount = amount;
    }
}

double ModulationStack::evaluate(double base_value,
                                 uint64_t ppqn,
                                 const ModulationContext& ctx) const
{
    if (entries_.empty()) return base_value;

    double result = base_value;

    for (const auto& entry : entries_) {
        if (entry.bypassed || !entry.source) continue;

        double mod_val = entry.source->evaluate(ppqn, ctx);

        // Apply modulation: source value × amount added to result
        // For unipolar sources (0..1), modulation always pushes upward.
        // For bipolar sources (-1..1), modulation pushes both directions.
        result = result + mod_val * static_cast<double>(entry.amount);
    }

    return std::clamp(result, 0.0, 1.0);
}

} // namespace aria::automation
