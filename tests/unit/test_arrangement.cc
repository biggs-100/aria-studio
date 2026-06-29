#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "project/arrangement.h"
#include "model/clip.h"
#include "model/marker.h"
#include "model/track.h"

#include <memory>

using namespace aria;

TEST_CASE("Arrangement — track ordering", "[project][arrangement]") {
    Arrangement arr;

    SECTION("empty track order") {
        REQUIRE(arr.track_order().empty());
    }

    SECTION("insert tracks") {
        TrackID t1{1}, t2{2}, t3{3};

        arr.insert_track(t1, 0);
        arr.insert_track(t2, 1);
        arr.insert_track(t3, 2);

        auto order = arr.track_order();
        REQUIRE(order.size() == 3);
        REQUIRE(order[0] == t1);
        REQUIRE(order[1] == t2);
        REQUIRE(order[2] == t3);
    }

    SECTION("insert at specific index") {
        TrackID t1{1}, t2{2}, t3{3};

        arr.insert_track(t1, 0);
        arr.insert_track(t3, 1);
        arr.insert_track(t2, 1);  // insert between t1 and t3

        auto order = arr.track_order();
        REQUIRE(order.size() == 3);
        REQUIRE(order[0] == t1);
        REQUIRE(order[1] == t2);
        REQUIRE(order[2] == t3);
    }

    SECTION("move track") {
        TrackID t1{1}, t2{2}, t3{3};
        arr.insert_track(t1, 0);
        arr.insert_track(t2, 1);
        arr.insert_track(t3, 2);

        arr.move_track(t3, 0);  // move t3 to front

        auto order = arr.track_order();
        REQUIRE(order[0] == t3);
        REQUIRE(order[1] == t1);
        REQUIRE(order[2] == t2);
    }

    SECTION("remove track") {
        TrackID t1{1}, t2{2}, t3{3};
        arr.insert_track(t1, 0);
        arr.insert_track(t2, 1);
        arr.insert_track(t3, 2);

        arr.remove_track(t2);

        auto order = arr.track_order();
        REQUIRE(order.size() == 2);
        REQUIRE(order[0] == t1);
        REQUIRE(order[1] == t3);
    }
}

TEST_CASE("Arrangement — project length", "[project][arrangement]") {
    Arrangement arr;

    SECTION("default length is 16 bars") {
        REQUIRE(arr.length() == 960 * 64);
    }

    SECTION("set length") {
        arr.set_length(960 * 128);
        REQUIRE(arr.length() == 960 * 128);
    }
}

TEST_CASE("Arrangement — loop range", "[project][arrangement]") {
    Arrangement arr;

    SECTION("default state") {
        REQUIRE_FALSE(arr.loop_enabled());
        auto [start, end] = arr.loop_range();
        REQUIRE(start == 0);
        REQUIRE(end == 960 * 16);
    }

    SECTION("set loop range") {
        arr.set_loop_range(960, 960 * 8);
        auto [start, end] = arr.loop_range();
        REQUIRE(start == 960);
        REQUIRE(end == 960 * 8);
    }

    SECTION("enable/disable loop") {
        arr.set_loop_enabled(true);
        REQUIRE(arr.loop_enabled());
        arr.set_loop_enabled(false);
        REQUIRE_FALSE(arr.loop_enabled());
    }
}

TEST_CASE("Arrangement — markers access", "[project][arrangement]") {
    Arrangement arr;

    SECTION("add marker via arrangement") {
        auto id = arr.markers().add("Test", 960);
        REQUIRE(arr.markers().count() == 1);
        REQUIRE(arr.markers().find(id) != nullptr);
    }
}

