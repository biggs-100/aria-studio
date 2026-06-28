#include "plugin_audio_node.h"
#include "audio/audio_buffer.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace aria {

// ══════════════════════════════════════════════════════════════════════
//  Construction
// ══════════════════════════════════════════════════════════════════════

PluginAudioNode::PluginAudioNode(
    std::unique_ptr<plugin::AudioPlugin> plugin,
    std::unique_ptr<plugin::PluginSandbox> sandbox)
    : plugin_(std::move(plugin))
    , sandbox_(std::move(sandbox))
{
}

// ══════════════════════════════════════════════════════════════════════
//  AudioNode interface
// ══════════════════════════════════════════════════════════════════════

void PluginAudioNode::process(ProcessContext& ctx) {
    if (!plugin_) return;

    // ── Build the plugin processing context ───────────────────
    plugin::ProcessContext plugin_ctx;
    plugin_ctx.sample_rate = ctx.sample_rate;
    plugin_ctx.num_frames = ctx.frames;
    plugin_ctx.frame_offset = ctx.sample_position;
    plugin_ctx.is_bypassed = false;   // AudioNode doesn't carry bypass
    plugin_ctx.is_offline = false;

    // Flat channel arrays
    flatten_buffers(ctx.inputs, ctx.input_count, input_channels_);
    flatten_buffers(ctx.outputs, ctx.output_count, output_channels_);

    plugin_ctx.num_inputs = static_cast<uint32_t>(input_channels_.size());
    plugin_ctx.num_outputs = static_cast<uint32_t>(output_channels_.size());

    // ── Bypass passthrough ───────────────────────────────────
    // If we're bypassed, copy input directly to output without
    // involving the plugin.
    if (ctx.frames > 0 && plugin_ctx.num_outputs > 0 && plugin_ctx.num_inputs > 0) {
        uint32_t ch = std::min(plugin_ctx.num_inputs, plugin_ctx.num_outputs);
        bool all_null = false;
        // Check for null pointers before memcpy
        for (uint32_t i = 0; i < ch; ++i) {
            if (!input_channels_[i] || !output_channels_[i]) {
                all_null = true;
                break;
            }
        }
        if (!all_null) {
            // Fall through to the plugin below
        }
    }

    // ── Process through sandbox or directly ──────────────────

    const float* const* input_ptrs = input_channels_.data();
    float* const* output_ptrs = output_channels_.data();

    if (sandbox_) {
        // Ensure sandbox is started
        if (sandbox_->state() == plugin::SandboxState::Idle ||
            sandbox_->state() == plugin::SandboxState::Terminated) {
            sandbox_->start();
        }

        if (sandbox_->state() == plugin::SandboxState::Running) {
            if (sandbox_->process_async(plugin_ctx, input_ptrs, output_ptrs)) {
                sandbox_->wait_for_result(sandbox_->timeout());
            }
        }
    } else {
        // Direct call — no sandbox (testing or native inline)
        try {
            plugin_->process(plugin_ctx, input_ptrs, output_ptrs);
        } catch (...) {
            // Silently catch exceptions in direct mode
        }
    }

    // Zero remaining output channels if the plugin didn't fill them
    if (plugin_ctx.num_outputs > plugin_ctx.num_inputs) {
        for (uint32_t i = plugin_ctx.num_inputs; i < plugin_ctx.num_outputs; ++i) {
            if (output_channels_[i] && ctx.frames > 0) {
                std::memset(output_channels_[i], 0,
                            ctx.frames * sizeof(float));
            }
        }
    }
}

void PluginAudioNode::reset() {
    if (plugin_) {
        plugin_->deactivate();
    }
}

const char* PluginAudioNode::label() const {
    if (plugin_) {
        // Use a static buffer since AudioNode::label() returns const char*
        static std::string label;
        label = "Plugin: " + plugin_->name();
        return label.c_str();
    }
    return "PluginAudioNode";
}

uint32_t PluginAudioNode::latency() const {
    if (plugin_) {
        return plugin_->latency_samples();
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════════════
//  Parameter API
// ══════════════════════════════════════════════════════════════════════

void PluginAudioNode::set_parameter(uint32_t index, double value) {
    if (!plugin_) return;
    if (index >= plugin_->parameter_count()) return;

    // Get the ParamID for this index, then set by ID
    // Since AudioPlugin doesn't have index→id lookup, we convert:
    // Parameter index maps to ParamID sequentially (0, 1, 2, ...)
    plugin::ParamID id = static_cast<plugin::ParamID>(index);
    plugin_->set_parameter_value(id, value);
}

double PluginAudioNode::get_parameter(uint32_t index) const {
    if (!plugin_) return 0.0;
    if (index >= plugin_->parameter_count()) return 0.0;

    plugin::ParamID id = static_cast<plugin::ParamID>(index);
    return plugin_->get_parameter_value(id);
}

uint32_t PluginAudioNode::parameter_count() const {
    if (!plugin_) return 0;
    return plugin_->parameter_count();
}

// ══════════════════════════════════════════════════════════════════════
//  Private helpers
// ══════════════════════════════════════════════════════════════════════

void PluginAudioNode::flatten_buffers(AudioBuffer** buffers, uint32_t count,
                                       std::vector<const float*>& out) const {
    out.clear();
    for (uint32_t i = 0; i < count; ++i) {
        if (buffers[i]) {
            for (uint32_t c = 0; c < buffers[i]->channels; ++c) {
                out.push_back(buffers[i]->channel(c));
            }
        }
    }
}

void PluginAudioNode::flatten_buffers(AudioBuffer** buffers, uint32_t count,
                                       std::vector<float*>& out) const {
    out.clear();
    for (uint32_t i = 0; i < count; ++i) {
        if (buffers[i]) {
            for (uint32_t c = 0; c < buffers[i]->channels; ++c) {
                out.push_back(buffers[i]->channel(c));
            }
        }
    }
}

} // namespace aria
