#include "audio_graph.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace aria {

AudioGraph::AudioGraph() = default;
AudioGraph::~AudioGraph() = default;

// ─── Topology ──────────────────────────────────────────────────

NodeID AudioGraph::add_node(std::unique_ptr<AudioNode> node) {
    if (!node) return kInvalidNodeID;

    NodeID id = next_id_++;

    // Ensure the vector is large enough
    if (id >= nodes_.size()) {
        nodes_.resize(id + 1);
    }

    nodes_[id].node = std::move(node);
    dirty_ = true;
    return id;
}

void AudioGraph::remove_node(NodeID id) {
    if (id >= nodes_.size() || !nodes_[id].node) return;

    // Disconnect all connections involving this node
    disconnect_all(id);

    // Clear the node
    nodes_[id].node.reset();
    nodes_[id].inputs.clear();
    nodes_[id].outputs.clear();
    dirty_ = true;
}

void AudioGraph::connect(NodeID source, NodeID target,
                          uint32_t source_ch, uint32_t target_ch) {
    if (source >= nodes_.size() || target >= nodes_.size()) return;
    if (!nodes_[source].node || !nodes_[target].node) return;

    // Add connection to target's inputs
    GraphConnection conn;
    conn.source = source;
    conn.source_channel = source_ch;
    conn.target = target;
    conn.target_channel = target_ch;

    nodes_[target].inputs.push_back(conn);

    // Add to source's outputs
    GraphConnection out_conn;
    out_conn.source = source;
    out_conn.source_channel = source_ch;
    out_conn.target = target;
    out_conn.target_channel = target_ch;

    nodes_[source].outputs.push_back(out_conn);

    dirty_ = true;
}

void AudioGraph::disconnect(NodeID source, NodeID target,
                             uint32_t source_ch, uint32_t target_ch) {
    if (source >= nodes_.size() || target >= nodes_.size()) return;

    // Remove from target's inputs
    auto& inputs = nodes_[target].inputs;
    inputs.erase(
        std::remove_if(inputs.begin(), inputs.end(),
            [&](const GraphConnection& c) {
                return c.source == source &&
                       c.target == target &&
                       c.source_channel == source_ch &&
                       c.target_channel == target_ch;
            }),
        inputs.end());

    // Remove from source's outputs
    auto& outputs = nodes_[source].outputs;
    outputs.erase(
        std::remove_if(outputs.begin(), outputs.end(),
            [&](const GraphConnection& c) {
                return c.source == source &&
                       c.target == target &&
                       c.source_channel == source_ch &&
                       c.target_channel == target_ch;
            }),
        outputs.end());

    dirty_ = true;
}

