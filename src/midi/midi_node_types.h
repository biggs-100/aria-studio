#ifndef ARIA_MIDI_NODE_TYPES_H
#define ARIA_MIDI_NODE_TYPES_H

#include "midi_node.h"
#include <vector>

namespace aria {

// ─── MidiInputNode ──────────────────────────────────────────────
//
/// Input node — receives MIDI events from a hardware driver.
/// Passes events from an external ring buffer into the MIDI graph.
class MidiInputNode : public MidiNode {
public:
    MidiInputNode();

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "MidiInput"; }

    /// Feed external MIDI events into this node for the next process cycle.
    void push_events(const std::vector<MidiEvent>& events);

private:
    std::vector<MidiEvent> pending_events_;
};

// ─── MidiOutputNode ─────────────────────────────────────────────
//
/// Output node — sends MIDI events to a hardware driver.
/// Captures events from the graph and makes them available for output.
class MidiOutputNode : public MidiNode {
public:
    MidiOutputNode();

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "MidiOutput"; }

    /// Retrieve events captured during the last process cycle.
    std::vector<MidiEvent> drain_output();

private:
    std::vector<MidiEvent> output_buffer_;
};

// ─── MidiRouterNode ─────────────────────────────────────────────
//
/// Router node — duplicates incoming events to multiple destinations.
/// In a standard MIDI graph, this node forks the event stream into N
/// parallel paths. Each connected output receives the same events.
class MidiRouterNode : public MidiNode {
public:
    MidiRouterNode();

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "MidiRouter"; }
};

// ─── MidiTransformerNode ────────────────────────────────────────
//
/// Transformer node — applies transposition and channel remapping
/// to MIDI note events passing through the graph.
class MidiTransformerNode : public MidiNode {
public:
    MidiTransformerNode();

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "MidiTransformer"; }

    /// Set transposition in semitones (-24 to +24).
    void set_transpose(int8_t semitones) { transpose_ = semitones; }
    int8_t transpose() const { return transpose_; }

    /// Remap all output to a specific channel (0-15). Set to -1 for passthrough.
    void set_channel_remap(int8_t channel) { channel_remap_ = channel; }
    int8_t channel_remap() const { return channel_remap_; }

private:
    int8_t transpose_ = 0;
    int8_t channel_remap_ = -1;  // -1 = passthrough, 0-15 = force channel
};

} // namespace aria

#endif // ARIA_MIDI_NODE_TYPES_H
