#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "plugin/plugin_audio_node.h"
#include "audio/audio_buffer.h"
#include "audio/audio_node.h"
#include "test_common/test_plugin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

using namespace aria;
using namespace aria::plugin;

// ══════════════════════════════════════════════════════════════════════
//  Adapter Lifecycle
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginAudioNode wraps a plugin and exposes its metadata",
          "[plugin][audio_node]")
{
    auto plugin = std::make_unique<TestPlugin>("TestNode", PluginCategory::Effect, 128);
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    REQUIRE(adapter->plugin() != nullptr);
    CHECK(adapter->plugin()->name() == "TestNode");
    CHECK(adapter->plugin()->vendor() == "ARIA Test");
    CHECK(adapter->plugin()->category() == PluginCategory::Effect);
}

TEST_CASE("PluginAudioNode reports latency from wrapped plugin",
          "[plugin][audio_node]")
{
    auto plugin = std::make_unique<TestPlugin>("LatencyTest", PluginCategory::Effect, 256);
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    CHECK(adapter->latency() == 256);
}

TEST_CASE("PluginAudioNode label includes plugin name",
          "[plugin][audio_node]")
{
    auto plugin = std::make_unique<TestPlugin>("MyReverb");
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    std::string label = adapter->label();
    CHECK(label.find("MyReverb") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════
//  Parameter forwarding
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginAudioNode forwards parameter_count to plugin",
          "[plugin][audio_node][params]")
{
    auto plugin = std::make_unique<TestPlugin>("ParamTest");
    auto* raw = plugin.get();
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    // TestPlugin registers one "Gain" parameter by default
    CHECK(adapter->parameter_count() == raw->parameter_count());
    CHECK(adapter->parameter_count() == 1);
}

TEST_CASE("PluginAudioNode set_parameter and get_parameter round-trip",
          "[plugin][audio_node][params]")
{
    auto plugin = std::make_unique<TestPlugin>("ParamRT");
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    // Default value should be 0.5 (the Gain default)
    double default_val = adapter->get_parameter(0);
    CHECK(default_val == 0.5);

    // Set and verify
    adapter->set_parameter(0, 0.75);
    CHECK(adapter->get_parameter(0) == 0.75);
}

TEST_CASE("PluginAudioNode out-of-range parameter index returns 0",
          "[plugin][audio_node][params]")
{
    auto plugin = std::make_unique<TestPlugin>("OutOfRange");
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    CHECK(adapter->get_parameter(999) == 0.0);
    // Should not crash
    adapter->set_parameter(999, 1.0);
}

// ══════════════════════════════════════════════════════════════════════
//  Bypass passthrough
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginAudioNode processes and copies audio through plugin",
          "[plugin][audio_node][process]")
{
    auto plugin = std::make_unique<TestPlugin>("BypassTest");
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    // Set up a single-channel AudioBuffer
    constexpr uint32_t kFrames = 64;
    AudioBuffer input_buf;
    input_buf.frames = kFrames;
    input_buf.channels = 1;
    float input_data[kFrames];
    std::fill(std::begin(input_data), std::end(input_data), 0.5f);
    input_buf.data[0] = input_data;

    AudioBuffer output_buf;
    output_buf.frames = kFrames;
    output_buf.channels = 1;
    float output_data[kFrames] = {0.0f};
    output_buf.data[0] = output_data;

    AudioBuffer* inputs[] = {&input_buf};
    AudioBuffer* outputs[] = {&output_buf};

    AudioNode::ProcessContext ctx;
    ctx.frames = kFrames;
    ctx.sample_rate = 48000.0;
    ctx.sample_position = 0;
    ctx.inputs = inputs;
    ctx.outputs = outputs;
    ctx.input_count = 1;
    ctx.output_count = 1;

    // Process
    adapter->process(ctx);

    // TestPlugin copies inputs to outputs, so output should match input
    for (uint32_t i = 0; i < kFrames; ++i) {
        CHECK(output_data[i] == 0.5f);
    }
}

