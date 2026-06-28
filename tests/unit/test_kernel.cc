#include <catch2/catch_test_macros.hpp>
#include "kernel/application.h"

using namespace aria;

TEST_CASE("Application lifecycle", "[kernel]") {
    auto& app = Application::instance();

    SECTION("Application starts stopped") {
        REQUIRE_FALSE(app.is_running());
    }

    SECTION("Application initializes and runs") {
        char name[] = "aria_test";
        char* argv[] = {name, nullptr};
        REQUIRE(app.init(1, argv));
        app.shutdown();
        REQUIRE_FALSE(app.is_running());
    }
}

TEST_CASE("Application singleton", "[kernel]") {
    auto& app1 = Application::instance();
    auto& app2 = Application::instance();
    REQUIRE(&app1 == &app2);
}
