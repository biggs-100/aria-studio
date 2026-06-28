#include "midi_graph.h"
#include <algorithm>
#include <queue>

namespace aria {

MidiGraph::MidiGraph() = default;
MidiGraph::~MidiGraph() = default;

// ─── Topology ──────────────────────────────────────────────────

MidiNodeID MidiGraph::add_node(std::unique_ptr<MidiNode> node) {
    if (!node) return kInvalidMidiNodeID;

    MidiNodeID id = next_id_++;

    if (id >= nodes_.size()) {
        nodes_.resize(id + 1);
    }

    nodes_[id].node = std::move(node);
    dirty_ = true;
    return id;
}

void MidiGraph::remove_node(MidiNodeID id) {
    if (id >= nodes_.size() || !nodes_[id].node) return;

    // Disconnect all connections involving this node
    disconnect_all(id);

    // Clear the node entry
    nodes_[id].node.reset();
    nodes_[id].inputs.clear();
    nodes_[id].outputs.clear();
    dirty_ = true;
}

void MidiGraph::connect(MidiNodeID source, MidiNodeID target) {
    if (source >= nodes_.size() || target >= nodes_.size()) return;
    if (!nodes_[source].node || !nodes_[target].node) return;

    MidiGraphConnection conn;
    conn.source = source;
    conn.target = target;

    nodes_[target].inputs.push_back(conn);
    nodes_[source].outputs.push_back(conn);

    dirty_ = true;
}

void MidiGraph::disconnect(MidiNodeID source, MidiNodeID target) {
    if (source >= nodes_.size() || target >= nodes_.size()) return;

    // Remove from target's inputs
    auto& inputs = nodes_[target].inputs;
    inputs.erase(
        std::remove_if(inputs.begin(), inputs.end(),
            [&](const MidiGraphConnection& c) {
                return c.source == source && c.target == target;
            }),
        inputs.end());

    // Remove from source's outputs
    auto& outputs = nodes_[source].outputs;
    outputs.erase(
        std::remove_if(outputs.begin(), outputs.end(),
            [&](const MidiGraphConnection& c) {
                return c.source == source && c.target == target;
            }),
        outputs.end());

    dirty_ = true;
}

void MidiGraph::disconnect_all(MidiNodeID id) {
    if (id >= nodes_.size()) return;

    // Remove all connections involving this node
    for (auto& entry : nodes_) {
        if (entry.node) {
            entry.inputs.erase(
                std::remove_if(entry.inputs.begin(), entry.inputs.end(),
                    [id](const MidiGraphConnection& c) {
                        return c.source == id || c.target == id;
                    }),
                entry.inputs.end());

            entry.outputs.erase(
                std::remove_if(entry.outputs.begin(), entry.outputs.end(),
                    [id](const MidiGraphConnection& c) {
                        return c.source == id || c.target == id;
                    }),
                entry.outputs.end());
        }
    }

    // Also clear this node's own lists
    nodes_[id].inputs.clear();
    nodes_[id].outputs.clear();

    dirty_ = true;
}

bool MidiGraph::would_create_cycle(MidiNodeID source, MidiNodeID target) const {
    if (source >= nodes_.size() || target >= nodes_.size()) return false;
    if (!nodes_[source].node || !nodes_[target].node) return false;
    if (source == target) return true;

    // Build adjacency list from current outputs
    std::vector<std::vector<MidiNodeID>> adj(nodes_.size());
    for (MidiNodeID i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].node) {
            for (const auto& conn : nodes_[i].outputs) {
                adj[i].push_back(conn.target);
            }
        }
    }

    // If target can reach source, adding source→target creates a cycle
    return can_reach(target, source, adj);
}

MidiNode* MidiGraph::get_node(MidiNodeID id) const {
    if (id >= nodes_.size()) return nullptr;
    return nodes_[id].node.get();
}

// ─── Rebuild (topological sort) ────────────────────────────────

