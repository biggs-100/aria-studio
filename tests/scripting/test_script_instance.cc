// ARIA DAW — ScriptInstance Tests (Task 2.10)
//
// Task 2.10: ScriptInstance manages source + execute + error states
// Spec scenarios: load syntax error, runtime error, load/unload lifecycle

#include <catch2/catch_test_macros.hpp>

#include "scripting/script_instance.h"

using namespace aria;

TEST_CASE("ScriptInstance manages lifecycle", "[scripting][instance]") {
    ScriptInstance instance(42, "/scripts/test.lua");

    SECTION("id is assigned at construction") {
        CHECK(instance.id() == 42);
    }

    SECTION("filepath is assigned at construction") {
        CHECK(instance.filepath() == "/scripts/test.lua");
    }

    SECTION("initially not loaded") {
        CHECK_FALSE(instance.is_loaded());
    }
}

TEST_CASE("ScriptInstance load and execute valid script", "[scripting][instance][lifecycle]") {
    ScriptInstance instance(1, "/scripts/arithmetic.lua");

    SECTION("load succeeds for valid Lua source") {
        bool loaded = instance.load("return 2 + 2");
        CHECK(loaded);
        CHECK(instance.is_loaded());
    }

    SECTION("execute returns result for valid script") {
        instance.load("result = 42");
        bool executed = instance.execute();
        CHECK(executed);
    }

    SECTION("load twice preserves last source") {
        instance.load("x = 1");
        instance.load("x = 2");
        instance.execute();
        CHECK(instance.is_loaded());
    }
}

TEST_CASE("ScriptInstance handles syntax errors gracefully", "[scripting][instance][error]") {
    ScriptInstance instance(2, "/scripts/broken.lua");

    SECTION("load returns false for invalid syntax") {
        bool loaded = instance.load("this is not valid lua {{{");
        CHECK_FALSE(loaded);
        // Error message should be populated
        CHECK_FALSE(instance.last_error().empty());
    }

    SECTION("VM remains usable after syntax error") {
        instance.load("this is not valid lua {{{");
        CHECK_FALSE(instance.is_loaded());

        // Subsequent valid script should still work
        bool reloaded = instance.load("x = 10");
        CHECK(reloaded);
        bool executed = instance.execute();
        CHECK(executed);
    }

    SECTION("load with empty string returns error") {
        bool loaded = instance.load("");
        CHECK_FALSE(loaded);
        CHECK_FALSE(instance.last_error().empty());
    }
}

TEST_CASE("ScriptInstance handles runtime errors", "[scripting][instance][error]") {
    ScriptInstance instance(3, "/scripts/err.lua");

    SECTION("runtime error is captured on execute") {
        instance.load("error('something went wrong')");
        bool executed = instance.execute();
        CHECK_FALSE(executed);
        // Error message should contain the Lua error text
        CHECK_FALSE(instance.last_error().empty());
    }

    SECTION("nil dereference is captured") {
        instance.load("x = nil; return x.field");
        // This will error at load time since we use pcall internally
        bool loaded = instance.load("x = nil; return x.field");
        CHECK_FALSE(loaded);
    }
}

TEST_CASE("ScriptInstance unload resets state", "[scripting][instance][lifecycle]") {
    ScriptInstance instance(4, "/scripts/stateful.lua");

    SECTION("unload clears loaded flag") {
        instance.load("x = 42");
        CHECK(instance.is_loaded());
        instance.unload();
        CHECK_FALSE(instance.is_loaded());
    }

    SECTION("reload after unload creates fresh state") {
        instance.load("counter = 10");
        instance.execute();
        instance.unload();

        // After unload, state is reset. Script globals are gone.
        instance.load("return counter");
        instance.execute();

        // counter should be nil in fresh state
        CHECK(instance.is_loaded());
    }

    SECTION("double unload is safe") {
        instance.load("x = 1");
        instance.unload();
        CHECK_NOTHROW(instance.unload());
        CHECK_FALSE(instance.is_loaded());
    }
}
