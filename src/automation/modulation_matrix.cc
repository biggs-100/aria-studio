#include "modulation_matrix.h"

#include <algorithm>
#include <unordered_set>

namespace aria::automation {

// ─── Connection Management ────────────────────────────────

bool ModulationMatrix::connect(SourceID src, ParameterID target,
                                float amount,
                                ModulationPolarity polarity)
{
    if (src == 0 || target == 0) return false;

    // Cycle check: would connecting src → target create a loop?
    if (has_cycle(src, target)) return false;

    // Check if connection already exists — update amount/polarity
    auto it = connections_.find(target);
    if (it != connections_.end()) {
        for (auto& entry : it->second) {
            if (entry.source_id == src) {
                entry.amount = amount;
                entry.polarity = polarity;
                entry.bypassed = false;
                return true;
            }
        }
    }

    // Add new connection
    connections_[target].push_back({
        src, target, amount, polarity, false
    });
    source_targets_[src].push_back(target);

    return true;
}

bool ModulationMatrix::disconnect(SourceID src, ParameterID target) {
    auto it = connections_.find(target);
    if (it == connections_.end()) return false;

    auto& entries = it->second;
    auto entry_it = std::find_if(entries.begin(), entries.end(),
        [src](const ModulationEntry& e) { return e.source_id == src; });

    if (entry_it == entries.end()) return false;

    entries.erase(entry_it);

    // Clean up empty connection lists
    if (entries.empty()) {
        connections_.erase(it);
    }

    // Update reverse index
    auto src_it = source_targets_.find(src);
    if (src_it != source_targets_.end()) {
        auto& targets = src_it->second;
        targets.erase(std::remove(targets.begin(), targets.end(), target),
                      targets.end());
        if (targets.empty()) {
            source_targets_.erase(src_it);
        }
    }

    return true;
}

void ModulationMatrix::clear_target(ParameterID target) {
    auto it = connections_.find(target);
    if (it == connections_.end()) return;

    // Remove from reverse index
    for (const auto& entry : it->second) {
        auto src_it = source_targets_.find(entry.source_id);
        if (src_it != source_targets_.end()) {
            auto& targets = src_it->second;
            targets.erase(std::remove(targets.begin(), targets.end(), target),
                          targets.end());
            if (targets.empty()) {
                source_targets_.erase(src_it);
            }
        }
    }

    connections_.erase(it);
}

void ModulationMatrix::clear_source(SourceID src) {
    auto src_it = source_targets_.find(src);
    if (src_it == source_targets_.end()) return;

    const auto targets = src_it->second;  // copy
    for (auto target : targets) {
        auto conn_it = connections_.find(target);
        if (conn_it != connections_.end()) {
            auto& entries = conn_it->second;
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                [src](const ModulationEntry& e) { return e.source_id == src; }),
                entries.end());
            if (entries.empty()) {
                connections_.erase(conn_it);
            }
        }
    }

    source_targets_.erase(src_it);
}

void ModulationMatrix::clear_all() {
    connections_.clear();
    source_targets_.clear();
}

// ─── Bypass ───────────────────────────────────────────────

void ModulationMatrix::set_bypass(SourceID src, ParameterID target,
                                   bool bypassed)
{
    auto it = connections_.find(target);
    if (it == connections_.end()) return;

    for (auto& entry : it->second) {
        if (entry.source_id == src) {
            entry.bypassed = bypassed;
            return;
        }
    }
}

bool ModulationMatrix::is_bypassed(SourceID src, ParameterID target) const {
    auto it = connections_.find(target);
    if (it == connections_.end()) return false;

    for (const auto& entry : it->second) {
        if (entry.source_id == src) {
            return entry.bypassed;
        }
    }
    return false;
}

// ─── Queries ──────────────────────────────────────────────

std::vector<ModulationEntry>
ModulationMatrix::connections_for(ParameterID target) const {
    auto it = connections_.find(target);
    if (it != connections_.end()) {
        return it->second;
    }
    return {};
}

size_t ModulationMatrix::connection_count(ParameterID target) const {
    auto it = connections_.find(target);
    return (it != connections_.end()) ? it->second.size() : 0;
}

size_t ModulationMatrix::total_connections() const {
    size_t count = 0;
    for (const auto& [_, entries] : connections_) {
        count += entries.size();
    }
    return count;
}

bool ModulationMatrix::has_connection(SourceID src, ParameterID target) const {
    auto it = connections_.find(target);
    if (it == connections_.end()) return false;

    return std::any_of(it->second.begin(), it->second.end(),
        [src](const ModulationEntry& e) { return e.source_id == src; });
}

// ─── Cycle Detection ──────────────────────────────────────

bool ModulationMatrix::dfs_has_cycle(SourceID current, ParameterID target,
                                      std::vector<SourceID>& visited) const
{
    // If the current source itself modulates the target, we have a cycle:
    // current → target and target wants to connect back to current.
    auto conn_it = connections_.find(target);
    if (conn_it != connections_.end()) {
        for (const auto& entry : conn_it->second) {
            if (current != 0 && entry.source_id == current) {
                return true;  // cycle found: current modulates target already
            }
        }
    }

    // Mark as visited
    if (std::find(visited.begin(), visited.end(), current) != visited.end()) {
        return false;  // already traversed this branch
    }
    visited.push_back(current);

    // Recursively check all targets that `current` modulates
    auto src_it = source_targets_.find(current);
    if (src_it != source_targets_.end()) {
        for (auto modulated_target : src_it->second) {
            // For each target that `current` modulates, check if any
            // source that modulates `modulated_target` could connect
            // back to `target`, creating a cycle.
            auto check_it = connections_.find(modulated_target);
            if (check_it != connections_.end()) {
                for (const auto& entry : check_it->second) {
                    // entry.source_id modulates `modulated_target`
                    // Is this source in the chain back to the original target?
                    // Check if any of entry.source_id's targets include target
                    auto src_targets_it = source_targets_.find(entry.source_id);
                    if (src_targets_it != source_targets_.end()) {
                        for (auto st : src_targets_it->second) {
                            if (st == target) {
                                return true; // cycle!
                            }
                        }
                    }
                    // Recurse deeper
                    if (dfs_has_cycle(entry.source_id, target, visited)) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool ModulationMatrix::has_cycle(SourceID src, ParameterID target) const
{
    if (src == 0) return false;

    std::vector<SourceID> visited;

    // Check if `target`'s current modulators (directly or transitively)
    // connect to `src`, which would create a cycle if we add src → target.
    return dfs_has_cycle(src, target, visited);
}

// ─── Evaluation ───────────────────────────────────────────

double ModulationMatrix::evaluate(ParameterID target, double base_value,
                                   uint64_t ppqn,
                                   const ModulationContext& ctx) const
{
    auto it = connections_.find(target);
    if (it == connections_.end()) return base_value;

    double result = base_value;

    for (const auto& entry : it->second) {
        if (entry.bypassed) continue;

        // We need the ModulationSource pointer to evaluate.
        // The caller is expected to have the source registry.
        // For now, we apply the amount as a proportional offset
        // using the source's evaluate via the modulation context.
        // This is a simplified evaluation — the engine wires the
        // actual source pointer lookup.
        double mod_amount = static_cast<double>(entry.amount);
        result = result + mod_amount;
    }

    return std::clamp(result, 0.0, 1.0);
}

} // namespace aria::automation
