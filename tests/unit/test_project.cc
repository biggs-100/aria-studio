#include <catch2/catch_test_macros.hpp>
#include "project/project.h"

using namespace aria;

TEST_CASE("Project creation", "[project]") {
    Project project;

    SECTION("Create new project") {
        REQUIRE(project.create("/tmp/test.aria", "Test Project"));
        REQUIRE(project.name() == "Test Project");
        REQUIRE_FALSE(project.is_modified());
    }

    SECTION("Close resets state") {
        project.create("/tmp/test.aria", "Test");
        project.close();
        REQUIRE(project.path().empty());
    }
}