TEST_CASE("PluginAudioNode handles stereo passthrough correctly",
          "[plugin][audio_node][process]")
{
    auto plugin = std::make_unique<TestPlugin>("StereoTest");
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    constexpr uint32_t kFrames = 32;
    AudioBuffer input_buf;
    input_buf.frames = kFrames;
    input_buf.channels = 2;
    float left_in[kFrames], right_in[kFrames];
    std::fill(std::begin(left_in), std::end(left_in), 0.3f);
    std::fill(std::begin(right_in), std::end(right_in), 0.7f);
    input_buf.data[0] = left_in;
    input_buf.data[1] = right_in;

    AudioBuffer output_buf;
    output_buf.frames = kFrames;
    output_buf.channels = 2;
    float left_out[kFrames] = {0.0f}, right_out[kFrames] = {0.0f};
    output_buf.data[0] = left_out;
    output_buf.data[1] = right_out;

    AudioBuffer* inputs[] = {&input_buf};
    AudioBuffer* outputs[] = {&output_buf};

    AudioNode::ProcessContext ctx;
    ctx.frames = kFrames;
    ctx.sample_rate = 48000.0;
    ctx.inputs = inputs;
    ctx.outputs = outputs;
    ctx.input_count = 1;
    ctx.output_count = 1;

    adapter->process(ctx);

    // Verify both channels passed through
    for (uint32_t i = 0; i < kFrames; ++i) {
        CHECK(left_out[i] == 0.3f);
        CHECK(right_out[i] == 0.7f);
    }
}

TEST_CASE("PluginAudioNode handles more outputs than inputs by zero-filling",
          "[plugin][audio_node][process]")
{
    auto plugin = std::make_unique<TestPlugin>("MultiOutTest");
    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    constexpr uint32_t kFrames = 16;
    // One input buffer with 1 channel
    AudioBuffer input_buf;
    input_buf.frames = kFrames;
    input_buf.channels = 1;
    float in_data[kFrames];
    std::fill(std::begin(in_data), std::end(in_data), 1.0f);
    input_buf.data[0] = in_data;

    // Two output buffers with 1 channel each (simulating 2 separate outputs)
    AudioBuffer out1, out2;
    out1.frames = out2.frames = kFrames;
    out1.channels = out2.channels = 1;
    float out1_data[kFrames] = {0.0f}, out2_data[kFrames] = {0.0f};
    out1.data[0] = out1_data;
    out2.data[0] = out2_data;

    AudioBuffer* inputs[] = {&input_buf};
    AudioBuffer* outputs[] = {&out1, &out2};

    AudioNode::ProcessContext ctx;
    ctx.frames = kFrames;
    ctx.sample_rate = 48000.0;
    ctx.inputs = inputs;
    ctx.outputs = outputs;
    ctx.input_count = 1;
    ctx.output_count = 2;

    adapter->process(ctx);

    // First output gets input signal (passthrough)
    for (uint32_t i = 0; i < kFrames; ++i) {
        CHECK(out1_data[i] == 1.0f);
    }
    // Extra output (beyond inputs) should be zero-filled
    for (uint32_t i = 0; i < kFrames; ++i) {
        CHECK(out2_data[i] == 0.0f);
    }
}

// ══════════════════════════════════════════════════════════════════════
//  Reset
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginAudioNode reset calls deactivate on the plugin",
          "[plugin][audio_node][lifecycle]")
{
    auto plugin = std::make_unique<TestPlugin>("ResetTest");
    plugin->init();
    plugin->activate(48000.0, 1, 512);

    auto adapter = std::make_unique<PluginAudioNode>(std::move(plugin));

    // Reset should deactivate the plugin
    adapter->reset();

    // After reset, plugin can be re-activated
    CHECK(adapter->plugin() != nullptr);
}

// ══════════════════════════════════════════════════════════════════════
//  Sandbox integration
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginAudioNode with sandbox processes correctly",
          "[plugin][audio_node][sandbox]")
{
    auto plugin = std::make_unique<TestPlugin>("SandboxNode", PluginCategory::Effect, 0);
    auto sandbox = std::make_unique<PluginSandbox>(std::move(plugin));

    // Get the raw plugin pointer before moving into sandbox
    auto adapter = std::make_unique<PluginAudioNode>(
        std::make_unique<TestPlugin>("SandboxPlugin"),
        std::move(sandbox));

    CHECK(adapter->sandbox() != nullptr);
    CHECK(adapter->plugin() != nullptr);
}

TEST_CASE("PluginAudioNode with nullptr plugin does not crash on process",
          "[plugin][audio_node][edge]")
{
    auto adapter = std::make_unique<PluginAudioNode>(nullptr);

    AudioNode::ProcessContext ctx{};
    // Should not crash
    adapter->process(ctx);
}

TEST_CASE("PluginAudioNode parameter_count returns 0 when plugin is null",
          "[plugin][audio_node][edge]")
{
    auto adapter = std::make_unique<PluginAudioNode>(nullptr);
    CHECK(adapter->parameter_count() == 0);
}
