#ifndef ARIA_AUTOMATION_NESTED_MODULATION_H
#define ARIA_AUTOMATION_NESTED_MODULATION_H

#include "modulation_types.h"
#include "modulation_source.h"

#include <cstdint>
#include <vector>

namespace aria::automation {

/// Nested modulation chain — one source modulates another.
///
/// A modulation chain is a sequence of links:
///   Source A → Source B → ... → Target Parameter
///
/// Each link specifies a source and how it feeds into the next
/// link. The final link's output modulates the target parameter.
///
/// Depth is capped at `kMaxDepth` (8 levels). Circular chains
/// are detected and rejected at `set_chain()` time.
class NestedModulation : public ModulationSource {
public:
    /// Maximum allowed chain depth (8 levels).
    static constexpr size_t kMaxDepth = 8;

    /// A single link in the modulation chain.
    struct Link {
        ModulationSource* source = nullptr;  ///< The source at this link
        float amount = 1.0f;                  ///< Modulation amount for this link
        ModulationPolarity polarity = ModulationPolarity::Unipolar;

        /// Source parameter ID being modulated at this link
        /// (e.g., the rate parameter of an LFO source).
        /// When 0, the source output feeds directly into the next link.
        SourceID target_param = 0;
    };

    NestedModulation() = default;

    // ─── Chain Setup ─────────────────────────────────────────
    /// Set the modulation chain for a target.
    ///
    /// @param target  The final target parameter
    /// @param chain   Ordered list of links (source 0 → source 1 → ... → target)
    /// @return `true` if the chain was set, `false` if it exceeds kMaxDepth
    ///         or would create a circular dependency.
    bool set_chain(ParameterID target, std::vector<Link> chain);

    /// Clear the current chain.
    void clear_chain() { chain_.clear(); target_ = 0; }

    /// Get the current chain depth.
    size_t depth() const { return chain_.size(); }

    /// Get the chain links (read-only).
    const std::vector<Link>& links() const { return chain_; }

    /// Get the target parameter.
    ParameterID target() const { return target_; }

    // ─── Overrides ───────────────────────────────────────────
    double evaluate(uint64_t ppqn,
                    const ModulationContext& ctx) const override;
    void reset() override;

private:
    /// Detect circular dependencies in the proposed chain.
    bool has_cycle(const std::vector<Link>& chain) const;

    ParameterID target_ = 0;
    std::vector<Link> chain_;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_NESTED_MODULATION_H
