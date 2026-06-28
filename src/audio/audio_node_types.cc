#include "audio_node_types.h"
#include "simd_ops.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace aria {

// ─── AudioInputNode ────────────────────────────────────────────

AudioInputNode::AudioInputNode(uint32_t channel_count)
    : channel_count_(std::min(channel_count, kMaxChannels))
{}

void AudioInputNode::process(ProcessContext& ctx) {
    // Fill output buffers
    // If input data was set via set_input_data(), copy it.
    // Otherwise fill with silence.
    if (input_) {
        for (uint32_t c = 0; c < ctx.output_count && c < input_->channels; ++c) {
            if (ctx.outputs[c] && input_->data[c]) {
                simd_copy(ctx.outputs[c]->data[0], input_->data[c], ctx.frames);
                // Copy across all channels of the output buffer
                // (single-channel input expanded to all output channels if needed)
                if (c == 0 && ctx.output_count > 1) {
                    for (uint32_t ch = 1; ch < ctx.output_count; ++ch) {
                        if (ctx.outputs[ch]) {
                            simd_copy(ctx.outputs[ch]->data[0], input_->data[0], ctx.frames);
                        }
                    }
                    break;
                }
            }
        }
    } else {
        // Silence
        for (uint32_t c = 0; c < ctx.output_count; ++c) {
            if (ctx.outputs[c]) {
                simd_clear(ctx.outputs[c]->data[0], ctx.frames);
            }
        }
    }

    // Update frame counts
    for (uint32_t c = 0; c < ctx.output_count; ++c) {
        if (ctx.outputs[c]) {
            ctx.outputs[c]->frames = ctx.frames;
        }
    }
}

void AudioInputNode::reset() {
    input_ = nullptr;
}

// ─── AudioOutputNode ───────────────────────────────────────────

AudioOutputNode::AudioOutputNode(uint32_t channel_count)
    : channel_count_(std::min(channel_count, kMaxChannels))
{}

void AudioOutputNode::process(ProcessContext& ctx) {
    // Accumulate inputs into the output buffer
    // This is a placeholder — real implementation would copy to device buffer
    if (!output_) return;

    if (ctx.input_count > 0 && ctx.inputs[0]) {
        for (uint32_t c = 0; c < ctx.input_count && c < channel_count_; ++c) {
            if (ctx.inputs[c] && output_->data[c]) {
                simd_copy(output_->data[c], ctx.inputs[c]->data[0], ctx.frames);
            }
        }
        output_->frames = ctx.frames;
    }
}

void AudioOutputNode::reset() {
    if (output_) {
        output_->clear();
        output_->frames = 0;
    }
}

// ─── GainNode ──────────────────────────────────────────────────

GainNode::GainNode()
    : gain_(1.0)
{}

void GainNode::process(ProcessContext& ctx) {
    float g = static_cast<float>(gain_.load());

    uint32_t in_ch = ctx.input_count;
    uint32_t out_ch = ctx.output_count;
    uint32_t ch = std::min(in_ch, out_ch);

    for (uint32_t c = 0; c < ch; ++c) {
        if (ctx.inputs[c] && ctx.outputs[c]) {
            const float* src = ctx.inputs[c]->data[0];
            float* dst = ctx.outputs[c]->data[0];
            if (src && dst) {
                simd_copy(dst, src, ctx.frames);
                simd_mul(dst, g, ctx.frames);
            }
        }
    }

    // Copy remaining output channels from first input (mono→stereo)
    if (in_ch > 0 && out_ch > in_ch) {
        for (uint32_t c = in_ch; c < out_ch; ++c) {
            if (ctx.outputs[c] && ctx.inputs[0]) {
                const float* src = ctx.inputs[0]->data[0];
                float* dst = ctx.outputs[c]->data[0];
                if (src && dst) {
                    simd_copy(dst, src, ctx.frames);
                    simd_mul(dst, g, ctx.frames);
                }
            }
        }
    }

    // Update output frame counts
    for (uint32_t c = 0; c < out_ch; ++c) {
        if (ctx.outputs[c]) {
            ctx.outputs[c]->frames = ctx.frames;
        }
    }
}

void GainNode::reset() {
    // No state to reset — gain persists
}

void GainNode::set_parameter(uint32_t index, double value) {
    if (index == 0) {
        gain_.store(static_cast<float>(value));
    }
}

double GainNode::get_parameter(uint32_t index) const {
    if (index == 0) return static_cast<double>(gain_.load());
    return 0.0;
}

// ─── MeterNode ─────────────────────────────────────────────────

MeterNode::MeterNode() {
    std::memset(peak_, 0, sizeof(peak_));
    std::memset(rms_, 0, sizeof(rms_));
}

void MeterNode::process(ProcessContext& ctx) {
    uint32_t ch = std::min(ctx.input_count, static_cast<uint32_t>(kMaxMeterChannels));

    for (uint32_t c = 0; c < ch; ++c) {
        if (ctx.inputs[c]) {
            const float* buf = ctx.inputs[c]->data[0];
            peak_[c] = simd_peak(buf, ctx.frames);
            rms_[c] = static_cast<float>(simd_rms(buf, ctx.frames));

            // Pass through: copy input to output
            if (c < ctx.output_count && ctx.outputs[c]) {
                simd_copy(ctx.outputs[c]->data[0], buf, ctx.frames);
                ctx.outputs[c]->frames = ctx.frames;
            }
        }
    }

    // Handle extra output channels (copy from first input)
    if (ctx.input_count > 0 && ctx.output_count > ctx.input_count) {
        for (uint32_t c = ctx.input_count; c < ctx.output_count; ++c) {
            if (ctx.outputs[c] && ctx.inputs[0]) {
                simd_copy(ctx.outputs[c]->data[0], ctx.inputs[0]->data[0], ctx.frames);
                ctx.outputs[c]->frames = ctx.frames;
            }
        }
    }
}

void MeterNode::reset() {
    std::memset(peak_, 0, sizeof(peak_));
    std::memset(rms_, 0, sizeof(rms_));
}

float MeterNode::peak(uint32_t channel) const {
    if (channel >= kMaxMeterChannels) return 0.0f;
    return peak_[channel];
}

float MeterNode::rms(uint32_t channel) const {
    if (channel >= kMaxMeterChannels) return 0.0f;
    return rms_[channel];
}

// ─── SilenceNode ───────────────────────────────────────────────

SilenceNode::SilenceNode(uint32_t channel_count)
    : channel_count_(std::min(channel_count, kMaxChannels))
{}

void SilenceNode::process(ProcessContext& ctx) {
    for (uint32_t c = 0; c < ctx.output_count; ++c) {
        if (ctx.outputs[c]) {
            simd_clear(ctx.outputs[c]->data[0], ctx.frames);
            ctx.outputs[c]->frames = ctx.frames;
        }
    }
}

} // namespace aria
