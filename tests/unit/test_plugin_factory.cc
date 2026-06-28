#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "plugin/plugin_factory.h"
#include "test_common/test_plugin.h"

#include <memory>
#include <string>
#include <vector>

using namespace aria::plugin;

// ══════════════════════════════════════════════════════════════════════
//  Registration
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginFactory starts empty", "[plugin][factory]") {
    auto& factory = PluginFactory::instance();
    CHECK(factory.count() == 0);
}

TEST_CASE("PluginFactory can register and query plugins", "[plugin][factory]") {
    auto& factory = PluginFactory::instance();

    factory.register_plugin("test.eq",
        []{ return std::make_unique<TestPlugin>("EQ", PluginCategory::Effect); },
        "EQ", "TestVendor", PluginCategory::Effect);

    CHECK(factory.has("test.eq"));
    CHECK(factory.count() >= 1);
    CHECK(factory.category("test.eq") == PluginCategory::Effect);
}

TEST_CASE("PluginFactory create returns valid plugin", "[plugin][factory]") {
    auto& factory = PluginFactory::instance();

    factory.register_plugin("test.create_me",
        []{ return std::make_unique<TestPlugin>("CreateMe", PluginCategory::Synth); },
        "CreateMe", "TestVendor", PluginCategory::Synth);

    auto plugin = factory.create("test.create_me");
    REQUIRE(plugin != nullptr);
    CHECK(plugin->name() == "CreateMe");
    CHECK(plugin->category() == PluginCategory::Synth);
}

TEST_CASE("PluginFactory create returns nullptr for unknown ID",
          "[plugin][factory]")
{
    auto& factory = PluginFactory::instance();
    auto plugin = factory.create("nonexistent.plugin");
    CHECK(plugin == nullptr);
}

TEST_CASE("PluginFactory create returns nullptr when factory function fails",
          "[plugin][factory]")
{
    auto& factory = PluginFactory::instance();

    factory.register_plugin("test.broken",
        []() -> std::unique_ptr<AudioPlugin> { return nullptr; },
        "Broken", "TestVendor", PluginCategory::Effect);

    auto plugin = factory.create("test.broken");
    CHECK(plugin == nullptr);
}

// ══════════════════════════════════════════════════════════════════════
//  Search by vendor
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Search finds plugin by vendor name", "[plugin][factory][search]") {
    auto& factory = PluginFactory::instance();

    factory.register_plugin("valhalla.supermassive",
        []{ return std::make_unique<TestPlugin>("Supermassive", PluginCategory::Effect); },
        "Supermassive", "ValhallaDSP", PluginCategory::Effect);

    factory.register_plugin("valhalla.vintage_verb",
        []{ return std::make_unique<TestPlugin>("VintageVerb", PluginCategory::Effect); },
        "VintageVerb", "ValhallaDSP", PluginCategory::Effect);

    factory.register_plugin("uad.1176",
        []{ return std::make_unique<TestPlugin>("1176", PluginCategory::Effect); },
        "1176", "Universal Audio", PluginCategory::Effect);

    // Case-insensitive search by vendor
    auto results = factory.search("Valhalla", PluginCategory::Other);
    CHECK(results.size() == 2);

    // Both Valhalla plugins should be found
    bool found_supermassive = false;
    bool found_vintage = false;
    for (const auto& id : results) {
        if (id == "valhalla.supermassive") found_supermassive = true;
        if (id == "valhalla.vintage_verb") found_vintage = true;
    }
    CHECK(found_supermassive);
    CHECK(found_vintage);
}

TEST_CASE("Search by partial vendor name works", "[plugin][factory][search]") {
    auto& factory = PluginFactory::instance();

    factory.register_plugin("soundwide.thingy",
        []{ return std::make_unique<TestPlugin>("Thingy", PluginCategory::Effect); },
        "Thingy", "Soundwide Audio", PluginCategory::Effect);

    // Partial match
    auto results = factory.search("Soundwide", PluginCategory::Other);
    CHECK(results.size() >= 1);
}

