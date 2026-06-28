#include "nested_modulation.h"

#include <algorithm>
#include <unordered_set>

namespace aria::automation {

// ─── Cycle Detection ──────────────────────────────────────

bool NestedModulation::has_cycle(const std::vector<Link>& chain) const {
    // Build a set of all source pointers in the chain
    std::unordered_set<const ModulationSource*> seen;
    for (const auto& link : chain) {
        if (!link.source) continue;
        if (seen.count(link.source) > 0) {
            return true;  // Same source appears twice → cycle
        }
        seen.insert(link.source);
    }
    return false;
}

// ─── Chain Setup ──────────────────────────────────────────

bool NestedModulation::set_chain(ParameterID target,
                                  std::vector<Link> chain)
{
    // Enforce max depth
    if (chain.size() > kMaxDepth) return false;

    // Reject empty chains
    if (chain.empty()) {
        clear_chain();
        return true;
    }

    // Detect cycles
    if (has_cycle(chain)) return false;

    target_ = target;
    chain_ = std::move(chain);
    return true;
}

// ─── Evaluation ───────────────────────────────────────────

double NestedModulation::evaluate(uint64_t ppqn,
                                   const ModulationContext& ctx) const
{
    if (chain_.empty()) return 0.0;

    // Evaluate the chain recursively:
    //   output_0 = chain[0].source->evaluate(ppqn, ctx)
    //   output_1 = output_0 × chain[1].amount → feed into chain[1]
    //   ...
    //   final = chain[N-1] modulated by chain[N-2]'s output
    //
    // For nested modulation, each link's source output is scaled
    // by the link's amount and passed as modulation to the next link.

    double current_value = 0.0;

    for (size_t i = 0; i < chain_.size(); ++i) {
        const auto& link = chain_[i];
        if (!link.source) continue;

        // Evaluate the source at this link
        double source_val = link.source->evaluate(ppqn, ctx);

        if (i == 0) {
            // First link: source value is the starting point
            current_value = source_val * static_cast<double>(link.amount);
        } else {
            // Subsequent links: previous result is "base", source modulates it
            // Apply bipolar/unipolar modulation using the standard formula
            if (link.polarity == ModulationPolarity::Bipolar) {
                current_value = current_value
                    + source_val * static_cast<double>(link.amount);
            } else {
                // Unipolar: source (0..1) scaled by amount adds to current
                current_value = current_value
                    + source_val * static_cast<double>(link.amount);
            }
        }
    }

    return std::clamp(current_value, 0.0, 1.0);
}

void NestedModulation::reset() {
    for (auto& link : chain_) {
        if (link.source) {
            link.source->reset();
        }
    }
}

} // namespace aria::automation
