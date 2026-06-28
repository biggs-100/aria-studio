#ifndef ARIA_AUDIO_NODE_TYPES_H
#define ARIA_AUDIO_NODE_TYPES_H

#include "audio_node.h"
#include "atomic_parameter.h"

namespace aria {

// ─── AudioInputNode ────────────────────────────────────────────
//
/// Input node — provides audio from the audio device (placeholder).
/// Produces `channel_count` output buffers filled with silence.
/// In a full implementation, this would read from the device ring buffer.
class AudioInputNode : public AudioNode {
public:
    explicit AudioInputNode(uint32_t channel_count = 2);

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "AudioInput"; }
    uint32_t parameter_count() const override { return 0; }

    /// Set the input data to be used in the next process call.
    /// This is how the audio driver feeds data into the graph.
    void set_input_data(AudioBuffer* buf) { input_ = buf; }

private:
    uint32_t channel_count_;
    AudioBuffer* input_ = nullptr;
};

// ─── AudioOutputNode ───────────────────────────────────────────
//
/// Output node — accumulates audio for the audio device (placeholder).
/// Reads from its input and stores the buffer for device consumption.
class AudioOutputNode : public AudioNode {
public:
    explicit AudioOutputNode(uint32_t channel_count = 2);

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "AudioOutput"; }
    uint32_t parameter_count() const override { return 0; }

    /// Get the accumulated output data after processing.
    AudioBuffer* output_buffer() { return output_; }
    const AudioBuffer* output_buffer() const { return output_; }

private:
    uint32_t channel_count_;
    AudioBuffer* output_ = nullptr;
};

// ─── GainNode ──────────────────────────────────────────────────
//
/// Applies per-channel gain to the input signal.
/// Parameters:
///   0 — gain (linear amplitude, default 1.0)
class GainNode : public AudioNode {
public:
    GainNode();

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "Gain"; }

    void set_parameter(uint32_t index, double value) override;
    double get_parameter(uint32_t index) const override;
    uint32_t parameter_count() const override { return 1; }

    void set_gain(double g) { gain_.store(static_cast<float>(g)); }
    double gain() const { return static_cast<double>(gain_.load()); }

private:
    AtomicParameter gain_;
};

// ─── MeterNode ─────────────────────────────────────────────────
//
/// Measures peak and RMS levels of the input signal.
/// Passes input through to output unchanged.
class MeterNode : public AudioNode {
public:
    MeterNode();

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override { return "Meter"; }
    uint32_t parameter_count() const override { return 0; }

    float peak(uint32_t channel) const;
    float rms(uint32_t channel) const;

private:
    static constexpr uint32_t kMaxMeterChannels = 32;
    float peak_[kMaxMeterChannels]{};
    float rms_[kMaxMeterChannels]{};
};

// ─── SilenceNode ───────────────────────────────────────────────
//
/// Produces silent output buffers. Useful for testing and as a
/// placeholder for unconnected inputs.
class SilenceNode : public AudioNode {
public:
    explicit SilenceNode(uint32_t channel_count = 2);

    void process(ProcessContext& ctx) override;
    const char* label() const override { return "Silence"; }
    uint32_t parameter_count() const override { return 0; }

private:
    uint32_t channel_count_;
};

} // namespace aria

#endif // ARIA_AUDIO_NODE_TYPES_H
