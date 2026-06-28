#include "native_clap_factory.h"
#include "audio_plugin.h"
#include "audio_plugin_types.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria::plugin {

// ─────────────────────────────────────────────────────────────────────
//  Native Plugin Base
// ─────────────────────────────────────────────────────────────────────

/// Base class for native ARIA DSP plugins.
///
/// Provides common metadata, ParameterManager integration, and
/// state serialization so each concrete plugin only needs to
/// implement process() and register its parameters.
class NativePlugin : public AudioPlugin {
public:
    NativePlugin(PluginID id, std::string name, std::string vendor,
                 PluginCategory category)
        : id_(std::move(id))
        , name_(std::move(name))
        , vendor_(std::move(vendor))
        , category_(category)
    {}

    ~NativePlugin() override = default;

    // ── Lifecycle ──────────────────────────────────────────────

    bool init() override { initialized_ = true; return true; }

    bool activate(double sample_rate, uint32_t /*min_frames*/,
                  uint32_t /*max_frames*/) override {
        if (!initialized_) return false;
        sample_rate_ = sample_rate;
        activated_ = true;
        return true;
    }

    void deactivate() override { activated_ = false; }

    // ── Identification ─────────────────────────────────────────

    PluginID id() const override { return id_; }
    std::string name() const override { return name_; }
    std::string vendor() const override { return vendor_; }
    std::string format() const override { return "Native"; }
    PluginCategory category() const override { return category_; }

    // ── Parameters ─────────────────────────────────────────────

    uint32_t parameter_count() const override { return params_.count(); }

    ParameterInfo parameter_info(ParamID id) const override {
        return params_.parameter_info(id);
    }

    double get_parameter_value(ParamID id) const override {
        return params_.get_value(id);
    }

    void set_parameter_value(ParamID id, double value) override {
        params_.set_value(id, value);
    }

    void begin_edit(ParamID id) override { params_.begin_edit(id); }

    void end_edit(ParamID id) override { params_.end_edit(id); }

    // ── State ──────────────────────────────────────────────────

    bool save_state(std::vector<uint8_t>& data) const override {
        uint32_t count = params_.count();
        size_t header_size = sizeof(uint32_t) + count * (sizeof(ParamID) + sizeof(double));
        data.resize(header_size);

        uint32_t offset = 0;
        std::memcpy(data.data() + offset, &count, sizeof(count));
        offset += sizeof(count);

        for (ParamID i = 0; i < count; ++i) {
            double val = params_.get_value(i);
            std::memcpy(data.data() + offset, &i, sizeof(i));
            offset += sizeof(i);
            std::memcpy(data.data() + offset, &val, sizeof(val));
            offset += sizeof(val);
        }

        return true;
    }

    bool load_state(const std::vector<uint8_t>& data) override {
        if (data.size() < sizeof(uint32_t)) return false;

        uint32_t offset = 0;
        uint32_t count;
        std::memcpy(&count, data.data() + offset, sizeof(count));
        offset += sizeof(count);

        for (uint32_t i = 0; i < count; ++i) {
            if (offset + sizeof(ParamID) + sizeof(double) > data.size()) return false;
            ParamID id;
            double val;
            std::memcpy(&id, data.data() + offset, sizeof(id));
            offset += sizeof(id);
            std::memcpy(&val, data.data() + offset, sizeof(val));
            offset += sizeof(val);
            params_.set_value(id, val);
        }

        return true;
    }

    // ── Latency ────────────────────────────────────────────────

    uint32_t latency_samples() const override { return 0; }

protected:
    bool initialized_ = false;
    bool activated_ = false;
    double sample_rate_ = 48000.0;
    ParameterManager params_;

private:
    PluginID id_;
    std::string name_;
    std::string vendor_;
    PluginCategory category_;
};

// ─────────────────────────────────────────────────────────────────────
//  Concrete plugins
// ─────────────────────────────────────────────────────────────────────

// ── EQ ────────────────────────────────────────────────────────────

