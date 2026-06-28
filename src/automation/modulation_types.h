#ifndef ARIA_AUTOMATION_MODULATION_TYPES_H
#define ARIA_AUTOMATION_MODULATION_TYPES_H

#include "automation_types.h"

#include <cstdint>
#include <vector>

namespace aria {
class AutomationEngine;
}

namespace aria::automation {

// ─── Identifiers ──────────────────────────────────────────

/// Unique modulation source identifier. 0 = invalid.
using SourceID = uint64_t;

// ─── ModulationPolarity ───────────────────────────────────

/// Determines how a modulation source's output is interpreted.
///
/// Unipolar — source output is 0.0–1.0 (additive only upward).
/// Bipolar  — source output is -1.0–1.0 (pushes both directions).
enum class ModulationPolarity {
    Unipolar,
    Bipolar
};

// ─── ModulationEntry ──────────────────────────────────────

/// A single connection in the modulation matrix.
///
/// Records which source drives which target parameter,
/// the modulation amount, polarity, and bypass state.
struct ModulationEntry {
    SourceID source_id = 0;                              ///< Source identifier
    ParameterID target_param = 0;                        ///< Target parameter
    float amount = 1.0f;                                 ///< Modulation depth 0..1
    ModulationPolarity polarity = ModulationPolarity::Unipolar;
    bool bypassed = false;
};

// ─── ModulationContext ────────────────────────────────────

/// Runtime context passed to every `evaluate()` call.
///
/// Provides note-level data, sidechain amplitude, and a handle
/// to the engine for nested modulation lookups.
struct ModulationContext {
    double current_note_velocity = 0.0;  ///< 0.0–1.0
    double current_note_pitch = 0.0;     ///< Normalised pitch (0.0–1.0)
    double sidechain_amplitude = 0.0;    ///< Current sidechain level
    const class aria::AutomationEngine* engine = nullptr; ///< For nested modulation
};

// ─── ModulationStack ──────────────────────────────────────

/// Ordered list of modulation entries applied to a single parameter.
///
/// Modulations are applied in insertion order — each subsequent
/// entry modulates the accumulated result of the previous ones.
///
/// The stack is evaluated as:
///   result = base
///   for each active entry:
///     result = result + source_eval × entry.amount
///   result = clamp(result, 0.0, 1.0)
class ModulationSource;  // forward declaration

class ModulationStack {
public:
    /// Runtime entry with a live pointer to the source.
    struct Entry {
        ModulationSource* source = nullptr;
        float amount = 1.0f;
        ModulationPolarity polarity = ModulationPolarity::Unipolar;
        bool bypassed = false;
    };

    // ─── Management ──────────────────────────────────────────
    void add_entry(ModulationSource* source,
                   float amount = 1.0f,
                   ModulationPolarity polarity = ModulationPolarity::Unipolar);
    void remove_entry(size_t index);
    void clear();
    void set_bypass(size_t index, bool bypassed);
    void set_amount(size_t index, float amount);

    // ─── Queries ─────────────────────────────────────────────
    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }
    const Entry& entry(size_t index) const { return entries_[index]; }
    const std::vector<Entry>& entries() const { return entries_; }

    // ─── Evaluation ──────────────────────────────────────────
    /// Apply all active stack entries to `base_value`.
    ///
    /// @param base_value  The unmodulated parameter value (0.0–1.0)
    /// @param ppqn        Current PPQN position
    /// @param ctx         Modulation context (note data, sidechain, etc.)
    /// @return Modulated value clamped to [0.0, 1.0]
    double evaluate(double base_value, uint64_t ppqn,
                    const ModulationContext& ctx) const;

private:
    std::vector<Entry> entries_;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_MODULATION_TYPES_H
