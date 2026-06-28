#ifndef ARIA_PLUGIN_AUDIO_NODE_H
#define ARIA_PLUGIN_AUDIO_NODE_H

#include "audio/audio_node.h"
#include "audio_plugin.h"
#include "plugin_sandbox.h"

#include <memory>
#include <vector>

namespace aria {

/// Adapter that wraps an AudioPlugin as an AudioNode for the mixer graph.
///
/// Converts AudioNode's ProcessContext (AudioBuffer arrays) into the
/// flat per-channel pointer arrays that AudioPlugin::process() expects.
/// If a PluginSandbox is provided, all plugin processing happens on the
/// sandbox's worker thread. Otherwise, the plugin is called directly
/// on the audio thread (for testing or non-isolated native plugins).
///
/// Bypass passthrough: when the ProcessContext has is_bypassed=true,
/// the adapter copies input samples directly to outputs without
/// calling the plugin.
class PluginAudioNode : public AudioNode {
public:
    /// Construct an adapter that optionally uses a sandbox.
    /// @param plugin   The AudioPlugin to wrap (takes ownership)
    /// @param sandbox  Optional sandbox for isolated processing
    PluginAudioNode(std::unique_ptr<plugin::AudioPlugin> plugin,
                    std::unique_ptr<plugin::PluginSandbox> sandbox = nullptr);

    ~PluginAudioNode() override = default;

    // ── AudioNode interface ─────────────────────────────────────

    void process(ProcessContext& ctx) override;
    void reset() override;
    const char* label() const override;
    uint32_t latency() const override;

    // ── Parameter API (forwards to AudioPlugin) ─────────────────

    void set_parameter(uint32_t index, double value) override;
    double get_parameter(uint32_t index) const override;
    uint32_t parameter_count() const override;

    // ── Accessors ───────────────────────────────────────────────

    plugin::AudioPlugin* plugin() { return plugin_.get(); }
    const plugin::AudioPlugin* plugin() const { return plugin_.get(); }

    plugin::PluginSandbox* sandbox() { return sandbox_.get(); }
    const plugin::PluginSandbox* sandbox() const { return sandbox_.get(); }

private:
    /// Flatten AudioBuffer arrays into per-channel float pointer vectors.
    /// @param buffers  Array of AudioBuffer pointers
    /// @param count    Number of buffers
    /// @param out      Output vector filled with channel pointers
    void flatten_buffers(AudioBuffer** buffers, uint32_t count,
                         std::vector<const float*>& out) const;

    /// Non-const overload for output flattening.
    void flatten_buffers(AudioBuffer** buffers, uint32_t count,
                         std::vector<float*>& out) const;

    std::unique_ptr<plugin::AudioPlugin> plugin_;
    std::unique_ptr<plugin::PluginSandbox> sandbox_;

    // Reusable scratch vectors for flattened channel pointers.
    // These are recycled across process() calls to avoid allocation.
    std::vector<const float*> input_channels_;
    std::vector<float*> output_channels_;
};

} // namespace aria

#endif // ARIA_PLUGIN_AUDIO_NODE_H