void AudioGraph::disconnect_all(NodeID id) {
    if (id >= nodes_.size()) return;

    // Remove all connections targeting this node
    for (auto& entry : nodes_) {
        if (entry.node) {
            entry.inputs.erase(
                std::remove_if(entry.inputs.begin(), entry.inputs.end(),
                    [id](const GraphConnection& c) {
                        return c.source == id || c.target == id;
                    }),
                entry.inputs.end());

            entry.outputs.erase(
                std::remove_if(entry.outputs.begin(), entry.outputs.end(),
                    [id](const GraphConnection& c) {
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

bool AudioGraph::would_create_cycle(NodeID source, NodeID target) const {
    if (source >= nodes_.size() || target >= nodes_.size()) return false;
    if (!nodes_[source].node || !nodes_[target].node) return false;
    if (source == target) return true;

    // BFS from source following outputs — if we reach target, cycle exists
    std::vector<std::vector<NodeID>> adj(nodes_.size());
    for (NodeID i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].node) {
            for (const auto& conn : nodes_[i].outputs) {
                adj[i].push_back(conn.target);
            }
        }
    }

    // Also check if adding this connection would create a cycle:
    // can target reach source?
    return can_reach(target, source, adj);
}

AudioNode* AudioGraph::get_node(NodeID id) const {
    if (id >= nodes_.size()) return nullptr;
    return nodes_[id].node.get();
}

// ─── Rebuild (topological sort) ────────────────────────────────

bool AudioGraph::rebuild() {
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
    std::vector<std::vector<NodeID>> adj(nodes_.size());
    std::vector<uint32_t> in_degree(nodes_.size(), 0);

    for (NodeID i = 0; i < nodes_.size(); ++i) {
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
    std::queue<NodeID> q;
    for (NodeID i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].node && in_degree[i] == 0) {
            q.push(i);
        }
    }

    std::vector<NodeID> sorted;
    sorted.reserve(active);

    while (!q.empty()) {
        NodeID n = q.front();
        q.pop();
        sorted.push_back(n);

        for (NodeID neighbor : adj[n]) {
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

bool AudioGraph::can_reach(NodeID source, NodeID target,
                            const std::vector<std::vector<NodeID>>& adj) const {
    if (source == target) return true;

    std::vector<bool> visited(nodes_.size(), false);
    std::queue<NodeID> q;
    q.push(source);
    visited[source] = true;

    while (!q.empty()) {
        NodeID n = q.front();
        q.pop();

        for (NodeID neighbor : adj[n]) {
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

void AudioGraph::process(uint32_t frames, uint64_t sample_position) {
    (void)sample_position;
    if (!pool_) return;

    // Rebuild if dirty
    if (dirty_) {
        if (!rebuild()) {
            // Cycle detected — can't process
            return;
        }
    }

    if (sorted_nodes_.empty()) return;

    std::lock_guard<std::mutex> lock(process_mutex_);

    // Phase 1: acquire output buffers for every node
    struct NodeBuffers {
        std::vector<AudioBuffer*> outputs;
    };

    // Pre-allocate buffer lists
    std::vector<NodeBuffers> node_bufs(sorted_nodes_.size());

    for (size_t si = 0; si < sorted_nodes_.size(); ++si) {
        NodeID id = sorted_nodes_[si];
        auto& entry = nodes_[id];

        if (!entry.node) continue;

        // Determine output count from connections + knowledge
        // Each output connection needs a buffer. For leaf nodes with no outputs
        // they get 0 output buffers.
        uint32_t out_count = 0;

        // Count unique output channels
        std::unordered_set<uint32_t> out_channels;
        for (const auto& conn : entry.outputs) {
            out_channels.insert(conn.source_channel);
        }

        // If node has no outputs, allocate 1 buffer as a placeholder
        // so process() gets a valid outputs array to write into.
        // (e.g., AudioOutputNode may need buffers to output to)
        if (out_channels.empty()) {
            out_count = 1;
        } else {
            out_count = static_cast<uint32_t>(out_channels.size());
        }

    // Acquire buffers
        node_bufs[si].outputs.resize(out_count, nullptr);
        for (uint32_t c = 0; c < out_count; ++c) {
            AudioBuffer* buf = pool_->acquire();
            if (buf) {
                buf->frames = frames;
                buf->channels = 1;  // each output buf = 1 logical channel
                node_bufs[si].outputs[c] = buf;
            }
            }
        }

    try {
        // Phase 2: process nodes in topological order
        for (size_t si = 0; si < sorted_nodes_.size(); ++si) {
            NodeID id = sorted_nodes_[si];
            auto& entry = nodes_[id];
            if (!entry.node) continue;

            // Gather input buffers from upstream connections
            std::vector<AudioBuffer*> inputs;
            for (const auto& conn : entry.inputs) {
                // Find which sorted index produced the source
                // We need to map source NodeID to its buffer
                for (size_t sj = 0; sj < sorted_nodes_.size(); ++sj) {
                    if (sorted_nodes_[sj] == conn.source) {
                        if (conn.source_channel < node_bufs[sj].outputs.size()) {
                            inputs.push_back(node_bufs[sj].outputs[conn.source_channel]);
                        }
                        break;
                    }
                }
            }

            // Input buffers: each is a separate "logical input channel"
            // The node's process() receives them as inputs[ch]

            AudioNode::ProcessContext ctx;
            ctx.frames = frames;
            ctx.sample_rate = 48000.0;  // default, should come from engine
            ctx.sample_position = sample_position;
            ctx.inputs = inputs.data();
            ctx.input_count = static_cast<uint32_t>(inputs.size());
            ctx.outputs = node_bufs[si].outputs.data();
            ctx.output_count = static_cast<uint32_t>(node_bufs[si].outputs.size());

            // Clear output buffers before processing
            for (uint32_t c = 0; c < ctx.output_count; ++c) {
                if (ctx.outputs[c]) {
                    ctx.outputs[c]->frames = frames;
                }
            }

            entry.node->process(ctx);
        }
    } catch (...) {
        // If anything goes wrong during processing, release all buffers
        // and rethrow (audio thread should never throw, but be safe)
        for (auto& nb : node_bufs) {
            for (auto* buf : nb.outputs) {
                if (buf) pool_->release(buf);
            }
        }
        throw;
    }

    // Phase 3: release all acquired buffers
    for (auto& nb : node_bufs) {
        for (auto* buf : nb.outputs) {
            if (buf) pool_->release(buf);
        }
    }
}

} // namespace aria
