#ifndef ARIA_PLUGIN_AUDIO_PLUGIN_H
#define ARIA_PLUGIN_AUDIO_PLUGIN_H

#include "audio_plugin_types.h"

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria::plugin {

/// Pure abstract interface all plugin implementations must satisfy.
///
/// Lifecycle: init() → activate() → process()* → deactivate()
/// The host MUST NOT call process() before activate() returns true.
class AudioPlugin {
public:
    virtual ~AudioPlugin() = default;

    // ── Lifecycle ──────────────────────────────────────────────

    /// Load the plugin binary / prepare resources. Returns false on failure.
    virtual bool init() = 0;

    /// Prepare for audio processing at the given sample rate and block range.
    /// @return true if the given configuration is accepted.
    virtual bool activate(double sample_rate, uint32_t min_frames, uint32_t max_frames) = 0;

    /// Release audio resources. The plugin may be re-activated later.
    virtual void deactivate() = 0;

    // ── Identification ─────────────────────────────────────────

    virtual PluginID id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string vendor() const = 0;
    virtual std::string format() const = 0;   // "CLAP", "VST3", "Native"
    virtual PluginCategory category() const = 0;

    // ── Audio Processing ───────────────────────────────────────

    /// Process one audio block.
    /// @param ctx     Processing context (sample rate, frame count, position, etc.)
    /// @param inputs  Per-channel input buffer array (channel-count pointers)
    /// @param outputs Per-channel output buffer array (channel-count pointers)
    virtual void process(const ProcessContext& ctx,
                         const float* const* inputs,
                         float* const* outputs) = 0;

    // ── Parameters ─────────────────────────────────────────────

    virtual uint32_t parameter_count() const = 0;
    virtual ParameterInfo parameter_info(ParamID id) const = 0;
    virtual double get_parameter_value(ParamID id) const = 0;
    virtual void set_parameter_value(ParamID id, double value) = 0;

    /// Mark the beginning of an automation edit (creates an undo boundary).
    virtual void begin_edit(ParamID id) = 0;

    /// Mark the end of an automation edit.
    virtual void end_edit(ParamID id) = 0;

    // ── State ──────────────────────────────────────────────────

    /// Serialize full plugin state into an opaque byte vector.
    virtual bool save_state(std::vector<uint8_t>& data) const = 0;

    /// Restore plugin state from an opaque byte vector produced by save_state().
    virtual bool load_state(const std::vector<uint8_t>& data) = 0;

    // ── Latency ────────────────────────────────────────────────

    /// Number of samples of latency introduced by this plugin.
    /// Returns 0 by default (no latency compensation needed).
    virtual uint32_t latency_samples() const { return 0; }
};

// ─────────────────────────────────────────────────────────────────
// ParameterManager — reusable helper for AudioPlugin implementations
// ─────────────────────────────────────────────────────────────────

/// Thread-safe parameter storage used by concrete AudioPlugin subclasses.
///
/// Registration (mutating the parameter map) is exclusive-locked.
/// get_parameter_info() is shared-locked.
/// get/set of individual values uses relaxed atomics for real-time safety.
class ParameterManager {
public:
    /// Register a new parameter. Returns the assigned ParamID.
    ParamID register_parameter(const ParameterInfo& info);

    /// Retrieve metadata for a registered parameter.
    ParameterInfo parameter_info(ParamID id) const;

    /// Get the current value of a parameter (real-time safe).
    double get_value(ParamID id) const;

    /// Set the current value of a parameter.
    void set_value(ParamID id, double value);

    /// Start an automation edit for the given parameter.
    void begin_edit(ParamID id);

    /// Apply a new value during an active edit.
    void perform_edit(ParamID id, double value);

    /// Finalise an automation edit.
    void end_edit(ParamID id);

    /// Total number of registered parameters.
    uint32_t count() const;

private:
    struct Parameter {
        ParameterInfo info;
        std::atomic<double> value{0.0};
        bool editing = false;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<ParamID, Parameter> params_;
    ParamID next_id_ = 0;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_AUDIO_PLUGIN_H
