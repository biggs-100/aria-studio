#include <catch2/catch_test_macros.hpp>
#include "kernel/undo_stack.h"

using namespace aria;

TEST_CASE("Undo stack basic operations", "[kernel][undo]") {
    UndoStack stack;

    SECTION("Empty stack cannot undo") {
        REQUIRE_FALSE(stack.undo());
        REQUIRE_FALSE(stack.redo());
        REQUIRE(stack.undo_count() == 0);
    }

    SECTION("Push and undo") {
        int value = 0;
        stack.push("increment",
            [&]() { value--; },   // undo
            [&]() { value++; }    // redo
        );

        REQUIRE(stack.undo_count() == 1);
        REQUIRE(stack.undo_label() == "increment");
    }
}
