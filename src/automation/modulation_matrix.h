#ifndef ARIA_AUTOMATION_MODULATION_MATRIX_H
#define ARIA_AUTOMATION_MODULATION_MATRIX_H

#include "modulation_types.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace aria::automation {

class ModulationSource;

/// Directed modulation graph connecting sources to target parameters.
///
/// The matrix maintains a sparse DAG of source → target connections.
/// Each connection stores an amount, polarity, and bypass state.
/// Cycle detection is performed at connect time to prevent routing loops.
///
/// Connections are stored as `ModulationEntry` records indexed by target
/// parameter for O(1) lookup during evaluation.
class ModulationMatrix {
public:
    // ─── Connection Management ───────────────────────────────
    /// Connect a source to a target parameter.
    ///
    /// @param src      Source identifier
    /// @param target   Target parameter ID
    /// @param amount   Modulation amount (0.0–1.0)
    /// @param polarity Unipolar or Bipolar
    /// @return `true` if the connection was made, `false` if it would
    ///         create a cycle or the source/target are invalid.
    bool connect(SourceID src, ParameterID target,
                 float amount = 1.0f,
                 ModulationPolarity polarity = ModulationPolarity::Unipolar);

    /// Remove a specific source-to-target connection.
    bool disconnect(SourceID src, ParameterID target);

    /// Remove all connections for a given target.
    void clear_target(ParameterID target);

    /// Remove all connections from a given source.
    void clear_source(SourceID src);

    /// Remove all connections.
    void clear_all();

    // ─── Bypass ──────────────────────────────────────────────
    void set_bypass(SourceID src, ParameterID target, bool bypassed);
    bool is_bypassed(SourceID src, ParameterID target) const;

    // ─── Queries ─────────────────────────────────────────────
    /// Get all connections targeting a parameter.
    std::vector<ModulationEntry> connections_for(ParameterID target) const;

    /// Get the number of connections for a specific target.
    size_t connection_count(ParameterID target) const;

    /// Total number of connections across all targets.
    size_t total_connections() const;

    /// Check whether a connection exists between source and target.
    bool has_connection(SourceID src, ParameterID target) const;

    /// Check whether connecting `src` → `target` would create a cycle.
    /// Uses DFS traversal of the graph.
    bool has_cycle(SourceID src, ParameterID target) const;

    // ─── Evaluation ──────────────────────────────────────────
    /// Evaluate all active modulations for a target parameter
    /// applied to a base value. Returns the modulated result.
    ///
    /// @param target     Target parameter to evaluate
    /// @param base_value Unmodulated parameter value (0.0–1.0)
    /// @param ppqn       Current PPQN position
    /// @param ctx        Modulation context (note data, sidechain, etc.)
    /// @return Modulated value clamped to [0.0, 1.0]
    double evaluate(ParameterID target, double base_value,
                    uint64_t ppqn,
                    const ModulationContext& ctx) const;

private:
    // Graph adjacency: target → list of sources targeting it
    // Used for fast lookup: given a target, find all modulations
    using ConnectionList = std::vector<ModulationEntry>;
    std::unordered_map<ParameterID, ConnectionList> connections_;

    // Reverse index: source → list of targets it modulates
    // Used for cycle detection: given a source, find what it targets
    using SourceTargetList = std::vector<ParameterID>;
    std::unordered_map<SourceID, SourceTargetList> source_targets_;

    // DFS helper for cycle detection.
    bool dfs_has_cycle(SourceID current, ParameterID target,
                       std::vector<SourceID>& visited) const;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_MODULATION_MATRIX_H
