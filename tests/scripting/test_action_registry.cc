// ARIA DAW — ActionRegistry Tests (Tasks 4.1, 4.6)
//
// TDD: RED — written before ActionRegistry implementation.
// Tests cover register, list, duplicate (replace handler), find missing,
// unregister, and capture hook dispatch.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

#include "scripting/macro_recorder.h"

using namespace aria;
using Catch::Matchers::ContainsSubstring;

// ─── Task 4.6: ActionRegistry — register, list, duplicate, find missing ───

TEST_CASE("ActionRegistry registers and lists actions", "[scripting][action_registry]") {
    ActionRegistry registry;

    SECTION("initially empty") {
        CHECK(registry.list().empty());
    }

    SECTION("register adds action to list") {
        registry.register_action("track.create", [](const nlohmann::json&) {});
        auto names = registry.list();
        REQUIRE(names.size() == 1);
        CHECK(names[0] == "track.create");
    }

    SECTION("register multiple actions lists all") {
        registry.register_action("track.create", [](const nlohmann::json&) {});
        registry.register_action("track.delete", [](const nlohmann::json&) {});
        registry.register_action("clip.move", [](const nlohmann::json&) {});

        auto names = registry.list();
        REQUIRE(names.size() == 3);
        // Order doesn't matter, but all must be present
        CHECK(std::find(names.begin(), names.end(), "track.create") != names.end());
        CHECK(std::find(names.begin(), names.end(), "track.delete") != names.end());
        CHECK(std::find(names.begin(), names.end(), "clip.move") != names.end());
    }
}

TEST_CASE("ActionRegistry duplicate registration replaces handler",
           "[scripting][action_registry]") {
    ActionRegistry registry;
    std::string last_called;

    registry.register_action("action.test", [&](const nlohmann::json&) {
        last_called = "original";
    });

    // Replace with new handler
    registry.register_action("action.test", [&](const nlohmann::json&) {
        last_called = "replaced";
    });

    // List should still show one entry
    auto names = registry.list();
    REQUIRE(names.size() == 1);
    CHECK(names[0] == "action.test");

    // Dispatch should call the NEW handler
    auto* handler = registry.find("action.test");
    REQUIRE(handler != nullptr);
    (*handler)(nlohmann::json::object());
    CHECK(last_called == "replaced");
}

TEST_CASE("ActionRegistry find returns handler or nullptr", "[scripting][action_registry]") {
    ActionRegistry registry;
    registry.register_action("known.action", [](const nlohmann::json&) {});

    SECTION("find returns handler for registered action") {
        auto* handler = registry.find("known.action");
        REQUIRE(handler != nullptr);
    }

    SECTION("find returns nullptr for unregistered action") {
        auto* handler = registry.find("does.not.exist");
        CHECK(handler == nullptr);
    }

    SECTION("find returns nullptr after unregister") {
        registry.unregister("known.action");
        auto* handler = registry.find("known.action");
        CHECK(handler == nullptr);
    }
}

TEST_CASE("ActionRegistry unregister removes action", "[scripting][action_registry]") {
    ActionRegistry registry;

    registry.register_action("temp.action", [](const nlohmann::json&) {});
    registry.register_action("keep.action", [](const nlohmann::json&) {});

    SECTION("unregister removes specific action") {
        registry.unregister("temp.action");
        auto names = registry.list();
        CHECK(names.size() == 1);
        CHECK(names[0] == "keep.action");
    }

    SECTION("unregister missing action is safe") {
        CHECK_NOTHROW(registry.unregister("does.not.exist"));
    }

    SECTION("unregister empty name is safe") {
        CHECK_NOTHROW(registry.unregister(""));
    }
}

TEST_CASE("ActionRegistry capture hook is invoked on dispatch",
           "[scripting][action_registry][capture]") {
    ActionRegistry registry;
    bool hook_called = false;
    std::string captured_name;
    nlohmann::json captured_params;

    SECTION("capture hook fires when set") {
        registry.register_action("test.action", [](const nlohmann::json&) {});

        registry.set_capture_hook(
            [&](std::string_view name, nlohmann::json params) {
                hook_called = true;
                captured_name = std::string(name);
                captured_params = std::move(params);
            });

        // Dispatch via internal mechanism
        nlohmann::json params = {{"key", "value"}};
        registry.dispatch("test.action", params);

        CHECK(hook_called);
        CHECK(captured_name == "test.action");
        CHECK(captured_params["key"] == "value");
    }

    SECTION("dispatch without capture hook still calls handler") {
        bool handler_called = false;
        registry.register_action("direct.action", [&](const nlohmann::json&) {
            handler_called = true;
        });

        // Set hook to null, then dispatch
        registry.set_capture_hook(nullptr);
        registry.dispatch("direct.action", nlohmann::json::object());

        CHECK(handler_called);
    }

    SECTION("dispatch on unregistered action does nothing") {
        // Should not crash
        CHECK_NOTHROW(registry.dispatch("unknown.action", nlohmann::json::object()));
    }
}