TEST_CASE("Arrangement — tempo track access", "[project][arrangement]") {
    Arrangement arr;

    SECTION("default BPM") {
        REQUIRE(arr.tempo_track().bpm_at(0) == Catch::Approx(120.0));
    }

    SECTION("add tempo event via arrangement") {
        arr.tempo_track().add_tempo_event(0, 140.0);
        REQUIRE(arr.tempo_track().bpm_at(0) == Catch::Approx(140.0));
    }

    SECTION("time signature via arrangement") {
        arr.tempo_track().add_time_sig_event(0, 3, 4);
        auto ts = arr.tempo_track().time_sig_at(0);
        REQUIRE(ts.numerator == 3);
    }
}

// ─── Clip_at ─────────────────────────────────────────────────────

// Helper concrete Clip subclass for testing
class TestClip : public Clip {
public:
    TestClip() : Clip() {}
    explicit TestClip(uint64_t len) : Clip() { set_length(len); }
};

TEST_CASE("Track — clip_at position query", "[model][track][clip]") {
    Track track(TrackType::Audio);

    SECTION("no clips returns nullptr") {
        REQUIRE(track.clip_at(0) == nullptr);
        REQUIRE(track.clip_at(960) == nullptr);
    }

    SECTION("returns clip at exact position") {
        auto clip = std::make_shared<TestClip>(960);
        track.add_clip(clip, 0);

        auto* found = track.clip_at(0);
        REQUIRE(found != nullptr);
        REQUIRE(found->id() == clip->id());
    }

    SECTION("returns clip within range") {
        auto clip = std::make_shared<TestClip>(960);
        track.add_clip(clip, 0);

        auto* found = track.clip_at(480);
        REQUIRE(found != nullptr);
        REQUIRE(found->id() == clip->id());
    }

    SECTION("returns nullptr outside clip range") {
        auto clip = std::make_shared<TestClip>(960);
        track.add_clip(clip, 0);

        REQUIRE(track.clip_at(960) == nullptr); // at end = not within
        REQUIRE(track.clip_at(1920) == nullptr);
    }

    SECTION("multiple clips — finds correct one") {
        auto clip1 = std::make_shared<TestClip>(960);
        track.add_clip(clip1, 0);

        auto clip2 = std::make_shared<TestClip>(960);
        track.add_clip(clip2, 960);

        REQUIRE(track.clip_at(0)->id() == clip1->id());
        REQUIRE(track.clip_at(960)->id() == clip2->id());
        REQUIRE(track.clip_at(480)->id() == clip1->id());
        REQUIRE(track.clip_at(1440)->id() == clip2->id());
    }
}

// ─── Visible tracks (folded groups) ─────────────────────────────

TEST_CASE("Arrangement — visible tracks with folded groups",
          "[project][arrangement]") {
    Arrangement arr;
    TrackID t1{1}, t2{2}, t3{3}, t4{4};

    arr.insert_track(t1, 0);
    arr.insert_track(t2, 1);
    arr.insert_track(t3, 2);
    arr.insert_track(t4, 3);

    SECTION("no hidden tracks — all visible") {
        auto visible = arr.visible_track_order([](TrackID) { return false; });
        REQUIRE(visible.size() == 4);
    }

    SECTION("filters out hidden tracks") {
        auto is_hidden = [](TrackID id) { return id == TrackID{2} || id == TrackID{3}; };
        auto visible = arr.visible_track_order(is_hidden);
        REQUIRE(visible.size() == 2);
        REQUIRE(visible[0] == t1);
        REQUIRE(visible[1] == t4);
    }

    SECTION("all hidden — empty result") {
        auto is_hidden = [](TrackID) { return true; };
        auto visible = arr.visible_track_order(is_hidden);
        REQUIRE(visible.empty());
    }

    SECTION("predicate varied per call") {
        auto visible1 = arr.visible_track_order([](TrackID id) { return id == TrackID{1}; });
        REQUIRE(visible1.size() == 3);
        REQUIRE(visible1[0] == t2);

        auto visible2 = arr.visible_track_order([](TrackID) { return false; });
        REQUIRE(visible2.size() == 4);
    }
}
