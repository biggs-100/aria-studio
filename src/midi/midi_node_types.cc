#include "midi_node_types.h"
#include <algorithm>

namespace aria {

// ─── MidiInputNode ──────────────────────────────────────────────

MidiInputNode::MidiInputNode() = default;

void MidiInputNode::process(ProcessContext& ctx) {
    // Move pending events to output
    if (!pending_events_.empty()) {
        ctx.output_events.insert(
            ctx.output_events.end(),
            std::make_move_iterator(pending_events_.begin()),
            std::make_move_iterator(pending_events_.end()));
        pending_events_.clear();
    }
}

void MidiInputNode::reset() {
    pending_events_.clear();
}

void MidiInputNode::push_events(const std::vector<MidiEvent>& events) {
    pending_events_.insert(pending_events_.end(), events.begin(), events.end());
}

// ─── MidiOutputNode ─────────────────────────────────────────────

MidiOutputNode::MidiOutputNode() = default;

void MidiOutputNode::process(ProcessContext& ctx) {
    // Capture all input events into the output buffer
    output_buffer_.insert(
        output_buffer_.end(),
        ctx.input_events.begin(),
        ctx.input_events.end());

    // Pass through to graph
    ctx.output_events = ctx.input_events;
}

void MidiOutputNode::reset() {
    output_buffer_.clear();
}

std::vector<MidiEvent> MidiOutputNode::drain_output() {
    std::vector<MidiEvent> drained;
    drained.swap(output_buffer_);
    return drained;
}

// ─── MidiRouterNode ─────────────────────────────────────────────

MidiRouterNode::MidiRouterNode() = default;

void MidiRouterNode::process(ProcessContext& ctx) {
    // Forward all input events to each output.
    // The graph topology determines how many outputs exist.
    // Each output receives the same event stream.
    ctx.output_events = ctx.input_events;
}

void MidiRouterNode::reset() {
    // No state to reset
}

// ─── MidiTransformerNode ────────────────────────────────────────

MidiTransformerNode::MidiTransformerNode() = default;

void MidiTransformerNode::process(ProcessContext& ctx) {
    for (const auto& event : ctx.input_events) {
        MidiEvent out = event;

        if (out.is_note()) {
            // Transpose
            if (transpose_ != 0) {
                int transposed = static_cast<int>(out.data1) + transpose_;
                out.data1 = static_cast<uint8_t>(std::clamp(transposed, 0, 127));
            }

            // Channel remap
            if (channel_remap_ >= 0 && channel_remap_ <= 15) {
                out.channel = static_cast<uint8_t>(channel_remap_);
            }
        }

        ctx.output_events.push_back(out);
    }
}

void MidiTransformerNode::reset() {
    // No state to reset — transposition/channel mapping persists
}

} // namespace aria
