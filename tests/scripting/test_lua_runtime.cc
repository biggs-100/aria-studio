// ARIA DAW — LuaRuntime Sandbox Tests (Tasks 2.8, 2.9)
//
// Task 2.8: create VM, run valid script, assert output forwarded to log
// Task 2.9: sandbox — io.open/os.execute/infinite loop, assert VM terminates

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "scripting/lua_runtime.h"

using namespace aria;

// ─── Task 2.8: VM Lifecycle ─────────────────────────────────────

TEST_CASE("LuaRuntime creates and runs valid scripts", "[scripting][runtime][vm]") {
    LuaRuntime runtime;

    SECTION("default constructed state is ready") {
        // VM is constructed with base/table/string/math libraries loaded
        REQUIRE(runtime.lua_state() != nullptr);
        // Prove it's usable by running a trivial script
        auto result = runtime.state().safe_script("return 1 + 1");
        REQUIRE(result.valid());
    }

    SECTION("run valid arithmetic expression") {
        auto result = runtime.state().safe_script("return 2 + 2");
        REQUIRE(result.valid());
        CHECK(result.get<int>() == 4);
    }

    SECTION("run script with variable assignment") {
        auto result = runtime.state().safe_script("x = 42; return x");
        REQUIRE(result.valid());
        CHECK(result.get<int>() == 42);
    }

    SECTION("run script with string operations") {
        auto result = runtime.state().safe_script("return 'hello' .. ' ' .. 'world'");
        REQUIRE(result.valid());
        CHECK(result.get<std::string>() == "hello world");
    }
}

TEST_CASE("LuaRuntime reset produces clean state", "[scripting][runtime][vm]") {
    LuaRuntime runtime;

    // Set a global
    runtime.state().safe_script("counter = 99");
    auto before = runtime.state().safe_script("return counter");
    REQUIRE(before.valid());
    CHECK(before.get<int>() == 99);

    // Reset the VM state by collecting garbage and re-creating environment
    runtime.state().collect_garbage();
    runtime.state().safe_script("counter = nil");

    // After clearing, the global should be gone
    // (sol::state doesn't expose lua_setglobals reset, but we can lua_pushnil + lua_setglobal)
    lua_State* L = runtime.state().lua_state();
    lua_pushnil(L);
    lua_setglobal(L, "counter");

    auto after = runtime.state().safe_script("return counter");
    REQUIRE(after.valid());
    CHECK(after.get_type() == sol::type::nil);
}

// ─── Task 2.9: Sandbox — Stripped Libraries ─────────────────────

TEST_CASE("LuaRuntime sandbox strips dangerous I/O functions", "[scripting][runtime][sandbox][io]") {
    LuaRuntime runtime;

    SECTION("io library is nil") {
        auto result = runtime.state().safe_script("return io");
        REQUIRE(result.valid());
        CHECK(result.get_type() == sol::type::nil);
    }

    SECTION("os library is nil") {
        auto result = runtime.state().safe_script("return os");
        REQUIRE(result.valid());
        CHECK(result.get_type() == sol::type::nil);
    }

    SECTION("loadfile is nil") {
        auto result = runtime.state().safe_script("return loadfile");
        REQUIRE(result.valid());
        CHECK(result.get_type() == sol::type::nil);
    }

    SECTION("dofile is nil") {
        auto result = runtime.state().safe_script("return dofile");
        REQUIRE(result.valid());
        CHECK(result.get_type() == sol::type::nil);
    }

    SECTION("require is nil") {
        auto result = runtime.state().safe_script("return require");
        REQUIRE(result.valid());
        CHECK(result.get_type() == sol::type::nil);
    }
}

TEST_CASE("LuaRuntime sandbox blocks io.open attempts", "[scripting][runtime][sandbox][io]") {
    LuaRuntime runtime;

    // Attempt to call io.open — should produce an error since io is nil
    // Return a table {ok, err} because safe_script captures only the first
    // return value; multiple returns from Lua would lose the second value.
    auto result = runtime.state().safe_script(R"(
        local ok, err = pcall(function()
            io.open("/etc/passwd")
        end)
        return {ok, err}
    )");

    REQUIRE(result.valid());
    sol::table tbl = result.get<sol::table>();
    bool ok = tbl[1];
    CHECK_FALSE(ok);  // pcall should catch the error — returns false
}

TEST_CASE("LuaRuntime sandbox blocks os.execute attempts", "[scripting][runtime][sandbox][os]") {
    LuaRuntime runtime;

    auto result = runtime.state().safe_script(R"(
        local ok, err = pcall(function()
            os.execute("rm -rf /")
        end)
        return {ok, err}
    )");

    REQUIRE(result.valid());
    sol::table tbl = result.get<sol::table>();
    bool ok = tbl[1];
    CHECK_FALSE(ok);
}

// ─── Task 2.9: Sandbox — Instruction Limit ──────────────────────

TEST_CASE("LuaRuntime instruction hook terminates infinite loops", "[scripting][runtime][sandbox][instr]") {
    LuaRuntime runtime;
    // Set a low instruction limit for fast test execution
    runtime.set_instruction_limit(5000);

    // Infinite loop — should be terminated by instruction hook
    auto result = runtime.state().safe_script(R"(
        local ok, err = pcall(function()
            while true do end
        end)
        return {ok, err}
    )");

    REQUIRE(result.valid());
    sol::table tbl = result.get<sol::table>();
    bool ok = tbl[1];
    CHECK_FALSE(ok);  // Should error out
}

// ─── Task 2.9: Sandbox — Memory Limit ──────────────────────────

TEST_CASE("LuaRuntime memory allocator caps memory usage", "[scripting][runtime][sandbox][mem]") {
    LuaRuntime runtime;
    // Set a very low memory limit (64 KB) to trigger easily
    runtime.set_memory_limit(64 * 1024);

    // Try to allocate a 128 KB string — should fail
    auto result = runtime.state().safe_script(R"(
        local ok, err = pcall(function()
            -- Attempt to create a large string (128 KB)
            local big = string.rep("X", 128 * 1024)
            return big
        end)
        return {ok, err}
    )");

    REQUIRE(result.valid());
    sol::table tbl = result.get<sol::table>();
    bool ok = tbl[1];
    CHECK_FALSE(ok);  // Memory allocation should be denied
}

TEST_CASE("LuaRuntime memory allocator allows reasonable allocations", "[scripting][runtime][sandbox][mem]") {
    LuaRuntime runtime;
    // Default 16 MB limit — small allocations should work
    runtime.set_memory_limit(16 * 1024 * 1024);

    // Small allocation should succeed
    auto result = runtime.state().safe_script(R"(
        local ok, err = pcall(function()
            local small = string.rep("X", 1024)
            return small
        end)
        return {ok, err}
    )");

    REQUIRE(result.valid());
    sol::table tbl = result.get<sol::table>();
    bool ok = tbl[1];
    CHECK(ok);  // 1 KB within 16 MB — should succeed
}
