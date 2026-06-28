#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "plugin/plugin_host.h"
#include "plugin/native_clap_factory.h"
#include "test_common/test_plugin.h"

#include <chrono>
#include <memory>
#include <vector>

using namespace aria;
using namespace aria::plugin;
using namespace std::chrono_literals;

// ══════════════════════════════════════════════════════════════════════
//  Lifecycle
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginHost initializes and shuts down", "[plugin][host]") {
    PluginHost host;
    CHECK(host.init());
    host.shutdown();
}

TEST_CASE("PluginHost double init returns true", "[plugin][host]") {
    PluginHost host;
    CHECK(host.init());
    CHECK(host.init());  // second init should succeed
    host.shutdown();
}

TEST_CASE("PluginHost shutdown is idempotent", "[plugin][host]") {
    PluginHost host;
    host.init();
    host.shutdown();
    // Second shutdown should not crash
    host.shutdown();
}

// ══════════════════════════════════════════════════════════════════════
//  Sub-component access
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginHost provides factory access after init", "[plugin][host]") {
    PluginHost host;
    host.init();

    auto& factory = host.factory();
    CHECK(factory.count() >= 0);

    host.shutdown();
}

TEST_CASE("PluginHost provides scanner access after init", "[plugin][host]") {
    PluginHost host;
    host.init();

    auto& scanner = host.scanner();
    CHECK(&scanner != nullptr);

    host.shutdown();
}

TEST_CASE("PluginHost provides blacklist access after init", "[plugin][host]") {
    PluginHost host;
    host.init();

    auto& bl = host.blacklist();
    CHECK(bl.entries().empty());

    host.shutdown();
}

TEST_CASE("PluginHost factory access throws before init", "[plugin][host]") {
    PluginHost host;
    CHECK_THROWS_AS(host.factory(), std::logic_error);
}

TEST_CASE("PluginHost scanner access throws before init", "[plugin][host]") {
    PluginHost host;
    CHECK_THROWS_AS(host.scanner(), std::logic_error);
}

TEST_CASE("PluginHost blacklist access throws before init", "[plugin][host]") {
    PluginHost host;
    CHECK_THROWS_AS(host.blacklist(), std::logic_error);
}

// ══════════════════════════════════════════════════════════════════════
//  Native plugin registration
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginHost native plugins are registered on init",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    // Native plugins should be registered in the factory singleton
    auto& factory = PluginFactory::instance();
    CHECK(factory.has("aria.eq"));
    CHECK(factory.has("aria.compressor"));
    CHECK(factory.has("aria.reverb"));
    CHECK(factory.has("aria.delay"));
    CHECK(factory.has("aria.synth"));

    host.shutdown();
}

TEST_CASE("PluginHost plugin_count matches factory count", "[plugin][host]") {
    PluginHost host;
    host.init();

    auto& factory = PluginFactory::instance();
    CHECK(host.plugin_count() == factory.count());

    host.shutdown();
}

// ══════════════════════════════════════════════════════════════════════
//  Create / Destroy plugin instances
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginHost creates a plugin instance by native ID",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    // Register a test plugin in the factory
    auto& factory = PluginFactory::instance();
    factory.register_plugin("test.host_create",
        []{ return std::make_unique<TestPlugin>("HostCreate", PluginCategory::Effect); },
        "HostCreate", "TestVendor", PluginCategory::Effect);

    auto* sandbox = host.create_plugin_instance("test.host_create");
    REQUIRE(sandbox != nullptr);
    CHECK(sandbox->state() == aria::plugin::SandboxState::Idle);
    CHECK(sandbox->plugin() != nullptr);

    host.shutdown();
}

TEST_CASE("PluginHost create_plugin_instance returns nullptr for unknown ID",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    auto* sandbox = host.create_plugin_instance("nonexistent.id");
    CHECK(sandbox == nullptr);

    host.shutdown();
}

TEST_CASE("PluginHost destroy_plugin_instance stops and removes sandbox",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    auto& factory = PluginFactory::instance();
    factory.register_plugin("test.host_destroy",
        []{ return std::make_unique<TestPlugin>("HostDestroy", PluginCategory::Effect); },
        "HostDestroy", "TestVendor", PluginCategory::Effect);

    auto* sandbox = host.create_plugin_instance("test.host_destroy");
    REQUIRE(sandbox != nullptr);

    bool destroyed = host.destroy_plugin_instance(sandbox);
    CHECK(destroyed);

    // Second destroy should fail (already removed)
    CHECK_FALSE(host.destroy_plugin_instance(sandbox));

    host.shutdown();
}

