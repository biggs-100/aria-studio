#include "track_processor.h"
#include "simd_ops.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aria {

TrackProcessor::TrackProcessor() = default;

void TrackProcessor::configure(uint32_t num_channels, double sample_rate) {
    num_channels_ = std::min(num_channels, kMaxChannels);
    sample_rate_ = sample_rate;

    // Pre-allocate scratch buffer for plugin chain processing.
    // This buffer holds the intermediate output of one plugin before
    // feeding it to the next. We size it for worst-case buffer size.
    const uint32_t max_frames = 8192;  // reasonable worst-case
    const uint32_t needed = max_frames * num_channels_;
    if (scratch_capacity_ < needed) {
        scratch_.resize(needed);
        scratch_capacity_ = needed;
    }

    // Recalculate PDC for the plugin chain
    std::vector<AudioNode*> chain;
    chain.reserve(plugins_.size());
    for (auto& plugin : plugins_) {
        chain.push_back(plugin.get());
    }
    pdc_.recalculate(chain);
}

void TrackProcessor::process(uint32_t frames, uint64_t sample_position,
                              AudioBuffer* input, AudioBuffer* output) {
    // If muted, output silence
    if (muted_.load(std::memory_order_relaxed)) {
        if (output) {
            for (uint32_t c = 0; c < output->channels && c < num_channels_; ++c) {
                if (output->data[c]) {
                    simd_clear(output->data[c], frames);
                }
            }
            output->frames = frames;
        }
        return;
    }

    // If inactive and no input, output silence
    if (!active_.load(std::memory_order_relaxed) && !input) {
        if (output) {
            for (uint32_t c = 0; c < output->channels && c < num_channels_; ++c) {
                if (output->data[c]) {
                    simd_clear(output->data[c], frames);
                }
            }
            output->frames = frames;
        }
        return;
    }

    // If we have input, copy it to output as the base signal
    if (input && output) {
        uint32_t ch = std::min(input->channels, std::min(output->channels, num_channels_));
        for (uint32_t c = 0; c < ch; ++c) {
            if (input->data[c] && output->data[c]) {
                simd_copy(output->data[c], input->data[c], frames);
            }
        }
    } else if (output) {
        // Clear output if no input
        for (uint32_t c = 0; c < output->channels && c < num_channels_; ++c) {
            if (output->data[c]) {
                simd_clear(output->data[c], frames);
            }
        }
    }

    // Apply time stretch (placeholder — currently pass-through)
    apply_time_stretch(output, frames, sample_position);

    // Apply clip gain
    apply_clip_gain(output, frames);

    // Process through plugin chain
    if (!plugins_.empty() && output) {
        // Create a temporary buffer for plugin chain processing
        // We process in-place through the chain: output is both input and output
        // for each plugin in the chain.
        //
        // For this we need an intermediate buffer to avoid overwriting the
        // output too early. We'll use the scratch buffer.

        // First, copy output into scratch as the starting point for the chain
        const uint32_t bytes = frames * sizeof(float);
        for (uint32_t c = 0; c < num_channels_ && c < output->channels; ++c) {
            if (output->data[c] && (c * frames + frames) <= scratch_capacity_) {
                std::memcpy(&scratch_[c * frames], output->data[c], bytes);
            }
        }

        // Build temporary vectors for the plugin ProcessContext
        std::vector<AudioBuffer> plugin_bufs(num_channels_);
        std::vector<AudioBuffer*> input_ptrs(num_channels_);
        std::vector<AudioBuffer*> output_ptrs(num_channels_);

        for (uint32_t c = 0; c < num_channels_; ++c) {
            plugin_bufs[c].frames = frames;
            plugin_bufs[c].channels = 1;
            plugin_bufs[c].data[0] = &scratch_[c * frames];
            input_ptrs[c] = &plugin_bufs[c];
            output_ptrs[c] = &plugin_bufs[c];
        }

        // Apply PDC compensation delay for this chain
        uint32_t comp_delay = pdc_.max_delay();
        if (comp_delay > 0) {
            // We compensate by delaying the output. The chain's own delay is
            // already built into the plugin processing, so we apply
            // pdc compensation as extra delay.
            // Create a composite buffer from output channels
            for (uint32_t c = 0; c < num_channels_ && c < output->channels; ++c) {
                if (output->data[c]) {
                    // Use a temporary AudioBuffer pointing to output channel
                    AudioBuffer ch_buf;
                    ch_buf.frames = frames;
                    ch_buf.channels = 1;
                    ch_buf.data[0] = output->data[c];
                    pdc_.apply_compensation(ch_buf, comp_delay, reinterpret_cast<uint64_t>(this));
                }
            }
        }

        // Run each plugin in the chain
        AudioNode::ProcessContext ctx;
        ctx.frames = frames;
        ctx.sample_rate = sample_rate_;
        ctx.sample_position = sample_position;
        ctx.inputs = input_ptrs.data();
        ctx.input_count = num_channels_;
        ctx.outputs = output_ptrs.data();
        ctx.output_count = num_channels_;

        for (auto& plugin : plugins_) {
            if (plugin) {
                plugin->process(ctx);
            }
        }

        // Copy plugin chain output back to the output buffer
        for (uint32_t c = 0; c < num_channels_ && c < output->channels; ++c) {
            if (output->data[c]) {
                std::memcpy(output->data[c], &scratch_[c * frames], bytes);
            }
        }
    }

    // Update output frame count
    if (output) {
        output->frames = frames;
    }
}