TEST_CASE("Search returns empty for non-matching vendor", "[plugin][factory][search]") {
    auto& factory = PluginFactory::instance();

    auto results = factory.search("NonExistentVendorXYZ", PluginCategory::Other);
    CHECK(results.empty());
}

// ══════════════════════════════════════════════════════════════════════
//  Category filter
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Search filters by category", "[plugin][factory][search]") {
    auto& factory = PluginFactory::instance();

    factory.register_plugin("filter.eff_1",
        []{ return std::make_unique<TestPlugin>("Eff1", PluginCategory::Effect); },
        "Eff1", "TestVendor", PluginCategory::Effect);

    factory.register_plugin("filter.synth_1",
        []{ return std::make_unique<TestPlugin>("Synth1", PluginCategory::Synth); },
        "Synth1", "TestVendor", PluginCategory::Synth);

    factory.register_plugin("filter.eff_2",
        []{ return std::make_unique<TestPlugin>("Eff2", PluginCategory::Effect); },
        "Eff2", "TestVendor", PluginCategory::Effect);

    // Filter by Effect
    auto effects = factory.search("", PluginCategory::Effect);
    CHECK(effects.size() >= 2);
    for (const auto& id : effects) {
        CHECK(factory.category(id) == PluginCategory::Effect);
    }

    // Filter by Synth
    auto synths = factory.search("", PluginCategory::Synth);
    CHECK(synths.size() >= 1);
    for (const auto& id : synths) {
        CHECK(factory.category(id) == PluginCategory::Synth);
    }
}

TEST_CASE("Category Other means no filter", "[plugin][factory][search]") {
    auto& factory = PluginFactory::instance();

    // Search with Other (no filter) should return at least as many
    // results as any specific category
    auto all = factory.search("", PluginCategory::Other);

    // Our registrations above exist
    CHECK(all.size() >= 6);  // 6 plugins registered in previous tests
}

// ══════════════════════════════════════════════════════════════════════
//  Create by ID — exact match
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Create by ID returns matching plugin", "[plugin][factory]") {
    auto& factory = PluginFactory::instance();

    factory.register_plugin("exact.match.test",
        []{ return std::make_unique<TestPlugin>("ExactMatch", PluginCategory::Utility); },
        "ExactMatch", "TestVendor", PluginCategory::Utility);

    auto plugin = factory.create("exact.match.test");
    REQUIRE(plugin != nullptr);
    CHECK(plugin->name() == "ExactMatch");
    CHECK(plugin->category() == PluginCategory::Utility);
    // TestPlugin generates id as "test.<name>", so check by name instead
    CHECK(plugin->id() == "test.ExactMatch");
}

// ══════════════════════════════════════════════════════════════════════
//  Plugin IDs listing
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("PluginFactory plugin_ids returns sorted list", "[plugin][factory]") {
    auto& factory = PluginFactory::instance();
    auto ids = factory.plugin_ids();

    // Should contain all registered plugin IDs
    CHECK(ids.size() == factory.count());

    // Should be sorted
    for (size_t i = 1; i < ids.size(); ++i) {
        CHECK(ids[i - 1] <= ids[i]);
    }
}

// ══════════════════════════════════════════════════════════════════════
//  Search matches ID, name, and vendor
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Search matches plugin ID", "[plugin][factory][search]") {
    auto& factory = PluginFactory::instance();

    factory.register_plugin("com.unique-matcher.effect",
        []{ return std::make_unique<TestPlugin>("UniqueMatcher", PluginCategory::Effect); },
        "UniqueMatcher", "UniqueCorp", PluginCategory::Effect);

    // Search by ID substring
    auto by_id = factory.search("unique-matcher", PluginCategory::Other);
    CHECK(by_id.size() >= 1);
    CHECK(by_id[0] == "com.unique-matcher.effect");

    // Search by name substring  
    auto by_name = factory.search("UniqueMatcher", PluginCategory::Other);
    CHECK(by_name.size() >= 1);

    // Search by vendor substring
    auto by_vendor = factory.search("UniqueCorp", PluginCategory::Other);
    CHECK(by_vendor.size() >= 1);
}
