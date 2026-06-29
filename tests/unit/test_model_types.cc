#include <catch2/catch_test_macros.hpp>
#include "model/types.h"

using namespace aria;

// ─── TrackID ─────────────────────────────────────────────────────

TEST_CASE("TrackID — default construction", "[model][types][trackid]") {
    TrackID id;

    SECTION("default value is 0") {
        REQUIRE(id.value == 0);
    }
}

TEST_CASE("TrackID — construction from uint64_t", "[model][types][trackid]") {
    SECTION("explicit construction with value") {
        TrackID id{42};
        REQUIRE(id.value == 42);
    }

    SECTION("implicit conversion from uint64_t") {
        TrackID id = 100;
        REQUIRE(id.value == 100);
    }
}

TEST_CASE("TrackID — equality", "[model][types][trackid]") {
    TrackID a{42};
    TrackID b{42};
    TrackID c{99};

    SECTION("same values are equal") {
        REQUIRE(a == b);
    }

    SECTION("different values are not equal") {
        REQUIRE(a != c);
    }

    SECTION("self-equality") {
        REQUIRE(a == a);
    }
}

TEST_CASE("TrackID — uint64_t compatibility", "[model][types][trackid]") {
    TrackID id{42};

    SECTION("equality with uint64_t") {
        REQUIRE(id == uint64_t{42});
    }

    SECTION("inequality with uint64_t") {
        REQUIRE(id != uint64_t{99});
    }

    SECTION("comparison with uint64_t(0) for default") {
        TrackID default_id;
        REQUIRE(default_id == uint64_t{0});
    }
}

TEST_CASE("TrackID — value is uint64_t", "[model][types][trackid]") {
    TrackID id{0xDEADBEEF};

    SECTION("value member is accessible") {
        REQUIRE(id.value == 0xDEADBEEF);
    }

    SECTION("value is uint64_t type") {
        STATIC_REQUIRE(std::is_same_v<decltype(id.value), uint64_t>);
    }
}
