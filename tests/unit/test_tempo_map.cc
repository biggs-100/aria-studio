#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "audio/tempo_map.h"

using namespace aria;

TEST_CASE("TempoMap — default BPM", "[audio][tempo]") {
    TempoMap map;

    SECTION("no events returns default") {
        REQUIRE(map.bpm_at(0) == Catch::Approx(120.0));
        REQUIRE(map.bpm_at(44100) == Catch::Approx(120.0));
        REQUIRE(map.bpm_at(1000000) == Catch::Approx(120.0));
    }

    SECTION("custom default") {
        map.set_default_bpm(140.0);
        REQUIRE(map.bpm_at(0) == Catch::Approx(140.0));
        REQUIRE(map.bpm_at(99999) == Catch::Approx(140.0));
    }
}

TEST_CASE("TempoMap — single tempo event", "[audio][tempo]") {
    TempoMap map;

    std::vector<TempoEvent> events = {
        {0, 120.0}
    };
    map.set_events(events);

    REQUIRE(map.bpm_at(0) == Catch::Approx(120.0));
    REQUIRE(map.bpm_at(44100) == Catch::Approx(120.0));
    REQUIRE(map.bpm_at(88200) == Catch::Approx(120.0));
}

TEST_CASE("TempoMap — multiple tempo changes", "[audio][tempo]") {
    TempoMap map;

    std::vector<TempoEvent> events = {
        {0,      120.0},
        {44100,  140.0},
        {88200,  130.0},
    };
    map.set_events(events);

    SECTION("before first event uses default") {
        // The first event IS at 0, so there is no "before first".
        // If we add a gap, events before first use default.
        TempoMap map2;
        std::vector<TempoEvent> ev2 = {
            {1000, 120.0}
        };
        map2.set_events(ev2);
        REQUIRE(map2.bpm_at(0) == Catch::Approx(120.0));  // default
        REQUIRE(map2.bpm_at(999) == Catch::Approx(120.0)); // default
        REQUIRE(map2.bpm_at(1000) == Catch::Approx(120.0)); // event
    }

    SECTION("at exact event positions") {
        REQUIRE(map.bpm_at(0)     == Catch::Approx(120.0));
        REQUIRE(map.bpm_at(44100) == Catch::Approx(140.0));
        REQUIRE(map.bpm_at(88200) == Catch::Approx(130.0));
    }

    SECTION("between events") {
        REQUIRE(map.bpm_at(22050) == Catch::Approx(120.0));  // still first tempo
        REQUIRE(map.bpm_at(66150) == Catch::Approx(140.0));  // between 2nd and 3rd
    }

    SECTION("after last event") {
        REQUIRE(map.bpm_at(100000) == Catch::Approx(130.0));
        REQUIRE(map.bpm_at(441000) == Catch::Approx(130.0));
    }
}

TEST_CASE("TempoMap — unsorted events are sorted", "[audio][tempo]") {
    TempoMap map;

    std::vector<TempoEvent> events = {
        {88200, 130.0},
        {0,     120.0},
        {44100, 140.0},
    };
    map.set_events(events);

    // After sorting internally: [{0, 120}, {44100, 140}, {88200, 130}]
    REQUIRE(map.bpm_at(0)     == Catch::Approx(120.0));
    REQUIRE(map.bpm_at(22050) == Catch::Approx(120.0));
    REQUIRE(map.bpm_at(44100) == Catch::Approx(140.0));
    REQUIRE(map.bpm_at(66150) == Catch::Approx(140.0));
    REQUIRE(map.bpm_at(88200) == Catch::Approx(130.0));
}

TEST_CASE("TempoMap — event count and access", "[audio][tempo]") {
    TempoMap map;

    SECTION("empty") {
        REQUIRE(map.event_count() == 0);
        REQUIRE(map.events().empty());
    }

    SECTION("with events") {
        std::vector<TempoEvent> events = {
            {0, 120.0},
            {44100, 140.0},
        };
        map.set_events(events);
        REQUIRE(map.event_count() == 2);
        REQUIRE(map.events().size() == 2);
        REQUIRE(map.events()[0].sample_position == 0);
        REQUIRE(map.events()[0].bpm == Catch::Approx(120.0));
        REQUIRE(map.events()[1].sample_position == 44100);
        REQUIRE(map.events()[1].bpm == Catch::Approx(140.0));
    }
}

TEST_CASE("TempoMap — replace events", "[audio][tempo]") {
    TempoMap map;
    map.set_events({{0, 120.0}, {44100, 140.0}});
    REQUIRE(map.event_count() == 2);

    // Replace with new events
    map.set_events({{0, 100.0}});
    REQUIRE(map.event_count() == 1);
    REQUIRE(map.bpm_at(0) == Catch::Approx(100.0));
    REQUIRE(map.bpm_at(44100) == Catch::Approx(100.0));
}