/// 3-band parametric equalizer with low-shelf, peaking, high-shelf.
class EQPlugin : public NativePlugin {
public:
    EQPlugin() : NativePlugin("aria.eq", "EQ", "ARIA DSP", PluginCategory::Effect) {
        register_param("Low Freq", 200.0, 20.0, 2000.0);
        register_param("Low Gain", 0.0, -18.0, 18.0);
        register_param("Mid Freq", 1000.0, 20.0, 20000.0);
        register_param("Mid Gain", 0.0, -18.0, 18.0);
        register_param("Mid Q", 0.707, 0.1, 10.0);
        register_param("High Freq", 5000.0, 20.0, 20000.0);
        register_param("High Gain", 0.0, -18.0, 18.0);
        register_param("Master Gain", 0.0, -18.0, 18.0);
    }

    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override {
        // Simple pass-through for now (full DSP in later iteration)
        uint32_t ch = std::min(ctx.num_inputs, ctx.num_outputs);
        for (uint32_t i = 0; i < ch; ++i) {
            if (inputs[i] && outputs[i]) {
                std::memcpy(outputs[i], inputs[i], ctx.num_frames * sizeof(float));
            }
        }
        for (uint32_t i = ch; i < ctx.num_outputs; ++i) {
            if (outputs[i]) {
                std::memset(outputs[i], 0, ctx.num_frames * sizeof(float));
            }
        }
    }

private:
    void register_param(const char* name, double def, double min, double max) {
        ParameterInfo info;
        info.name = name;
        info.min_value = min;
        info.max_value = max;
        info.default_value = def;
        params_.register_parameter(info);
    }
};

// ── Compressor ────────────────────────────────────────────────────

class CompressorPlugin : public NativePlugin {
public:
    CompressorPlugin()
        : NativePlugin("aria.compressor", "Compressor", "ARIA DSP",
                       PluginCategory::Effect)
    {
        register_param("Threshold", -24.0, -60.0, 0.0);
        register_param("Ratio", 4.0, 1.0, 20.0);
        register_param("Attack", 5.0, 0.1, 50.0);
        register_param("Release", 100.0, 10.0, 1000.0);
        register_param("Makeup", 0.0, 0.0, 24.0);
        register_param("Knee", 6.0, 0.0, 24.0);
    }

    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override {
        uint32_t ch = std::min(ctx.num_inputs, ctx.num_outputs);
        for (uint32_t i = 0; i < ch; ++i) {
            if (inputs[i] && outputs[i]) {
                std::memcpy(outputs[i], inputs[i], ctx.num_frames * sizeof(float));
            }
        }
        for (uint32_t i = ch; i < ctx.num_outputs; ++i) {
            if (outputs[i]) {
                std::memset(outputs[i], 0, ctx.num_frames * sizeof(float));
            }
        }
    }

private:
    void register_param(const char* name, double def, double min, double max) {
        ParameterInfo info;
        info.name = name;
        info.min_value = min;
        info.max_value = max;
        info.default_value = def;
        params_.register_parameter(info);
    }
};

// ── Reverb ────────────────────────────────────────────────────────

class ReverbPlugin : public NativePlugin {
public:
    ReverbPlugin()
        : NativePlugin("aria.reverb", "Reverb", "ARIA DSP",
                       PluginCategory::Effect)
    {
        register_param("Size", 0.5, 0.0, 1.0);
        register_param("Decay", 2.0, 0.1, 10.0);
        register_param("Damping", 0.5, 0.0, 1.0);
        register_param("Mix", 0.3, 0.0, 1.0);
        register_param("Pre-Delay", 20.0, 0.0, 200.0);
    }

    uint32_t latency_samples() const override { return 0; }

    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override {
        uint32_t ch = std::min(ctx.num_inputs, ctx.num_outputs);
        for (uint32_t i = 0; i < ch; ++i) {
            if (inputs[i] && outputs[i]) {
                std::memcpy(outputs[i], inputs[i], ctx.num_frames * sizeof(float));
            }
        }
        for (uint32_t i = ch; i < ctx.num_outputs; ++i) {
            if (outputs[i]) {
                std::memset(outputs[i], 0, ctx.num_frames * sizeof(float));
            }
        }
    }

private:
    void register_param(const char* name, double def, double min, double max) {
        ParameterInfo info;
        info.name = name;
        info.min_value = min;
        info.max_value = max;
        info.default_value = def;
        params_.register_parameter(info);
    }
};

