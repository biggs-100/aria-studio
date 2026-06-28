#ifndef ARIA_MIDI_NODE_H
#define ARIA_MIDI_NODE_H

#include "midi_types.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace aria {

/// Unique identifier for a node within a MidiGraph.
using MidiNodeID = uint64_t;

/// Invalid/null node ID constant.
static constexpr MidiNodeID kInvalidMidiNodeID = UINT64_MAX;

/// Base class for all MIDI processing nodes in the MIDI graph.
///
/// Subclasses implement process() to handle MIDI event streams.
/// Nodes are owned by the MidiGraph — do not destroy them manually.
class MidiNode {
public:
    virtual ~MidiNode() = default;

    /// Context passed to process() on every callback.
    struct ProcessContext {
        uint32_t     frames;            ///< Number of audio frames in this block
        double       sample_rate;       ///< Current sample rate (Hz)
        uint64_t     sample_position;   ///< Global sample position

        std::vector<MidiEvent> input_events;   ///< Incoming MIDI events
        std::vector<MidiEvent> output_events;  ///< Outgoing MIDI events
    };

    /// Process MIDI events through this node.
    /// Called from the audio thread — MUST be real-time safe.
    virtual void process(ProcessContext& ctx) = 0;

    /// Reset internal state (e.g., on transport stop).
    virtual void reset() {}

    /// Human-readable label for debugging.
    virtual const char* label() const { return "MidiNode"; }
};

} // namespace aria

#endif // ARIA_MIDI_NODE_H