void TrackProcessor::apply_time_stretch(AudioBuffer* /*buffer*/,
                                         uint32_t /*frames*/,
                                         uint64_t /*sample_position*/) {
    // Placeholder — currently a pass-through.
    // Future implementation will use a time-stretching algorithm (e.g.,
    // WSOLA, phase vocoder, or elastique) to allow variable-speed playback.
}

void TrackProcessor::apply_clip_gain(AudioBuffer* buffer, uint32_t frames) {
    if (!buffer) return;

    float g = gain_;
    if (g == 1.0f && pan_ == 0.0f) return;  // no-op

    if (pan_ == 0.0f) {
        // Simple gain
        for (uint32_t c = 0; c < buffer->channels && c < num_channels_; ++c) {
            if (buffer->data[c]) {
                simd_mul(buffer->data[c], g, frames);
            }
        }
    } else {
        // Apply pan + gain
        float left_gain = g * std::min(1.0f, 1.0f - pan_);
        float right_gain = g * std::min(1.0f, 1.0f + pan_);

        if (num_channels_ >= 1 && buffer->data[0]) {
            simd_mul(buffer->data[0], left_gain, frames);
        }
        if (num_channels_ >= 2 && buffer->data[1]) {
            simd_mul(buffer->data[1], right_gain, frames);
        }
        // For channels beyond stereo, apply center gain
        for (uint32_t c = 2; c < buffer->channels && c < num_channels_; ++c) {
            if (buffer->data[c]) {
                simd_mul(buffer->data[c], g, frames);
            }
        }
    }
}

void TrackProcessor::add_plugin(std::unique_ptr<AudioNode> plugin) {
    if (!plugin) return;

    // Store old chain for PDC recalculation
    std::vector<AudioNode*> old_chain;
    old_chain.reserve(plugins_.size());
    for (auto& p : plugins_) {
        old_chain.push_back(p.get());
    }

    plugins_.push_back(std::move(plugin));

    // Recalculate PDC with new chain
    std::vector<AudioNode*> new_chain;
    new_chain.reserve(plugins_.size());
    for (auto& p : plugins_) {
        new_chain.push_back(p.get());
    }
    pdc_.recalculate(new_chain);
}

std::unique_ptr<AudioNode> TrackProcessor::remove_plugin(uint32_t index) {
    if (index >= plugins_.size()) return nullptr;

    auto plugin = std::move(plugins_[index]);
    plugins_.erase(plugins_.begin() + index);

    // Recalculate PDC
    std::vector<AudioNode*> chain;
    chain.reserve(plugins_.size());
    for (auto& p : plugins_) {
        chain.push_back(p.get());
    }
    pdc_.recalculate(chain);

    return plugin;
}

AudioNode* TrackProcessor::plugin(uint32_t index) const {
    if (index >= plugins_.size()) return nullptr;
    return plugins_[index].get();
}

uint32_t TrackProcessor::chain_latency() const {
    std::vector<AudioNode*> chain;
    chain.reserve(plugins_.size());
    for (auto& p : plugins_) {
        chain.push_back(p.get());
    }
    return pdc_.chain_delay(chain);
}

} // namespace aria