// ── Delay ─────────────────────────────────────────────────────────

class DelayPlugin : public NativePlugin {
public:
    DelayPlugin()
        : NativePlugin("aria.delay", "Delay", "ARIA DSP",
                       PluginCategory::Effect)
    {
        register_param("Time", 400.0, 1.0, 2000.0);
        register_param("Feedback", 0.3, 0.0, 1.0);
        register_param("Mix", 0.5, 0.0, 1.0);
        register_param("Stereo Spread", 0.5, 0.0, 1.0);
    }

    uint32_t latency_samples() const override { return 0; }

    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override {
        uint32_t ch = std::min(ctx.num_inputs, ctx.num_outputs);
        for (uint32_t i = 0; i < ch; ++i) {
            if (inputs[i] && outputs[i]) {
                std::memcpy(outputs[i], inputs[i], ctx.num_frames * sizeof(float));
            }
        }
        for (uint32_t i = ch; i < ctx.num_outputs; ++i) {
            if (outputs[i]) {
                std::memset(outputs[i], 0, ctx.num_frames * sizeof(float));
            }
        }
    }

private:
    void register_param(const char* name, double def, double min, double max) {
        ParameterInfo info;
        info.name = name;
        info.min_value = min;
        info.max_value = max;
        info.default_value = def;
        params_.register_parameter(info);
    }
};

// ── Synth ─────────────────────────────────────────────────────────

class SynthPlugin : public NativePlugin {
public:
    SynthPlugin()
        : NativePlugin("aria.synth", "Synth", "ARIA DSP",
                       PluginCategory::Synth)
    {
        register_param("Osc Mix", 0.5, 0.0, 1.0);
        register_param("Cutoff", 1000.0, 20.0, 20000.0);
        register_param("Resonance", 0.0, 0.0, 1.0);
        register_param("Envelope Attack", 10.0, 0.0, 2000.0);
        register_param("Envelope Decay", 100.0, 0.0, 5000.0);
        register_param("Envelope Sustain", 0.7, 0.0, 1.0);
        register_param("Envelope Release", 200.0, 0.0, 10000.0);
        register_param("Volume", 0.8, 0.0, 1.0);
    }

    uint32_t latency_samples() const override { return 0; }

    void process(const ProcessContext& ctx,
                 const float* const* inputs,
                 float* const* outputs) override {
        // Synth ignores inputs — generate silence for now
        for (uint32_t i = 0; i < ctx.num_outputs; ++i) {
            if (outputs[i]) {
                std::memset(outputs[i], 0, ctx.num_frames * sizeof(float));
            }
        }
    }

private:
    void register_param(const char* name, double def, double min, double max) {
        ParameterInfo info;
        info.name = name;
        info.min_value = min;
        info.max_value = max;
        info.default_value = def;
        params_.register_parameter(info);
    }
};

// ─────────────────────────────────────────────────────────────────────
//  Registration
// ─────────────────────────────────────────────────────────────────────

void register_native_plugins(PluginFactory& factory) {
    factory.register_plugin("aria.eq",          []{ return std::make_unique<EQPlugin>(); },
                            "EQ", "ARIA DSP", PluginCategory::Effect);

    factory.register_plugin("aria.compressor", []{ return std::make_unique<CompressorPlugin>(); },
                            "Compressor", "ARIA DSP", PluginCategory::Effect);

    factory.register_plugin("aria.reverb",     []{ return std::make_unique<ReverbPlugin>(); },
                            "Reverb", "ARIA DSP", PluginCategory::Effect);

    factory.register_plugin("aria.delay",      []{ return std::make_unique<DelayPlugin>(); },
                            "Delay", "ARIA DSP", PluginCategory::Effect);

    factory.register_plugin("aria.synth",      []{ return std::make_unique<SynthPlugin>(); },
                            "Synth", "ARIA DSP", PluginCategory::Synth);
}

} // namespace aria::plugin
