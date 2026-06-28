#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "plugin/plugin_factory.h"
#include "plugin/native_clap_factory.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

using namespace aria::plugin;

// ══════════════════════════════════════════════════════════════════════
//  Registration
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("NativeCLAPFactory registers all built-in plugins",
          "[plugin][native_clap]")
{
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    CHECK(factory.has("aria.eq"));
    CHECK(factory.has("aria.compressor"));
    CHECK(factory.has("aria.reverb"));
    CHECK(factory.has("aria.delay"));
    CHECK(factory.has("aria.synth"));
}

// ══════════════════════════════════════════════════════════════════════
//  Load and verify built-in EQ
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Built-in EQ loads as AudioPlugin", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.eq");
    REQUIRE(plugin != nullptr);

    CHECK(plugin->id() == "aria.eq");
    CHECK(plugin->name() == "EQ");
    CHECK(plugin->vendor() == "ARIA DSP");
    CHECK(plugin->format() == "Native");
    CHECK(plugin->category() == PluginCategory::Effect);
}

TEST_CASE("Built-in EQ lifecycle completes", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.eq");
    REQUIRE(plugin != nullptr);

    // init()
    CHECK(plugin->init());

    // activate()
    CHECK(plugin->activate(48000.0, 1, 512));

    // process()
    ProcessContext ctx;
    ctx.sample_rate = 48000.0;
    ctx.num_frames = 64;
    ctx.num_inputs = 2;
    ctx.num_outputs = 2;

    float input[2][64];
    float output[2][64] = {{0.0f}};
    std::fill(std::begin(input[0]), std::end(input[0]), 0.5f);
    std::fill(std::begin(input[1]), std::end(input[1]), 0.3f);
    const float* inputs[] = {input[0], input[1]};
    float* outputs[] = {output[0], output[1]};

    plugin->process(ctx, inputs, outputs);

    // Currently passes through (DSP is placeholder)
    for (uint32_t i = 0; i < 64; ++i) {
        CHECK(output[0][i] == 0.5f);
        CHECK(output[1][i] == 0.3f);
    }

    // deactivate()
    plugin->deactivate();
}

TEST_CASE("Built-in EQ has 8 parameters", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.eq");
    REQUIRE(plugin != nullptr);
    plugin->init();

    CHECK(plugin->parameter_count() == 8);
}

// ══════════════════════════════════════════════════════════════════════
//  Load and verify built-in Compressor
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Built-in compressor loads as AudioPlugin", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.compressor");
    REQUIRE(plugin != nullptr);

    CHECK(plugin->id() == "aria.compressor");
    CHECK(plugin->name() == "Compressor");
    CHECK(plugin->vendor() == "ARIA DSP");
    CHECK(plugin->format() == "Native");
    CHECK(plugin->category() == PluginCategory::Effect);
}

TEST_CASE("Built-in compressor has 6 parameters", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.compressor");
    REQUIRE(plugin != nullptr);
    plugin->init();

    CHECK(plugin->parameter_count() == 6);

    // Verify specific parameter
    auto threshold_info = plugin->parameter_info(0);
    CHECK(threshold_info.name == "Threshold");
    CHECK(threshold_info.min_value == -60.0);
    CHECK(threshold_info.max_value == 0.0);
}

TEST_CASE("Built-in compressor processes audio", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.compressor");
    REQUIRE(plugin != nullptr);
    plugin->init();
    plugin->activate(48000.0, 1, 512);

    ProcessContext ctx;
    ctx.sample_rate = 48000.0;
    ctx.num_frames = 32;
    ctx.num_inputs = 1;
    ctx.num_outputs = 1;

    float input[32];
    float output[32] = {0.0f};
    std::fill(std::begin(input), std::end(input), 0.75f);
    const float* inputs[] = {input};
    float* outputs[] = {output};

    plugin->process(ctx, inputs, outputs);

    // Currently passes through
    for (uint32_t i = 0; i < 32; ++i) {
        CHECK(output[i] == 0.75f);
    }

    plugin->deactivate();
}

// ══════════════════════════════════════════════════════════════════════
//  Load and verify built-in Reverb
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Built-in reverb loads as AudioPlugin", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.reverb");
    REQUIRE(plugin != nullptr);

    CHECK(plugin->id() == "aria.reverb");
    CHECK(plugin->name() == "Reverb");
    CHECK(plugin->category() == PluginCategory::Effect);
}

TEST_CASE("Built-in reverb has 5 parameters", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.reverb");
    REQUIRE(plugin != nullptr);
    plugin->init();
    CHECK(plugin->parameter_count() == 5);
}

// ══════════════════════════════════════════════════════════════════════
//  Load and verify built-in Delay
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Built-in delay loads as AudioPlugin", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.delay");
    REQUIRE(plugin != nullptr);

    CHECK(plugin->id() == "aria.delay");
    CHECK(plugin->name() == "Delay");
    CHECK(plugin->category() == PluginCategory::Effect);
}

TEST_CASE("Built-in delay has 4 parameters", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.delay");
    REQUIRE(plugin != nullptr);
    plugin->init();
    CHECK(plugin->parameter_count() == 4);
}

// ══════════════════════════════════════════════════════════════════════
//  Load and verify built-in Synth
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Built-in synth loads as AudioPlugin", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.synth");
    REQUIRE(plugin != nullptr);

    CHECK(plugin->id() == "aria.synth");
    CHECK(plugin->name() == "Synth");
    CHECK(plugin->category() == PluginCategory::Synth);
}

TEST_CASE("Built-in synth has 8 parameters", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.synth");
    REQUIRE(plugin != nullptr);
    plugin->init();
    CHECK(plugin->parameter_count() == 8);
}

TEST_CASE("Built-in synth produces silence (no inputs)", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto plugin = factory.create("aria.synth");
    REQUIRE(plugin != nullptr);
    plugin->init();
    plugin->activate(48000.0, 1, 512);

    ProcessContext ctx;
    ctx.sample_rate = 48000.0;
    ctx.num_frames = 64;
    ctx.num_inputs = 0;
    ctx.num_outputs = 2;

    float output[2][64] = {{0.0f}};
    float* outputs[] = {output[0], output[1]};

    plugin->process(ctx, nullptr, outputs);

    // Synth currently generates silence
    for (uint32_t i = 0; i < 64; ++i) {
        CHECK(output[0][i] == 0.0f);
        CHECK(output[1][i] == 0.0f);
    }

    plugin->deactivate();
}

// ══════════════════════════════════════════════════════════════════════
//  Parameter values round-trip
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Built-in plugins support state round-trip", "[plugin][native_clap]") {
    auto& factory = PluginFactory::instance();
    register_native_plugins(factory);

    auto original = factory.create("aria.compressor");
    REQUIRE(original != nullptr);
    original->init();

    // Change some parameter values
    original->set_parameter_value(1, 8.0);  // ratio
    original->set_parameter_value(4, 6.0);  // makeup

    // Save state
    std::vector<uint8_t> state;
    CHECK(original->save_state(state));
    CHECK_FALSE(state.empty());

    // Create new instance and load state
    auto restored = factory.create("aria.compressor");
    REQUIRE(restored != nullptr);
    restored->init();
    CHECK(restored->load_state(state));

    // Verify parameter values match
    CHECK(restored->get_parameter_value(1) == 8.0);
    CHECK(restored->get_parameter_value(4) == 6.0);
}
