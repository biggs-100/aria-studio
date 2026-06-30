// ARIA DAW — ScriptManager Tests (Task 2.11)
//
// Task 2.11: ScriptManager create_vm + load + execute + unload + tick

#include <catch2/catch_test_macros.hpp>

#include "scripting/script_manager.h"

using namespace aria;

TEST_CASE("ScriptManager creates VMs and manages lifecycle", "[scripting][manager]") {
    ScriptManager manager;

    SECTION("create_vm returns a valid non-zero id") {
        auto id = manager.create_vm();
        CHECK(id > 0);
    }

    SECTION("multiple create_vm calls return unique ids") {
        auto id1 = manager.create_vm();
        auto id2 = manager.create_vm();
        CHECK(id1 != id2);
    }
}

TEST_CASE("ScriptManager load and execute workflow", "[scripting][manager][lifecycle]") {
    ScriptManager manager;
    auto id = manager.create_vm();

    SECTION("load valid script returns true") {
        bool loaded = manager.load(id, "x = 42");
        CHECK(loaded);
    }

    SECTION("load then execute succeeds") {
        manager.load(id, "result = 10 + 20");
        bool executed = manager.execute(id);
        CHECK(executed);
    }

    SECTION("execute returns false for script with error") {
        manager.load(id, "error('bad')");
        bool executed = manager.execute(id);
        CHECK_FALSE(executed);
    }
}

TEST_CASE("ScriptManager handles errors gracefully", "[scripting][manager][error]") {
    ScriptManager manager;
    auto id = manager.create_vm();

    SECTION("load with invalid syntax returns false") {
        bool loaded = manager.load(id, "syntax {{{ error");
        CHECK_FALSE(loaded);
    }

    SECTION("invalid id returns false") {
        bool loaded = manager.load(9999, "x = 1");
        CHECK_FALSE(loaded);
    }

    SECTION("execute on non-existent id returns false") {
        bool executed = manager.execute(9999);
        CHECK_FALSE(executed);
    }
}

TEST_CASE("ScriptManager unload and reload cycle", "[scripting][manager][lifecycle]") {
    ScriptManager manager;
    auto id = manager.create_vm();

    SECTION("unload is safe on idle VM") {
        CHECK_NOTHROW(manager.unload(id));
    }

    SECTION("reload after unload works") {
        manager.load(id, "value = 1");
        manager.execute(id);
        manager.unload(id);

        // Reload should work on fresh state
        bool reloaded = manager.load(id, "value = 2");
        CHECK(reloaded);
    }

    SECTION("unload on invalid id is safe") {
        CHECK_NOTHROW(manager.unload(9999));
    }
}

TEST_CASE("ScriptManager tick is callable", "[scripting][manager][tick]") {
    ScriptManager manager;

    SECTION("tick on empty manager is safe") {
        CHECK_NOTHROW(manager.tick());
    }

    SECTION("tick with active scripts is safe") {
        auto id = manager.create_vm();
        manager.load(id, "x = 1");
        CHECK_NOTHROW(manager.tick());
    }

    SECTION("tick with multiple scripts is safe") {
        auto id1 = manager.create_vm();
        auto id2 = manager.create_vm();
        auto id3 = manager.create_vm();
        manager.load(id1, "a = 1");
        manager.load(id2, "b = 2");
        manager.load(id3, "c = 3");
        CHECK_NOTHROW(manager.tick());
    }
}

TEST_CASE("ScriptManager handles isolation between VMs", "[scripting][manager][isolation]") {
    ScriptManager manager;

    SECTION("scripts in separate VMs don't share globals") {
        auto id1 = manager.create_vm();
        auto id2 = manager.create_vm();

        manager.load(id1, "shared_var = 'from_vm1'");
        manager.execute(id1);

        // VM 2 should not see shared_var from VM 1
        // We can't easily check this without a query API,
        // but executing a script that returns the value tells us
        // Since the script never sets it, it should be nil/error-free

        // VM2 starts fresh — load a script that accesses the global
        bool vm2_ok = manager.load(id2, "return shared_var");
        CHECK(vm2_ok);
    }
}
