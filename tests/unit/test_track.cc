#include <catch2/catch_test_macros.hpp>
#include "model/track.h"

using namespace aria;

TEST_CASE("Track creation and properties", "[model][track]") {
    Track track(TrackType::Audio);

    SECTION("Default state") {
        REQUIRE(track.volume() == Approx(0.0));
        REQUIRE(track.pan() == Approx(0.0));
        REQUIRE_FALSE(track.is_muted());
        REQUIRE_FALSE(track.is_soloed());
    }

    SECTION("Set volume") {
        track.set_volume(-6.0);
        REQUIRE(track.volume() == Approx(-6.0));
    }

    SECTION("Set name") {
        track.set_name("Kick");
        REQUIRE(track.name() == "Kick");
    }

    SECTION("Mute and solo") {
        track.set_muted(true);
        REQUIRE(track.is_muted());
        track.set_soloed(true);
        REQUIRE(track.is_soloed());
    }

    SECTION("Unique IDs") {
        Track other(TrackType::MIDI);
        REQUIRE(track.id() != other.id());
    }
}
