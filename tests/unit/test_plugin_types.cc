#include <catch2/catch_test_macros.hpp>
#include "plugin/audio_plugin_types.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace aria::plugin;

// ─── ParamID tests ──────────────────────────────────────────

TEST_CASE("ParamID is uint32_t", "[plugin][types]") {
    ParamID id = 42;
    REQUIRE(id == 42);
    REQUIRE(sizeof(ParamID) == sizeof(uint32_t));
}

TEST_CASE("ParamID arithmetic", "[plugin][types]") {
    ParamID a = 100;
    ParamID b = 200;
    REQUIRE(b - a == 100);
    REQUIRE(a < b);
}

// ─── PluginID tests ─────────────────────────────────────────

TEST_CASE("PluginID behaves as string", "[plugin][types]") {
    PluginID id = "aria.eq";
    REQUIRE(id == "aria.eq");
    REQUIRE(id.length() == 7);
}

TEST_CASE("PluginID comparison", "[plugin][types]") {
    PluginID a = "test.plugin.a";
    PluginID b = "test.plugin.b";
    REQUIRE(a != b);
    REQUIRE(a < b);
}

TEST_CASE("PluginID empty is valid", "[plugin][types]") {
    PluginID id;
    REQUIRE(id.empty());
}

// ─── PluginCategory enum tests ──────────────────────────────

TEST_CASE("PluginCategory enum values", "[plugin][types]") {
    REQUIRE(static_cast<int>(PluginCategory::Synth) == 0);
    REQUIRE(static_cast<int>(PluginCategory::Effect) == 1);
    REQUIRE(static_cast<int>(PluginCategory::Analyzer) == 2);
    REQUIRE(static_cast<int>(PluginCategory::Utility) == 3);
    REQUIRE(static_cast<int>(PluginCategory::Instrument) == 4);
    REQUIRE(static_cast<int>(PluginCategory::Other) == 5);
}

TEST_CASE("PluginCategory can switch", "[plugin][types]") {
    auto cat = PluginCategory::Effect;
    switch (cat) {
        case PluginCategory::Effect: break;
        default: FAIL("switch failed for PluginCategory::Effect");
    }
}

// ─── ParameterInfo tests ────────────────────────────────────

TEST_CASE("ParameterInfo default construction", "[plugin][types]") {
    ParameterInfo info;
    REQUIRE(info.id == 0);
    REQUIRE(info.name.empty());
    REQUIRE(info.min_value == 0.0);
    REQUIRE(info.max_value == 1.0);
    REQUIRE(info.default_value == 0.0);
    REQUIRE(info.value_labels.empty());
}

TEST_CASE("ParameterInfo custom construction", "[plugin][types]") {
    ParameterInfo info;
    info.id = 1;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    REQUIRE(info.id == 1);
    REQUIRE(info.name == "Gain");
    REQUIRE(info.min_value == 0.0);
    REQUIRE(info.max_value == 1.0);
    REQUIRE(info.default_value == 0.5);
}

TEST_CASE("ParameterInfo with value labels", "[plugin][types]") {
    ParameterInfo info;
    info.id = 2;
    info.name = "Waveform";
    info.min_value = 0.0;
    info.max_value = 2.0;
    info.default_value = 0.0;
    info.value_labels = {"Sine", "Saw", "Square"};

    REQUIRE(info.value_labels.size() == 3);
    REQUIRE(info.value_labels[0] == "Sine");
    REQUIRE(info.value_labels[1] == "Saw");
    REQUIRE(info.value_labels[2] == "Square");
}

TEST_CASE("ParameterInfo range clamping invariant", "[plugin][types]") {
    ParameterInfo info;
    info.min_value = -10.0;
    info.max_value = 10.0;
    info.default_value = 0.0;

    REQUIRE(info.min_value < info.max_value);
    REQUIRE(info.default_value >= info.min_value);
    REQUIRE(info.default_value <= info.max_value);
}

// ─── ProcessContext tests ────────────────────────────────────

TEST_CASE("ProcessContext default construction", "[plugin][types]") {
    ProcessContext ctx;
    REQUIRE(ctx.sample_rate == 48000.0);
    REQUIRE(ctx.num_frames == 0);
    REQUIRE(ctx.frame_offset == 0);
    REQUIRE(ctx.num_inputs == 0);
    REQUIRE(ctx.num_outputs == 0);
    REQUIRE_FALSE(ctx.is_bypassed);
    REQUIRE_FALSE(ctx.is_offline);
}

TEST_CASE("ProcessContext custom construction", "[plugin][types]") {
    ProcessContext ctx;
    ctx.sample_rate = 96000.0;
    ctx.num_frames = 256;
    ctx.frame_offset = 12345;
    ctx.num_inputs = 2;
    ctx.num_outputs = 2;
    ctx.is_bypassed = true;
    ctx.is_offline = false;

    REQUIRE(ctx.sample_rate == 96000.0);
    REQUIRE(ctx.num_frames == 256);
    REQUIRE(ctx.frame_offset == 12345);
    REQUIRE(ctx.num_inputs == 2);
    REQUIRE(ctx.num_outputs == 2);
    REQUIRE(ctx.is_bypassed);
    REQUIRE_FALSE(ctx.is_offline);
}

TEST_CASE("ProcessContext offline mode", "[plugin][types]") {
    ProcessContext ctx;
    ctx.is_offline = true;
    REQUIRE(ctx.is_offline);

    ctx.is_offline = false;
    REQUIRE_FALSE(ctx.is_offline);
}

TEST_CASE("ProcessContext mono config", "[plugin][types]") {
    ProcessContext ctx;
    ctx.sample_rate = 44100.0;
    ctx.num_frames = 512;
    ctx.num_inputs = 1;
    ctx.num_outputs = 1;

    REQUIRE(ctx.num_inputs == 1);
    REQUIRE(ctx.num_outputs == 1);
    REQUIRE(ctx.num_frames == 512);
}

// ─── Combined type tests ─────────────────────────────────────

TEST_CASE("Types compose together", "[plugin][types]") {
    // Verify that the types work together as the AudioPlugin API expects
    ProcessContext ctx;
    ctx.sample_rate = 48000.0;
    ctx.num_frames = 256;
    ctx.num_inputs = 2;
    ctx.num_outputs = 2;

    ParameterInfo info;
    info.id = 0;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    ParamID id = info.id;
    REQUIRE(id == 0);

    PluginCategory cat = PluginCategory::Effect;
    REQUIRE(cat == PluginCategory::Effect);
}
