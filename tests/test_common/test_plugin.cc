#include "test_plugin.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace aria::plugin {

TestPlugin::TestPlugin(std::string name, PluginCategory category, uint32_t latency)
    : name_(std::move(name))
    , category_(category)
    , latency_(latency)
{
    id_ = "test." + name_;
    std::replace(id_.begin(), id_.end(), ' ', '_');

    // Register a single test parameter: gain 0..1, default 0.5
    ParameterInfo gain;
    gain.name = "Gain";
    gain.min_value = 0.0;
    gain.max_value = 1.0;
    gain.default_value = 0.5;
    params_.register_parameter(gain);
}

// ── Lifecycle ──────────────────────────────────────────────────

bool TestPlugin::init() {
    if (simulate_crash_.load(std::memory_order_relaxed)) {
        return false;
    }
    initialized_ = true;
    return true;
}

bool TestPlugin::activate(double /*sample_rate*/,
                          uint32_t /*min_frames*/,
                          uint32_t /*max_frames*/) {
    if (!initialized_) return false;
    if (simulate_crash_.load(std::memory_order_relaxed)) {
        return false;
    }
    active_ = true;
    return true;
}

void TestPlugin::deactivate() {
    active_ = false;
}

// ── Identification ─────────────────────────────────────────────

PluginID TestPlugin::id() const { return id_; }
std::string TestPlugin::name() const { return name_; }
std::string TestPlugin::vendor() const { return vendor_; }
std::string TestPlugin::format() const { return "Native"; }
PluginCategory TestPlugin::category() const { return category_; }

// ── Processing ─────────────────────────────────────────────────

void TestPlugin::process(const ProcessContext& ctx,
                         const float* const* inputs,
                         float* const* outputs) {
    if (simulate_crash_.load(std::memory_order_relaxed)) {
        throw std::runtime_error("TestPlugin simulated crash");
    }

    uint32_t duration_ms = process_duration_ms_.load(std::memory_order_relaxed);
    if (duration_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    }

    if (simulate_hang_.load(std::memory_order_relaxed)) {
        // Block indefinitely (watchdog should terminate)
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    // Default: bypass — copy inputs to outputs
    uint32_t num_channels = std::min(ctx.num_inputs, ctx.num_outputs);
    for (uint32_t ch = 0; ch < num_channels; ++ch) {
        if (inputs[ch] && outputs[ch]) {
            std::memcpy(outputs[ch], inputs[ch], ctx.num_frames * sizeof(float));
        }
    }

    // Zero remaining output channels
    for (uint32_t ch = num_channels; ch < ctx.num_outputs; ++ch) {
        if (outputs[ch]) {
            std::memset(outputs[ch], 0, ctx.num_frames * sizeof(float));
        }
    }
}

// ── Parameters ─────────────────────────────────────────────────

uint32_t TestPlugin::parameter_count() const { return params_.count(); }

ParameterInfo TestPlugin::parameter_info(ParamID id) const {
    return params_.parameter_info(id);
}

double TestPlugin::get_parameter_value(ParamID id) const {
    return params_.get_value(id);
}

void TestPlugin::set_parameter_value(ParamID id, double value) {
    params_.set_value(id, value);
}

void TestPlugin::begin_edit(ParamID id) { params_.begin_edit(id); }
void TestPlugin::end_edit(ParamID id) { params_.end_edit(id); }

// ── State ──────────────────────────────────────────────────────

bool TestPlugin::save_state(std::vector<uint8_t>& data) const {
    // Simple state: parameter count + (id, value) pairs
    uint32_t count = params_.count();
    size_t header_size = sizeof(uint32_t) + count * (sizeof(ParamID) + sizeof(double));
    data.resize(header_size);

    uint32_t offset = 0;
    std::memcpy(data.data() + offset, &count, sizeof(count));
    offset += sizeof(count);

    // Write each parameter
    for (ParamID i = 0; i < count; ++i) {
        double val = params_.get_value(i);
        std::memcpy(data.data() + offset, &i, sizeof(i));
        offset += sizeof(i);
        std::memcpy(data.data() + offset, &val, sizeof(val));
        offset += sizeof(val);
    }

    return true;
}

bool TestPlugin::load_state(const std::vector<uint8_t>& data) {
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

// ── Latency ────────────────────────────────────────────────────

uint32_t TestPlugin::latency_samples() const { return latency_; }

// ── Test configuration ─────────────────────────────────────────

void TestPlugin::set_simulate_hang(bool hang) {
    simulate_hang_.store(hang, std::memory_order_relaxed);
}

void TestPlugin::set_simulate_crash(bool crash) {
    simulate_crash_.store(crash, std::memory_order_relaxed);
}

void TestPlugin::set_process_duration_ms(uint32_t ms) {
    process_duration_ms_.store(ms, std::memory_order_relaxed);
}

void TestPlugin::set_latency(uint32_t samples) {
    latency_ = samples;
}

} // namespace aria::plugin
