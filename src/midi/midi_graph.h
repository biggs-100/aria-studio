#ifndef ARIA_MIDI_GRAPH_H
#define ARIA_MIDI_GRAPH_H

#include "midi_node.h"
#include "midi_node_types.h"
#include <memory>
#include <mutex>
#include <vector>

namespace aria {

/// Connection between two MIDI nodes.
struct MidiGraphConnection {
    MidiNodeID source;
    MidiNodeID target;
};

/// Internal node entry with adjacency data.
struct MidiGraphNodeEntry {
    std::unique_ptr<MidiNode> node;
    std::vector<MidiGraphConnection> inputs;   // connections INTO this node
    std::vector<MidiGraphConnection> outputs;  // connections FROM this node
};

/// Directed Acyclic Graph (DAG) of MIDI processing nodes.
///
/// Processes nodes in topological order on each audio callback.
/// Structural changes (add/remove/connect) trigger a rebuild
/// that re-sorts the graph and detects cycles.
///
/// Thread safety:
///   - add_node / remove_node / connect / disconnect are NOT thread-safe
///     and should only be called from the control (UI) thread.
///   - process() is called from the audio thread and is real-time safe.
///   - A mutex guards the sorted node list for audio thread access.
class MidiGraph {
public:
    MidiGraph();
    ~MidiGraph();

    MidiGraph(const MidiGraph&) = delete;
    MidiGraph& operator=(const MidiGraph&) = delete;

    // ─── Graph topology ────────────────────────────────────────

    /// Add a node to the graph. Returns the assigned NodeID.
    MidiNodeID add_node(std::unique_ptr<MidiNode> node);

    /// Remove a node and all its connections.
    void remove_node(MidiNodeID id);

    /// Connect source → target.
    void connect(MidiNodeID source, MidiNodeID target);

    /// Disconnect source → target.
    void disconnect(MidiNodeID source, MidiNodeID target);

    /// Disconnect all connections involving a node.
    void disconnect_all(MidiNodeID id);

    /// Check if connecting source→target would create a cycle.
    bool would_create_cycle(MidiNodeID source, MidiNodeID target) const;

    /// Number of nodes in the graph.
    uint32_t node_count() const {
        return static_cast<uint32_t>(nodes_.size());
    }

    /// Get a node by ID (returns nullptr if invalid).
    MidiNode* get_node(MidiNodeID id) const;

    // ─── Processing ────────────────────────────────────────────

    /// Process the entire graph for one audio callback.
    /// Must only be called from the audio thread.
    void process(uint32_t frames, uint64_t sample_position);

    /// Whether the graph has any nodes.
    bool empty() const { return nodes_.empty(); }

    /// Mark graph as needing a rebuild on next process().
    void set_dirty() { dirty_ = true; }

private:
    /// Topological sort via Kahn's algorithm.
    /// Returns true if the sort succeeded (no cycles).
    bool rebuild();

    /// Check if source can reach target (for cycle detection).
    bool can_reach(MidiNodeID source, MidiNodeID target,
                   const std::vector<std::vector<MidiNodeID>>& adj) const;

    // Node storage — indexed by NodeID.
    std::vector<MidiGraphNodeEntry> nodes_;

    // Sorted node IDs for processing (result of topological sort).
    std::vector<MidiNodeID> sorted_nodes_;

    // Mutex protecting sorted_nodes_ for audio thread access.
    mutable std::mutex process_mutex_;

    // Dirty flag — set when topology changes.
    bool dirty_ = false;

    // Next available node ID.
    MidiNodeID next_id_ = 0;
};

} // namespace aria

#endif // ARIA_MIDI_GRAPH_H
