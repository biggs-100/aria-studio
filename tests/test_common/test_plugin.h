#ifndef ARIA_TEST_PLUGIN_H
#define ARIA_TEST_PLUGIN_H

#include "plugin/audio_plugin.h"

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria::plugin {

/// Minimal AudioPlugin subclass for testing.
///
/// Provides configurable latency, simulated crashes, and adjustable
/// process duration for watchdog timeout testing. Parameters are
/// managed through the built-in ParameterManager.
class TestPlugin : public AudioPlugin {
public:
    explicit TestPlugin(std::string name = "TestPlugin",
                        PluginCategory category = PluginCategory::Effect,
                        uint32_t latency = 0);
    ~TestPlugin() override = default;

    // ── AudioPlugin interface ──────────────────────────────────

    bool init() override;
    bool activate(double sample_rate, uint32_t min_frames, uint32_t max_frames) override;
    void deactivate() override;

    PluginID id() const override;
    std::string name() const override;
    std::string vendor() const override;
    std::string format() const override;
    PluginCategory category() const override;

    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override;

    uint32_t parameter_count() const override;
    ParameterInfo parameter_info(ParamID id) const override;
    double get_parameter_value(ParamID id) const override;
    void set_parameter_value(ParamID id, double value) override;
    void begin_edit(ParamID id) override;
    void end_edit(ParamID id) override;

    bool save_state(std::vector<uint8_t>& data) const override;
    bool load_state(const std::vector<uint8_t>& data) override;

    uint32_t latency_samples() const override;

    // ── Test configuration ─────────────────────────────────────

    /// When true, process() will block for the configured duration
    /// (or default 10 s) to simulate a plugin hang.
    void set_simulate_hang(bool hang);

    /// When true, process() will throw an exception to simulate a crash.
    void set_simulate_crash(bool crash);

    /// Duration in milliseconds that process() sleeps (for watchdog testing).
    void set_process_duration_ms(uint32_t ms);

    /// Override latency reported by latency_samples().
    void set_latency(uint32_t samples);

private:
    // Metadata
    std::string name_;
    std::string vendor_ = "ARIA Test";
    PluginCategory category_;
    PluginID id_;
    uint32_t latency_ = 0;
    bool initialized_ = false;
    bool active_ = false;

    // Test simulation flags
    std::atomic<bool> simulate_hang_{false};
    std::atomic<bool> simulate_crash_{false};
    std::atomic<uint32_t> process_duration_ms_{0};

    // Parameter storage
    ParameterManager params_;
};

} // namespace aria::plugin

#endif // ARIA_TEST_PLUGIN_H