bool MidiGraph::rebuild() {
    std::lock_guard<std::mutex> lock(process_mutex_);

    // Count active nodes
    uint32_t active = 0;
    for (auto& entry : nodes_) {
        if (entry.node) ++active;
    }
    if (active == 0) {
        sorted_nodes_.clear();
        dirty_ = false;
        return true;
    }

    // Build adjacency list and in-degree count
    std::vector<std::vector<MidiNodeID>> adj(nodes_.size());
    std::vector<uint32_t> in_degree(nodes_.size(), 0);

    for (MidiNodeID i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].node) {
            for (const auto& conn : nodes_[i].outputs) {
                if (conn.target < nodes_.size() && nodes_[conn.target].node) {
                    adj[i].push_back(conn.target);
                    in_degree[conn.target]++;
                }
            }
        }
    }

    // Kahn's algorithm
    std::queue<MidiNodeID> q;
    for (MidiNodeID i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].node && in_degree[i] == 0) {
            q.push(i);
        }
    }

    std::vector<MidiNodeID> sorted;
    sorted.reserve(active);

    while (!q.empty()) {
        MidiNodeID n = q.front();
        q.pop();
        sorted.push_back(n);

        for (MidiNodeID neighbor : adj[n]) {
            if (--in_degree[neighbor] == 0) {
                q.push(neighbor);
            }
        }
    }

    // Check for cycle
    if (sorted.size() != active) {
        return false;  // cycle detected
    }

    sorted_nodes_ = std::move(sorted);
    dirty_ = false;
    return true;
}

bool MidiGraph::can_reach(MidiNodeID source, MidiNodeID target,
                           const std::vector<std::vector<MidiNodeID>>& adj) const {
    if (source == target) return true;

    std::vector<bool> visited(nodes_.size(), false);
    std::queue<MidiNodeID> q;
    q.push(source);
    visited[source] = true;

    while (!q.empty()) {
        MidiNodeID n = q.front();
        q.pop();

        for (MidiNodeID neighbor : adj[n]) {
            if (neighbor == target) return true;
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                q.push(neighbor);
            }
        }
    }
    return false;
}

// ─── Processing ────────────────────────────────────────────────

void MidiGraph::process(uint32_t frames, uint64_t sample_position) {
    // Rebuild if dirty
    if (dirty_) {
        if (!rebuild()) {
            // Cycle detected — can't process
            return;
        }
    }

    if (sorted_nodes_.empty()) return;

    std::lock_guard<std::mutex> lock(process_mutex_);

    // Create per-node processing contexts
    std::vector<MidiNode::ProcessContext> node_ctxs(sorted_nodes_.size());

    // Initialize contexts
    for (size_t si = 0; si < sorted_nodes_.size(); ++si) {
        MidiNodeID id = sorted_nodes_[si];
        if (!nodes_[id].node) continue;

        node_ctxs[si].frames           = frames;
        node_ctxs[si].sample_rate      = 48000.0;
        node_ctxs[si].sample_position  = sample_position;
    }

    // Process nodes in topological order, forwarding events downstream
    for (size_t si = 0; si < sorted_nodes_.size(); ++si) {
        MidiNodeID id = sorted_nodes_[si];
        auto& entry = nodes_[id];
        if (!entry.node) continue;

        // Gather input events from all upstream connections
        for (const auto& conn : entry.inputs) {
            for (size_t sj = 0; sj < sorted_nodes_.size(); ++sj) {
                if (sorted_nodes_[sj] == conn.source) {
                    auto& src_ctx = node_ctxs[sj];
                    node_ctxs[si].input_events.insert(
                        node_ctxs[si].input_events.end(),
                        src_ctx.output_events.begin(),
                        src_ctx.output_events.end());
                    break;
                }
            }
        }

        // Process this node
        entry.node->process(node_ctxs[si]);
    }

    // Output events from leaf nodes remain in their contexts for any
    // post-processing (e.g., MidiOutputNode captures its own buffer
    // during process() via drain_output).
}

} // namespace aria
