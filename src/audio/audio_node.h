#ifndef ARIA_AUDIO_NODE_H
#define ARIA_AUDIO_NODE_H

#include "audio_buffer.h"
#include <cstdint>
#include <memory>
#include <string>

namespace aria {

/// Unique identifier for a node within an AudioGraph.
using NodeID = uint32_t;

/// Invalid/null node ID constant.
static constexpr NodeID kInvalidNodeID = UINT32_MAX;

/// Base class for all audio processing nodes in the graph.
///
/// Subclasses implement process() to perform their DSP work.
/// Nodes are owned by the AudioGraph — do not destroy them manually.
class AudioNode {
public:
    virtual ~AudioNode() = default;

    /// Context passed to process() on every callback.
    struct ProcessContext {
        uint32_t     frames;          ///< Number of frames to process
        double       sample_rate;     ///< Current sample rate (Hz)
        uint64_t     sample_position; ///< Global sample position

        AudioBuffer** inputs;         ///< Array of input buffers
        AudioBuffer** outputs;        ///< Array of output buffers
        uint32_t      input_count;    ///< Number of input buffers
        uint32_t      output_count;   ///< Number of output buffers
    };

    /// Process audio through this node.
    /// Called from the audio thread — MUST be real-time safe.
    virtual void process(ProcessContext& ctx) = 0;

    /// Reset internal state (e.g., on transport stop or sample rate change).
    virtual void reset() {}

    /// Latency introduced by this node in samples.
    virtual uint32_t latency() const { return 0; }

    /// Human-readable label for debugging/metering.
    virtual const char* label() const { return "AudioNode"; }

    /// @name Parameter API
    /// Thread-safe parameter access. Default implementation uses AtomicParameter
    /// or can be overridden for custom logic.
    ///@{
    virtual void   set_parameter(uint32_t index, double value);
    virtual double get_parameter(uint32_t index) const;
    virtual uint32_t parameter_count() const { return 0; }
    ///@}
};

} // namespace aria

#endif // ARIA_AUDIO_NODE_H
