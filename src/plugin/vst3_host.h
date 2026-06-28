#ifndef ARIA_PLUGIN_VST3_HOST_H
#define ARIA_PLUGIN_VST3_HOST_H

#include "audio_plugin.h"
#include "format_scanner.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

// ── VST3 SDK forward declarations ──────────────────────────────────
// These come from the VST3 SDK headers. In this build, we provide minimal
// stub headers under vendor/vst3sdk/. All VST3 code is guarded by
// ARIA_FEATURE_VST3 to allow building without the SDK.

namespace Steinberg {
class FUnknown;
class IBStream;
class IPluginFactory;

namespace Vst {
class IComponent;
class IEditController;
class IEditController2;
class IAudioProcessor;
class IUnitInfo;
class IParameterChanges;
struct BusInfo;
struct ProcessSetup;
struct ProcessData;
} // namespace Vst
} // namespace Steinberg

namespace aria::plugin {

// ── VST3Host ─────────────────────────────────────────────────────────

/// AudioPlugin implementation wrapping a VST3 plugin (.vst3 bundle).
///
/// Loads the VST3 module, creates the component and edit controller,
/// and bridges the AudioPlugin interface to the VST3 C++ API.
///
/// All VST3-specific code is compiled only when ARIA_FEATURE_VST3 is
/// defined. Without it, VST3Host is a stub that returns false on init().
class VST3Host : public AudioPlugin {
public:
    /// Construct a VST3Host for the plugin module at the given path.
    explicit VST3Host(std::string module_path, PluginID id);
    ~VST3Host() override;

    // ── AudioPlugin interface ─────────────────────────────────────

    bool init() override;
    bool activate(double sample_rate, uint32_t min_frames, uint32_t max_frames) override;
    void deactivate() override;

    PluginID id() const override;
    std::string name() const override;
    std::string vendor() const override;
    std::string format() const override;
    PluginCategory category() const override;
    uint32_t parameter_count() const override;
    ParameterInfo parameter_info(ParamID id) const override;
    double get_parameter_value(ParamID id) const override;
    void set_parameter_value(ParamID id, double value) override;
    void begin_edit(ParamID id) override;
    void end_edit(ParamID id) override;

    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override;

    bool save_state(std::vector<uint8_t>& data) const override;
    bool load_state(const std::vector<uint8_t>& data) override;

    uint32_t latency_samples() const override;

    /// Audio I/O configuration: number of input/output busses and channels.
    struct AudioIOConfig {
        uint32_t num_input_busses = 0;
        uint32_t num_output_busses = 0;
        uint32_t total_input_channels = 0;
        uint32_t total_output_channels = 0;
    };

    AudioIOConfig audio_io_config() const;

private:
    // ── Members (always compiled — getters need them even in VST3-off stubs) ──

    std::string module_path_;
    PluginID id_;
    std::string name_;
    std::string vendor_;
    PluginCategory category_ = PluginCategory::Effect;

#if defined(ARIA_FEATURE_VST3)
    // ── Module loading ───────────────────────────────────────────

    bool load_module();
    void unload_module();

    // ── Members ──────────────────────────────────────────────────

    // Module handle (platform-specific)
    void* module_handle_ = nullptr;

    // Factory
    Steinberg::IPluginFactory* factory_ = nullptr;

    // VST3 interfaces
    Steinberg::Vst::IComponent* component_ = nullptr;
    Steinberg::Vst::IEditController* controller_ = nullptr;
    Steinberg::Vst::IAudioProcessor* processor_ = nullptr;

    // State
    double sample_rate_ = 0.0;
    uint32_t min_frames_ = 0;
    uint32_t max_frames_ = 0;
    bool activated_ = false;

    // Bus configuration
    AudioIOConfig io_config_;

    std::mutex process_mutex_;

    // VST3-specific internal buffer management
    std::vector<float*> input_buffers_;
    std::vector<float*> output_buffers_;
#endif // ARIA_FEATURE_VST3
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_VST3_HOST_H