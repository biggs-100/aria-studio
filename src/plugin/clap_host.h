#ifndef ARIA_PLUGIN_CLAP_HOST_H
#define ARIA_PLUGIN_CLAP_HOST_H

#include "audio_plugin.h"

#include <clap/clap.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace aria::plugin {

// ── CLAPHost ──────────────────────────────────────────────────────────

/// AudioPlugin implementation wrapping a CLAP plugin (.clap binary).
///
/// Loads the CLAP plugin binary, initialises it via clap_plugin_entry,
/// and bridges the AudioPlugin interface to the CLAP C API.
/// Host extensions (audio ports, params, state, etc.) are provided
/// via the clap_host_bridge RAII struct.
class CLAPHost : public AudioPlugin {
public:
    /// Construct a CLAPHost for the plugin at the given file path.
    explicit CLAPHost(std::string plugin_path, PluginID id);
    ~CLAPHost() override;

    // ── AudioPlugin interface ─────────────────────────────────────

    // Lifecycle
    bool init() override;
    bool activate(double sample_rate, uint32_t min_frames, uint32_t max_frames) override;
    void deactivate() override;

    // Identification
    PluginID id() const override;
    std::string name() const override;
    std::string vendor() const override;
    std::string format() const override;
    PluginCategory category() const override;

    // Audio processing
    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override;

    // Parameters
    uint32_t parameter_count() const override;
    ParameterInfo parameter_info(ParamID id) const override;
    double get_parameter_value(ParamID id) const override;
    void set_parameter_value(ParamID id, double value) override;
    void begin_edit(ParamID id) override;
    void end_edit(ParamID id) override;

    // State
    bool save_state(std::vector<uint8_t>& data) const override;
    bool load_state(const std::vector<uint8_t>& data) override;

    // Latency
    uint32_t latency_samples() const override;

    /// Expose the internal clap_host for use by format scanners.
    const clap_host* host() const { return &host_; }

    // ── Host extension structs (public for CLAP C API callbacks) ──

    /// Host audio ports extension callbacks.
    clap_host_audio_ports host_audio_ports_{};
    /// Host params extension callbacks.
    clap_host_params host_params_{};
    /// Host MPE extension callbacks.
    clap_host_mpe host_mpe_{};

private:
    // ── DLL loading ───────────────────────────────────────────────

    /// Load the .clap shared library.
    bool load_library();

    /// Unload the .clap shared library.
    void unload_library();

    // ── Host callbacks ────────────────────────────────────────────

    /// Initialise the clap_host struct with our callbacks.
    void init_host();

    /// Initialise host extension structs.
    void init_extensions();

    // ── Members ───────────────────────────────────────────────────

    std::string plugin_path_;      ///< Filesystem path to the .clap binary
    PluginID id_;                  ///< Unique plugin identifier
    std::string name_;             ///< Plugin display name (from descriptor)
    std::string vendor_;           ///< Plugin vendor (from descriptor)
    PluginCategory category_ = PluginCategory::Effect;

    // Library handle
    void* library_handle_ = nullptr;

    // CLAP plugin entry and factory
    const clap_plugin_entry* entry_ = nullptr;
    const clap_plugin* plugin_ = nullptr;

    // CLAP host structures
    clap_host host_;
    bool host_initialized_ = false;

    // State tracking
    double sample_rate_ = 0.0;
    uint32_t min_frames_ = 0;
    uint32_t max_frames_ = 0;
    bool activated_ = false;

    // Cached extension pointers
    const clap_plugin_audio_ports* plugin_audio_ports_ = nullptr;
    const clap_plugin_note_ports* plugin_note_ports_ = nullptr;
    const clap_plugin_params* plugin_params_ = nullptr;
    const clap_plugin_latency* plugin_latency_ = nullptr;
    const clap_plugin_tail* plugin_tail_ = nullptr;
    const clap_plugin_state* plugin_state_ = nullptr;
    const clap_plugin_gui* plugin_gui_ = nullptr;

    /// Mutex protecting process() setup and teardown.
    std::mutex process_mutex_;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_CLAP_HOST_H