TEST_CASE("PluginHost destroy_plugin_instance with nullptr returns false",
          "[plugin][host]")
{
    PluginHost host;
    host.init();
    CHECK_FALSE(host.destroy_plugin_instance(nullptr));
    host.shutdown();
}

// ══════════════════════════════════════════════════════════════════════
//  Process plugin
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginHost processes a plugin instance", "[plugin][host]") {
    PluginHost host;
    host.init();

    auto& factory = PluginFactory::instance();
    factory.register_plugin("test.host_process",
        []{ return std::make_unique<TestPlugin>("HostProcess", PluginCategory::Effect); },
        "HostProcess", "TestVendor", PluginCategory::Effect);

    auto* sandbox = host.create_plugin_instance("test.host_process");
    REQUIRE(sandbox != nullptr);

    // Set up process context
    ProcessContext ctx;
    ctx.num_inputs = 1;
    ctx.num_outputs = 1;
    ctx.num_frames = 32;
    ctx.sample_rate = 48000.0;

    float input[32];
    std::fill(std::begin(input), std::end(input), 0.5f);
    float output[32] = {0.0f};
    const float* inputs[] = {input};
    float* outputs[] = {output};

    bool result = host.process_plugin(sandbox, ctx, inputs, outputs, 500ms);
    CHECK(result);

    // Output should match input (TestPlugin bypasses)
    for (uint32_t i = 0; i < 32; ++i) {
        CHECK(output[i] == 0.5f);
    }

    host.shutdown();
}

TEST_CASE("PluginHost process_plugin with nullptr returns false",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    ProcessContext ctx;
    float input;
    float output;
    const float* inputs[] = {&input};
    float* outputs[] = {&output};

    bool result = host.process_plugin(nullptr, ctx, inputs, outputs, 100ms);
    CHECK_FALSE(result);

    host.shutdown();
}

// ══════════════════════════════════════════════════════════════════════
//  Blacklist integration
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginHost blocks blacklisted plugins from creating instances",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    auto& factory = PluginFactory::instance();
    factory.register_plugin("test.blacklisted",
        []{ return std::make_unique<TestPlugin>("Blacklisted", PluginCategory::Effect); },
        "Blacklisted", "TestVendor", PluginCategory::Effect);

    // Blacklist the plugin
    host.blacklist().report_crash("test.blacklisted");
    host.blacklist().report_crash("test.blacklisted");
    host.blacklist().report_crash("test.blacklisted");
    CHECK(host.blacklist().is_blacklisted("test.blacklisted"));

    // Creation should fail because plugin is blacklisted
    auto* sandbox = host.create_plugin_instance("test.blacklisted");
    CHECK(sandbox == nullptr);

    host.shutdown();
}

TEST_CASE("PluginHost allows non-blacklisted plugin creation",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    auto& factory = PluginFactory::instance();
    factory.register_plugin("test.clean",
        []{ return std::make_unique<TestPlugin>("CleanPlugin", PluginCategory::Effect); },
        "CleanPlugin", "TestVendor", PluginCategory::Effect);

    // Not blacklisted — should create successfully
    auto* sandbox = host.create_plugin_instance("test.clean");
    REQUIRE(sandbox != nullptr);

    host.shutdown();
}

// ══════════════════════════════════════════════════════════════════════
//  Multiple instances
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginHost can create multiple instances independently",
          "[plugin][host]")
{
    PluginHost host;
    host.init();

    auto& factory = PluginFactory::instance();
    factory.register_plugin("test.multi_a",
        []{ return std::make_unique<TestPlugin>("MultiA", PluginCategory::Effect); },
        "MultiA", "TestVendor", PluginCategory::Effect);

    factory.register_plugin("test.multi_b",
        []{ return std::make_unique<TestPlugin>("MultiB", PluginCategory::Synth); },
        "MultiB", "TestVendor", PluginCategory::Synth);

    auto* a = host.create_plugin_instance("test.multi_a");
    auto* b = host.create_plugin_instance("test.multi_b");
    auto* a2 = host.create_plugin_instance("test.multi_a");

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a2 != nullptr);

    // All should be distinct
    CHECK(a != b);
    CHECK(a != a2);
    CHECK(b != a2);

    // Destroy all
    CHECK(host.destroy_plugin_instance(a));
    CHECK(host.destroy_plugin_instance(b));
    CHECK(host.destroy_plugin_instance(a2));

    host.shutdown();
}
