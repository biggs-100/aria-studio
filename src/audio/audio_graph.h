#ifndef ARIA_AUDIO_GRAPH_H
#define ARIA_AUDIO_GRAPH_H

#include "audio_node.h"
#include "audio_node_types.h"
#include "buffer_pool.h"
#include <memory>
#include <mutex>
#include <vector>

namespace aria {

/// Internal graph connection.
struct GraphConnection {
    NodeID source;
    uint32_t source_channel;
    NodeID target;
    uint32_t target_channel;
};

/// Internal node entry with adjacency data.
struct GraphNodeEntry {
    std::unique_ptr<AudioNode> node;
    std::vector<GraphConnection> inputs;   // connections feeding INTO this node
    std::vector<GraphConnection> outputs;  // connections FROM this node
};

/// Directed Acyclic Graph (DAG) of audio processing nodes.
///
/// The graph processes nodes in topological order on every audio callback.
/// Structural changes (add/remove/connect/disconnect) trigger a rebuild
/// that re-sorts the graph and detects cycles.
///
/// Thread safety:
///   - add_node / remove_node / connect / disconnect are NOT thread-safe
///     and should only be called from the control (UI) thread.
///   - process() is called from the audio thread and is real-time safe.
///   - A mutex guards the sorted node list for audio thread access.
class AudioGraph {
public:
    AudioGraph();
    ~AudioGraph();

    AudioGraph(const AudioGraph&) = delete;
    AudioGraph& operator=(const AudioGraph&) = delete;

    /// Set the buffer pool used for graph processing.
    void set_pool(LockFreeBufferPool* pool) { pool_ = pool; }

    // ─── Graph topology ────────────────────────────────────────

    /// Add a node to the graph. Returns the assigned NodeID.
    NodeID add_node(std::unique_ptr<AudioNode> node);

    /// Remove a node and all its connections.
    void remove_node(NodeID id);

    /// Connect source channel to target channel.
    void connect(NodeID source, NodeID target,
                 uint32_t source_ch = 0, uint32_t target_ch = 0);

    /// Remove a connection.
    void disconnect(NodeID source, NodeID target,
                    uint32_t source_ch = 0, uint32_t target_ch = 0);

    /// Disconnect all connections involving a node.
    void disconnect_all(NodeID id);

    /// Check if connecting source→target would create a cycle.
    bool would_create_cycle(NodeID source, NodeID target) const;

    /// Number of nodes in the graph.
    uint32_t node_count() const { return static_cast<uint32_t>(nodes_.size()); }

    /// Get a node by ID (returns nullptr if invalid).
    AudioNode* get_node(NodeID id) const;

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
    bool can_reach(NodeID source, NodeID target,
                   const std::vector<std::vector<NodeID>>& adj) const;

    LockFreeBufferPool* pool_ = nullptr;

    // Node storage — indexed by NodeID.
    std::vector<GraphNodeEntry> nodes_;

    // Sorted node IDs for processing (result of topological sort).
    std::vector<NodeID> sorted_nodes_;

    // Mutex protecting sorted_nodes_ for audio thread access.
    mutable std::mutex process_mutex_;

    // Dirty flag — set when topology changes.
    bool dirty_ = false;

    // Next available node ID.
    NodeID next_id_ = 0;
};

} // namespace aria

#endif // ARIA_AUDIO_GRAPH_H
