#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "project/project_manager.h"
#include "model/track.h"

using namespace aria;

TEST_CASE("ProjectManager — project lifecycle", "[project]") {
    ProjectManager pm;

    SECTION("create project") {
        auto id = pm.create("Test", "/tmp/test.aria");
        REQUIRE(id.value != 0);
        REQUIRE(pm.project_count() == 1);
        REQUIRE(pm.active_project() == id);
    }

    SECTION("open project") {
        auto id = pm.open("/tmp/my_song.aria");
        REQUIRE(id.value != 0);
        REQUIRE(pm.project_count() == 1);
    }

    SECTION("save project") {
        auto id = pm.create("Test", "/tmp/test.aria");
        REQUIRE(pm.save(id));
    }

    SECTION("save non-existent project") {
        REQUIRE_FALSE(pm.save(ProjectID{999}));
    }

    SECTION("close project") {
        auto id = pm.create("Test", "/tmp/test.aria");
        REQUIRE(pm.close(id));
        REQUIRE(pm.project_count() == 0);
    }

    SECTION("close non-existent") {
        REQUIRE_FALSE(pm.close(ProjectID{999}));
    }

    SECTION("multiple projects") {
        auto id1 = pm.create("P1", "/tmp/p1.aria");
        auto id2 = pm.create("P2", "/tmp/p2.aria");
        REQUIRE(pm.project_count() == 2);
        REQUIRE(pm.active_project() == id2);  // last created
    }
}

TEST_CASE("ProjectManager — track management", "[project]") {
    ProjectManager pm;
    auto proj = pm.create("Test", "/tmp/test.aria");

    SECTION("create track") {
        auto track_id = pm.create_track(proj, TrackType::Audio, "Kick");
        REQUIRE(track_id.value != 0);
    }

    SECTION("get track by ID") {
        auto track_id = pm.create_track(proj, TrackType::MIDI, "Synth");
        auto* track = pm.get_track(track_id);
        REQUIRE(track != nullptr);
        REQUIRE(track->name() == "Synth");
        REQUIRE(track->type() == TrackType::MIDI);
    }

    SECTION("delete track") {
        auto track_id = pm.create_track(proj, TrackType::Audio, "Vox");
        REQUIRE(pm.delete_track(proj, track_id));
        REQUIRE(pm.get_track(track_id) == nullptr);
    }

    SECTION("get non-existent track") {
        REQUIRE(pm.get_track(TrackID{999}) == nullptr);
    }

    SECTION("create track in non-existent project") {
        auto track_id = pm.create_track(ProjectID{999}, TrackType::Audio, "X");
        REQUIRE(track_id.value == 0);  // invalid
    }
}

TEST_CASE("ProjectManager — clip management", "[project]") {
    ProjectManager pm;
    auto proj = pm.create("Test", "/tmp/test.aria");

    SECTION("create MIDI clip") {
        auto track = pm.create_track(proj, TrackType::MIDI, "Keys");
        auto clip = pm.create_midi_clip(track, 0, 960 * 4);
        REQUIRE(clip.value != 0);
    }

    SECTION("create audio clip") {
        auto track = pm.create_track(proj, TrackType::Audio, "Vox");
        auto clip = pm.create_audio_clip(track, 960, "vox_take1.wav");
        REQUIRE(clip.value != 0);
    }

    SECTION("create clip on non-existent track") {
        auto clip = pm.create_midi_clip(TrackID{999}, 0, 960);
        REQUIRE(clip.value == 0);  // invalid
    }
}

TEST_CASE("ProjectManager — undo integration", "[project]") {
    ProjectManager pm;
    auto proj = pm.create("Test", "/tmp/test.aria");

    SECTION("undo stack exists") {
        auto& stack = pm.undo(proj);
        REQUIRE(stack.undo_count() == 0);
    }

    SECTION("create track pushes undo") {
        pm.create_track(proj, TrackType::Audio, "Kick");
        auto& stack = pm.undo(proj);
        REQUIRE(stack.undo_count() > 0);
    }
}

TEST_CASE("ProjectManager — arrangement access", "[project]") {
    ProjectManager pm;
    auto proj = pm.create("Test", "/tmp/test.aria");

    SECTION("get arrangement") {
        auto& arr = pm.get_arrangement(proj);
        REQUIRE(arr.length() == 960 * 64);  // default 16 bars
    }

    SECTION("arrangement mutation persists") {
        auto& arr = pm.get_arrangement(proj);
        arr.set_length(960 * 128);
        REQUIRE(pm.get_arrangement(proj).length() == 960 * 128);
    }

    SECTION("set active project") {
        auto proj2 = pm.create("P2", "/tmp/p2.aria");
        pm.set_active_project(proj);
        REQUIRE(pm.active_project() == proj);
    }
}
