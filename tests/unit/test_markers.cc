#include <catch2/catch_test_macros.hpp>
#include "model/marker.h"

using namespace aria;

TEST_CASE("MarkerList — add and find markers", "[model][markers]") {
    MarkerList ml;

    SECTION("empty list") {
        REQUIRE(ml.count() == 0);
        REQUIRE(ml.all().empty());
    }

    SECTION("add marker") {
        auto id = ml.add("Intro", 0);
        REQUIRE(id.value != 0);
        REQUIRE(ml.count() == 1);

        auto* m = ml.find(id);
        REQUIRE(m != nullptr);
        REQUIRE(m->name == "Intro");
        REQUIRE(m->ppqn == 0);
    }

    SECTION("add multiple markers return sorted") {
        auto id2 = ml.add("Verse", 3840);
        auto id1 = ml.add("Intro", 0);

        auto all = ml.all();
        REQUIRE(all.size() == 2);
        REQUIRE(all[0].ppqn == 0);
        REQUIRE(all[1].ppqn == 3840);
    }
}

TEST_CASE("MarkerList — remove markers", "[model][markers]") {
    MarkerList ml;
    auto id = ml.add("Chorus", 960);

    SECTION("remove existing") {
        REQUIRE(ml.remove(id));
        REQUIRE(ml.count() == 0);
        REQUIRE(ml.find(id) == nullptr);
    }

    SECTION("remove non-existent") {
        REQUIRE_FALSE(ml.remove(MarkerID{999}));
    }
}

TEST_CASE("MarkerList — move markers", "[model][markers]") {
    MarkerList ml;
    auto id = ml.add("Bridge", 960);

    SECTION("move to new position") {
        REQUIRE(ml.move(id, 3840));
        auto* m = ml.find(id);
        REQUIRE(m != nullptr);
        REQUIRE(m->ppqn == 3840);
    }

    SECTION("move non-existent") {
        REQUIRE_FALSE(ml.move(MarkerID{999}, 960));
    }
}

TEST_CASE("MarkerList — rename markers", "[model][markers]") {
    MarkerList ml;
    auto id = ml.add("Old", 0);

    SECTION("rename") {
        REQUIRE(ml.rename(id, "New"));
        auto* m = ml.find(id);
        REQUIRE(m->name == "New");
    }

    SECTION("rename non-existent") {
        REQUIRE_FALSE(ml.rename(MarkerID{999}, "X"));
    }
}

TEST_CASE("MarkerList — find_at exact position", "[model][markers]") {
    MarkerList ml;
    ml.add("Start", 0);
    ml.add("End", 3840);

    SECTION("find at exact PPQN") {
        auto* m = ml.find_at(0);
        REQUIRE(m != nullptr);
        REQUIRE(m->name == "Start");

        m = ml.find_at(3840);
        REQUIRE(m != nullptr);
        REQUIRE(m->name == "End");
    }

    SECTION("find_at no match") {
        REQUIRE(ml.find_at(960) == nullptr);
    }
}

TEST_CASE("MarkerList — in_range queries", "[model][markers]") {
    MarkerList ml;
    ml.add("M1", 0);
    ml.add("M2", 960);
    ml.add("M3", 3840);
    ml.add("M4", 4800);

    SECTION("range covering some markers") {
        auto range = ml.in_range(960, 4000);
        REQUIRE(range.size() == 2);
        REQUIRE(range[0].name == "M2");
        REQUIRE(range[1].name == "M3");
    }

    SECTION("range before all") {
        auto range = ml.in_range(5000, 9999);
        REQUIRE(range.empty());
    }

    SECTION("range covering all") {
        auto range = ml.in_range(0, 9999);
        REQUIRE(range.size() == 4);
    }

    SECTION("empty range") {
        auto range = ml.in_range(100, 100);
        REQUIRE(range.empty());
    }
}

TEST_CASE("MarkerList — all returns sorted copy", "[model][markers]") {
    MarkerList ml;
    ml.add("C", 3840);
    ml.add("A", 0);
    ml.add("B", 960);

    auto all = ml.all();
    REQUIRE(all.size() == 3);
    REQUIRE(all[0].ppqn == 0);
    REQUIRE(all[1].ppqn == 960);
    REQUIRE(all[2].ppqn == 3840);
}
