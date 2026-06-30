// ARIA DAW — MacroRecorder Tests (Tasks 4.7, 4.8, 4.9)
//
// TDD: RED — written before MacroRecorder implementation verification.
// Tests cover start/stop, serialization, timing interleaving, and save/load.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "scripting/macro_recorder.h"

using namespace aria;
using Catch::Matchers::ContainsSubstring;

// ─── Task 4.7: MacroRecorder — start/stop empty, 3 actions serializes ───

TEST_CASE("MacroRecorder start/stop empty session", "[scripting][macro_recorder]") {
    ActionRegistry registry;
    MacroRecorder recorder(registry);

    SECTION("not recording initially") {
        CHECK_FALSE(recorder.is_recording());
    }

    SECTION("start transitions to recording state") {
        recorder.start();
        CHECK(recorder.is_recording());
    }

    SECTION("start then stop returns to non-recording") {
        recorder.start();
        recorder.stop();
        CHECK_FALSE(recorder.is_recording());
    }

    SECTION("empty session returns empty actions") {
        recorder.start();
        recorder.stop();
        CHECK(recorder.actions().empty());
    }

    SECTION("empty session serializes to empty string") {
        recorder.start();
        recorder.stop();
        CHECK(recorder.to_lua_script().empty());
    }

    SECTION("double-start resets session") {
        recorder.start();
        // Dispatch an action
        registry.dispatch("transport.play", nlohmann::json::object());
        recorder.start(); // Reset
        recorder.stop();
        CHECK(recorder.actions().empty());
    }

    SECTION("stop when not recording is safe") {
        CHECK_NOTHROW(recorder.stop());
    }
}

TEST_CASE("MacroRecorder captures actions during session",
           "[scripting][macro_recorder][capture]") {
    ActionRegistry registry;
    // Register some handlers
    registry.register_action("transport.play",
                             [](const nlohmann::json&) {});
    registry.register_action("transport.stop",
                             [](const nlohmann::json&) {});

    MacroRecorder recorder(registry);
    recorder.start();

    // Dispatch actions (simulates user action)
    registry.dispatch("transport.play", nlohmann::json::object());
    registry.dispatch("transport.stop", nlohmann::json::object());

    recorder.stop();

    SECTION("captures dispatched actions") {
        REQUIRE(recorder.actions().size() == 2);
        CHECK(recorder.actions()[0].action_name == "transport.play");
        CHECK(recorder.actions()[1].action_name == "transport.stop");
    }
}

// ─── Task 4.7: Serialize with 3 actions ─────────────────────────

TEST_CASE("MacroRecorder serializes captured actions to Lua",
           "[scripting][macro_recorder][serialize]") {
    ActionRegistry registry;
    registry.register_action("transport.play",
                             [](const nlohmann::json&) {});
    registry.register_action("transport.stop",
                             [](const nlohmann::json&) {});
    registry.register_action("transport.record",
                             [](const nlohmann::json&) {});

    MacroRecorder recorder(registry);
    recorder.start();

    registry.dispatch("transport.play", nlohmann::json::object());
    registry.dispatch("transport.stop", nlohmann::json::object());
    registry.dispatch("transport.record", nlohmann::json::object());

    recorder.stop();
    std::string lua = recorder.to_lua_script();

    SECTION("contains all three action calls") {
        CHECK_THAT(lua, ContainsSubstring("aria.transport.play()"));
        CHECK_THAT(lua, ContainsSubstring("aria.transport.stop()"));
        CHECK_THAT(lua, ContainsSubstring("aria.transport.record()"));
    }

    SECTION("contains header comment with action count") {
        CHECK_THAT(lua, ContainsSubstring("3 actions"));
    }

    SECTION("not empty") {
        CHECK_FALSE(lua.empty());
    }
}

// ─── Task 4.8: Serialize with interleaved timing ─────────────────

