#include <catch2/catch_test_macros.hpp>

#include "plugin/plugin_blacklist.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace aria::plugin;

// ══════════════════════════════════════════════════════════════════════
//  Escalation
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Initial state is None for unknown plugin", "[plugin][blacklist]") {
    PluginBlacklist bl;
    REQUIRE(bl.level("unknown.plugin") == BlacklistLevel::None);
    REQUIRE_FALSE(bl.is_blacklisted("unknown.plugin"));
}

TEST_CASE("One crash escalates to Warning", "[plugin][blacklist]") {
    PluginBlacklist bl;
    bl.report_crash("test.plugin.a");
    REQUIRE(bl.level("test.plugin.a") == BlacklistLevel::Warning);
    REQUIRE_FALSE(bl.is_blacklisted("test.plugin.a"));
}

TEST_CASE("Three crashes escalate to Disabled", "[plugin][blacklist]") {
    PluginBlacklist bl;
    bl.report_crash("test.plugin.b");
    bl.report_crash("test.plugin.b");
    bl.report_crash("test.plugin.b");

    REQUIRE(bl.level("test.plugin.b") == BlacklistLevel::Disabled);
    REQUIRE(bl.is_blacklisted("test.plugin.b"));
}

TEST_CASE("Five crashes escalate to Banned", "[plugin][blacklist]") {
    PluginBlacklist bl;
    for (int i = 0; i < 5; ++i) {
        bl.report_crash("test.plugin.c");
    }

    REQUIRE(bl.level("test.plugin.c") == BlacklistLevel::Banned);
    REQUIRE(bl.is_blacklisted("test.plugin.c"));
}

TEST_CASE("Crash count accumulates across reports", "[plugin][blacklist]") {
    PluginBlacklist bl;
    bl.report_crash("test.plugin.d");
    REQUIRE(bl.level("test.plugin.d") == BlacklistLevel::Warning);

    bl.report_crash("test.plugin.d");
    REQUIRE(bl.level("test.plugin.d") == BlacklistLevel::Warning);  // still Warning (2 < 3)

    bl.report_crash("test.plugin.d");
    REQUIRE(bl.level("test.plugin.d") == BlacklistLevel::Disabled);
}

// ══════════════════════════════════════════════════════════════════════
//  Manual Ban
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Manual ban overrides auto escalation", "[plugin][blacklist]") {
    PluginBlacklist bl;

    // Start with a Warning-level plugin
    bl.report_crash("test.plugin.e");
    REQUIRE(bl.level("test.plugin.e") == BlacklistLevel::Warning);

    // Manual ban
    bl.ban("test.plugin.e", "User banned this plugin");
    REQUIRE(bl.level("test.plugin.e") == BlacklistLevel::Banned);
    REQUIRE(bl.is_blacklisted("test.plugin.e"));
}

TEST_CASE("Manual ban with reason is stored", "[plugin][blacklist]") {
    PluginBlacklist bl;
    bl.ban("test.plugin.f", "Causes audio dropouts");

    auto entry = bl.entry("test.plugin.f");
    REQUIRE(entry.has_value());
    REQUIRE(entry->level == BlacklistLevel::Banned);
    REQUIRE(entry->reason == "Causes audio dropouts");
}

TEST_CASE("Unban removes entry", "[plugin][blacklist]") {
    PluginBlacklist bl;
    bl.report_crash("test.plugin.g");
    bl.report_crash("test.plugin.g");
    bl.report_crash("test.plugin.g");
    REQUIRE(bl.is_blacklisted("test.plugin.g"));

    bl.unban("test.plugin.g");
    REQUIRE_FALSE(bl.is_blacklisted("test.plugin.g"));
    REQUIRE(bl.level("test.plugin.g") == BlacklistLevel::None);
}

// ══════════════════════════════════════════════════════════════════════
//  Clear
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Clear entry allows reload", "[plugin][blacklist]") {
    PluginBlacklist bl;
    bl.report_crash("test.plugin.h");
    bl.report_crash("test.plugin.h");
    bl.report_crash("test.plugin.h");
    REQUIRE(bl.is_blacklisted("test.plugin.h"));

    bl.clear_entry("test.plugin.h");
    REQUIRE_FALSE(bl.is_blacklisted("test.plugin.h"));
    REQUIRE(bl.level("test.plugin.h") == BlacklistLevel::None);
}

TEST_CASE("Clear all removes all entries", "[plugin][blacklist]") {
    PluginBlacklist bl;
    bl.report_crash("test.one");
    bl.report_crash("test.two");
    bl.report_crash("test.two");
    bl.report_crash("test.two");

    REQUIRE(bl.entries().size() == 2);

    bl.clear();
    REQUIRE(bl.entries().empty());
}

// ══════════════════════════════════════════════════════════════════════
//  Persistence — JSON
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Save and load round-trip", "[plugin][blacklist][persist]") {
    PluginBlacklist bl;

    // Add some entries
    bl.report_crash("test.persistence.a");
    bl.report_crash("test.persistence.b");
    bl.report_crash("test.persistence.b");
    bl.report_crash("test.persistence.b");
    bl.ban("test.persistence.c", "Manual ban");

    // Save to temp file
    std::string path = "test_blacklist_roundtrip.json";
    REQUIRE(bl.save(path));

    // Create a new blacklist and load
    PluginBlacklist bl2;
    REQUIRE(bl2.load(path));

    // Verify entries
    REQUIRE(bl2.level("test.persistence.a") == BlacklistLevel::Warning);
    REQUIRE(bl2.level("test.persistence.b") == BlacklistLevel::Disabled);
    REQUIRE(bl2.level("test.persistence.c") == BlacklistLevel::Banned);

    // Verify reasons
    auto entry_c = bl2.entry("test.persistence.c");
    REQUIRE(entry_c.has_value());
    REQUIRE(entry_c->reason == "Manual ban");

    // Cleanup
    std::remove(path.c_str());
}

TEST_CASE("Corrupt JSON initializes empty", "[plugin][blacklist][persist]") {
    std::string path = "test_blacklist_corrupt.json";

    // Write invalid JSON
    {
        std::ofstream ofs(path);
        REQUIRE(ofs.is_open());
        ofs << "{ this is not valid json }";
        ofs.close();
    }

    PluginBlacklist bl;
    // load() returns false for corrupt files
    bool loaded = bl.load(path);
    // Should return false (parse failed)
    // But entries should be empty
    REQUIRE(bl.entries().empty());

    // Cleanup
    std::remove(path.c_str());
}

TEST_CASE("Missing file initializes empty", "[plugin][blacklist][persist]") {
    PluginBlacklist bl;
    // Non-existent file should not crash — returns true (empty)
    REQUIRE(bl.load("nonexistent_blacklist.json"));
    REQUIRE(bl.entries().empty());
}

TEST_CASE("Empty JSON object loads cleanly", "[plugin][blacklist][persist]") {
    std::string path = "test_blacklist_empty.json";

    {
        std::ofstream ofs(path);
        REQUIRE(ofs.is_open());
        ofs << "{}";
        ofs.close();
    }

    PluginBlacklist bl;
    REQUIRE(bl.load(path));
    REQUIRE(bl.entries().empty());

    std::remove(path.c_str());
}