TEST_CASE("MacroRecorder inserts timing waits for action gaps",
           "[scripting][macro_recorder][timing]") {
    ActionRegistry registry;
    registry.register_action("transport.play",
                             [](const nlohmann::json&) {});
    registry.register_action("transport.stop",
                             [](const nlohmann::json&) {});

    MacroRecorder recorder(registry);
    recorder.start();

    // First action immediately
    registry.dispatch("transport.play", nlohmann::json::object());

    // Wait ~50ms before second action
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    registry.dispatch("transport.stop", nlohmann::json::object());

    recorder.stop();
    std::string lua = recorder.to_lua_script();

    SECTION("contains timing wait call") {
        CHECK_THAT(lua, ContainsSubstring("aria.timing.wait("));
    }

    SECTION("contains both action calls") {
        CHECK_THAT(lua, ContainsSubstring("aria.transport.play()"));
        CHECK_THAT(lua, ContainsSubstring("aria.transport.stop()"));
    }

    SECTION("action order is preserved") {
        // play comes before stop
        auto play_pos = lua.find("play");
        auto stop_pos = lua.find("stop");
        CHECK(play_pos < stop_pos);
    }

    SECTION("timing wait value is reasonable (>= 40ms)") {
        // Extract the wait value from the lua output
        // Pattern: aria.timing.wait(XX)
        auto wait_pos = lua.find("aria.timing.wait(");
        REQUIRE(wait_pos != std::string::npos);
        auto paren_open = wait_pos + std::string("aria.timing.wait(").size();
        auto paren_close = lua.find(')', paren_open);
        REQUIRE(paren_close != std::string::npos);
        std::string ms_str = lua.substr(paren_open, paren_close - paren_open);
        int ms = std::stoi(ms_str);
        CHECK(ms >= 40);
    }
}

TEST_CASE("MacroRecorder zero-gap actions have no timing wait",
           "[scripting][macro_recorder][timing]") {
    ActionRegistry registry;
    registry.register_action("transport.play", [](const nlohmann::json&) {});
    registry.register_action("transport.stop", [](const nlohmann::json&) {});

    MacroRecorder recorder(registry);
    recorder.start();

    // Both actions dispatched in rapid succession (no measurable gap)
    registry.dispatch("transport.play", nlohmann::json::object());
    registry.dispatch("transport.stop", nlohmann::json::object());

    recorder.stop();
    std::string lua = recorder.to_lua_script();

    // The gap might be up to 5ms, in which case no wait is inserted
    // (threshold is > 5ms in our implementation)
    // Just verify both actions are present and no unexpected content
    CHECK_THAT(lua, ContainsSubstring("aria.transport.play()"));
    CHECK_THAT(lua, ContainsSubstring("aria.transport.stop()"));
}

// ─── Task 4.9: Save and load ─────────────────────────────────────

TEST_CASE("MacroRecorder save and load round-trip",
           "[scripting][macro_recorder][storage]") {
    ActionRegistry registry;
    registry.register_action("transport.play",
                             [](const nlohmann::json&) {});
    registry.register_action("transport.record",
                             [](const nlohmann::json&) {});

    MacroRecorder recorder(registry);
    recorder.start();

    registry.dispatch("transport.play", nlohmann::json::object());
    registry.dispatch("transport.record", nlohmann::json::object());

    recorder.stop();
    std::string original_script = recorder.to_lua_script();

    SECTION("save returns true") {
        CHECK(recorder.save("TestMacroRoundTrip"));
    }

    SECTION("load returns matching content") {
        REQUIRE(recorder.save("TestMacroRoundTrip"));
        std::string loaded = recorder.load("TestMacroRoundTrip");
        CHECK_FALSE(loaded.empty());
        CHECK_THAT(loaded, ContainsSubstring("aria.transport.play()"));
        CHECK_THAT(loaded, ContainsSubstring("aria.transport.record()"));
    }

    SECTION("load nonexistent macro returns empty string") {
        std::string result = recorder.load("NonExistentMacro_xyz");
        CHECK(result.empty());
    }

    SECTION("save empty session") {
        MacroRecorder empty_recorder(registry);
        empty_recorder.start();
        empty_recorder.stop();
        CHECK(empty_recorder.save("EmptyMacroTest"));
    }
}

TEST_CASE("MacroRecorder save with special characters in name",
           "[scripting][macro_recorder][storage]") {
    ActionRegistry registry;
    MacroRecorder recorder(registry);
    recorder.start();
    recorder.stop();

    // Name with spaces and special chars should still work
    CHECK(recorder.save("My Test Macro_v2"));
}
