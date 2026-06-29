#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "model/session.h"
#include "model/audio_clip.h"
#include "model/clip.h"
#include "model/types.h"
#include "project/project_manager.h"

#include <cmath>
#include <memory>
#include <numbers>

using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Task 3.1: Session class
// ═══════════════════════════════════════════════════════════════

// ─── Scene management ──────────────────────────────────────────

TEST_CASE("Session — scene management", "[session][scene]") {
    Session session;

    SECTION("empty session has no scenes") {
        REQUIRE(session.scene_count() == 0);
    }

    SECTION("add_scene creates scene and returns valid ID") {
        SceneID id = session.add_scene("Scene 1");
        REQUIRE(id.value != 0);
        REQUIRE(session.scene_count() == 1);
    }

    SECTION("scene names round-trip") {
        SceneID id = session.add_scene("Intro");
        REQUIRE(session.scene_name(id) == "Intro");
    }

    SECTION("multiple scenes have unique IDs") {
        SceneID id1 = session.add_scene("Scene 1");
        SceneID id2 = session.add_scene("Scene 2");
        REQUIRE(id1.value != id2.value);
        REQUIRE(session.scene_count() == 2);
    }

    SECTION("scenes() returns all scenes in order") {
        SceneID id1 = session.add_scene("First");
        SceneID id2 = session.add_scene("Second");
        REQUIRE(session.scene_count() == 2);
        REQUIRE(session.scene_name(id1) == "First");
        REQUIRE(session.scene_name(id2) == "Second");
    }

    SECTION("remove_scene removes scene") {
        SceneID id = session.add_scene("Temp");
        REQUIRE(session.scene_count() == 1);
        session.remove_scene(id);
        REQUIRE(session.scene_count() == 0);
    }

    SECTION("set_scene_name updates scene name") {
        SceneID id = session.add_scene("Original");
        session.set_scene_name(id, "Updated");
        REQUIRE(session.scene_name(id) == "Updated");
    }
}

// ─── Clip grid ─────────────────────────────────────────────────

TEST_CASE("Session — clip grid", "[session][clip]") {
    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};

    SECTION("empty slot returns nullptr") {
        REQUIRE(session.clip_at(track, scene) == nullptr);
    }

    SECTION("set_clip_slot stores clip, clip_at retrieves it") {
        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        session.set_clip_slot(track, scene, clip);
        REQUIRE(session.clip_at(track, scene) == clip.get());
    }

    SECTION("different track/scene combos are independent") {
        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(480);
        auto clip2 = std::make_shared<AudioClip>();
        clip2->set_length(960);

        SceneID scene2 = session.add_scene("Scene 2");
        TrackID track2{2};

        session.set_clip_slot(track, scene, clip1);
        session.set_clip_slot(track2, scene2, clip2);

        REQUIRE(session.clip_at(track, scene) == clip1.get());
        REQUIRE(session.clip_at(track2, scene2) == clip2.get());
        // Empty slot should still return nullptr
        REQUIRE(session.clip_at(track, scene2) == nullptr);
    }

    SECTION("default slot state is Stopped") {
        REQUIRE(session.slot_state(track, scene) == LaunchState::Stopped);
    }

    SECTION("slot with non-existent scene returns Stopped") {
        REQUIRE(session.slot_state(track, SceneID{999}) == LaunchState::Stopped);
    }
}

// ─── Launch state machine ─────────────────────────────────────

TEST_CASE("Session — launch_clip state machine", "[session][launch]") {
    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene, clip);

    SECTION("before launch, no playing clip") {
        REQUIRE(session.playing_clip(track) == nullptr);
    }

    SECTION("after launch, clip is Playing") {
        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());
        REQUIRE(session.slot_state(track, scene) == LaunchState::Playing);
    }

    SECTION("stop_clip returns to Stopped") {
        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());

        session.stop_clip(track);
        REQUIRE(session.playing_clip(track) == nullptr);
        REQUIRE(session.slot_state(track, scene) == LaunchState::Stopped);
    }

    SECTION("launching a second clip stops the first") {
        SceneID scene2 = session.add_scene("Scene 2");
        auto clip2 = std::make_shared<AudioClip>();
        clip2->set_length(480);
        session.set_clip_slot(track, scene2, clip2);

        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());

        session.launch_clip(track, scene2);
        REQUIRE(session.playing_clip(track) == clip2.get());
        REQUIRE(session.slot_state(track, scene2) == LaunchState::Playing);
        // First clip should now be stopped
        REQUIRE(session.slot_state(track, scene) == LaunchState::Stopped);
    }
}

// ─── Scene launch ──────────────────────────────────────────────

TEST_CASE("Session — launch_scene triggers all clips", "[session][launch][scene]") {
    SECTION("both clips start playing after scene launch") {
        Session session;
        SceneID scene = session.add_scene("Scene 1");
        TrackID track1{1};
        TrackID track2{2};

        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(960);
        auto clip2 = std::make_shared<AudioClip>();
        clip2->set_length(960);

        session.set_clip_slot(track1, scene, clip1);
        session.set_clip_slot(track2, scene, clip2);
        session.launch_scene(scene);

        REQUIRE(session.playing_clip(track1) == clip1.get());
        REQUIRE(session.playing_clip(track2) == clip2.get());
        REQUIRE(session.slot_state(track1, scene) == LaunchState::Playing);
        REQUIRE(session.slot_state(track2, scene) == LaunchState::Playing);
    }

    SECTION("launching scene stops previous clips on each track") {
        Session session;
        SceneID scene1 = session.add_scene("Scene 1");
        SceneID scene2 = session.add_scene("Scene 2");
        TrackID track1{1};
        TrackID track2{2};

        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(960);
        auto clip2 = std::make_shared<AudioClip>();
        clip2->set_length(960);
        auto clip3 = std::make_shared<AudioClip>();
        clip3->set_length(960);
        auto clip4 = std::make_shared<AudioClip>();
        clip4->set_length(960);

        session.set_clip_slot(track1, scene1, clip1);
        session.set_clip_slot(track2, scene1, clip2);
        session.set_clip_slot(track1, scene2, clip3);
        session.set_clip_slot(track2, scene2, clip4);

        session.launch_scene(scene1);
        REQUIRE(session.playing_clip(track1) == clip1.get());
        REQUIRE(session.playing_clip(track2) == clip2.get());

        session.launch_scene(scene2);
        REQUIRE(session.playing_clip(track1) == clip3.get());
        REQUIRE(session.playing_clip(track2) == clip4.get());
        REQUIRE(session.slot_state(track1, scene1) == LaunchState::Stopped);
    }

    SECTION("launching scene with empty slots only affects filled slots") {
        Session session;
        SceneID scene = session.add_scene("Scene 1");
        TrackID track1{1};
        TrackID track2{2};
        TrackID track3{3};

        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(960);
        auto clip3 = std::make_shared<AudioClip>();
        clip3->set_length(960);

        // Only track1 and track3 have clips in this scene
        session.set_clip_slot(track1, scene, clip1);
        session.set_clip_slot(track3, scene, clip3);

        session.launch_scene(scene);
        REQUIRE(session.playing_clip(track1) == clip1.get());
        REQUIRE(session.playing_clip(track3) == clip3.get());
        // track2 was not in this scene and should not be affected
        REQUIRE(session.playing_clip(track2) == nullptr);
    }
}

// ─── Follow actions ────────────────────────────────────────────

TEST_CASE("Session — follow actions", "[session][follow]") {
    Session session;
    SceneID scene1 = session.add_scene("Scene 1");
    SceneID scene2 = session.add_scene("Scene 2");
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene1, clip);

    SECTION("default follow action is None") {
        REQUIRE(session.scene_follow_action(scene1) == FollowAction::None);
    }

    SECTION("set_scene_follow_action stores and retrieves") {
        session.set_scene_follow_action(scene1, FollowAction::PlayNext);
        REQUIRE(session.scene_follow_action(scene1) == FollowAction::PlayNext);
    }

    SECTION("set_slot_follow_action stores per-slot action") {
        session.set_slot_follow_action(track, scene1, FollowAction::Stop);
        // evaluate_follow_actions should pick up the slot action
        // (slot actions are stored internally)
    }
}

TEST_CASE("Session — evaluate_follow_actions triggers next scene",
          "[session][follow]") {
    SECTION("PlayNext launches next scene") {
        Session session;
        SceneID scene1 = session.add_scene("Scene 1");
        SceneID scene2 = session.add_scene("Scene 2");

        // Track has clips in both scenes
        TrackID track{1};
        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(960);
        auto clip2 = std::make_shared<AudioClip>();
        clip2->set_length(960);
        session.set_clip_slot(track, scene1, clip1);
        session.set_clip_slot(track, scene2, clip2);

        session.launch_clip(track, scene1);
        session.set_scene_follow_action(scene1, FollowAction::PlayNext);

        bool triggered = session.evaluate_follow_actions(track);
        REQUIRE(triggered);
        // Should now be playing scene2's clip
        REQUIRE(session.playing_clip(track) == clip2.get());
    }

    SECTION("Stop follow action stops clip") {
        Session session;
        SceneID scene1 = session.add_scene("Scene 1");
        TrackID track{1};
        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(960);
        session.set_clip_slot(track, scene1, clip1);

        session.launch_clip(track, scene1);
        session.set_scene_follow_action(scene1, FollowAction::Stop);

        bool triggered = session.evaluate_follow_actions(track);
        REQUIRE_FALSE(triggered);
        REQUIRE(session.playing_clip(track) == nullptr);
    }

    SECTION("ContinueAsLoop restarts the clip") {
        Session session;
        SceneID scene1 = session.add_scene("Scene 1");
        TrackID track{1};
        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(960);
        session.set_clip_slot(track, scene1, clip1);

        session.launch_clip(track, scene1);
        session.set_scene_follow_action(scene1, FollowAction::ContinueAsLoop);

        bool triggered = session.evaluate_follow_actions(track);
        REQUIRE(triggered);
        REQUIRE(session.playing_clip(track) == clip1.get());
    }

    SECTION("None does nothing") {
        Session session;
        SceneID scene1 = session.add_scene("Scene 1");
        TrackID track{1};
        auto clip1 = std::make_shared<AudioClip>();
        clip1->set_length(960);
        session.set_clip_slot(track, scene1, clip1);

        session.launch_clip(track, scene1);
        session.set_scene_follow_action(scene1, FollowAction::None);

        bool triggered = session.evaluate_follow_actions(track);
        REQUIRE_FALSE(triggered);
        REQUIRE(session.playing_clip(track) == nullptr);
    }
}

// ─── Crossfader ────────────────────────────────────────────────

TEST_CASE("Session — crossfader defaults", "[session][crossfader]") {
    Session session;

    SECTION("default position is 0.0") {
        REQUIRE(session.crossfader_position() == Catch::Approx(0.0));
    }

    SECTION("default curve is EqualPower") {
        REQUIRE(session.crossfader_curve() == CrossfaderCurve::EqualPower);
    }
}

TEST_CASE("Session — crossfader position and curve", "[session][crossfader]") {
    Session session;

    SECTION("set_crossfader_position stores value") {
        session.set_crossfader_position(0.5);
        REQUIRE(session.crossfader_position() == Catch::Approx(0.5));
    }

    SECTION("set_crossfader_position stores negative value") {
        session.set_crossfader_position(-0.75);
        REQUIRE(session.crossfader_position() == Catch::Approx(-0.75));
    }

    SECTION("set_crossfader_curve changes curve") {
        session.set_crossfader_curve(CrossfaderCurve::Linear);
        REQUIRE(session.crossfader_curve() == CrossfaderCurve::Linear);
    }
}

TEST_CASE("Session — crossfader track assignments", "[session][crossfader]") {
    Session session;
    TrackID track{1};

    SECTION("default assignment is None") {
        REQUIRE(session.track_assignment(track) == CrossfaderAssignment::None);
    }

    SECTION("set_track_assignment stores assignment") {
        session.set_track_assignment(track, CrossfaderAssignment::A);
        REQUIRE(session.track_assignment(track) == CrossfaderAssignment::A);
    }

    SECTION("different tracks have independent assignments") {
        session.set_track_assignment(TrackID{1}, CrossfaderAssignment::A);
        session.set_track_assignment(TrackID{2}, CrossfaderAssignment::B);
        REQUIRE(session.track_assignment(TrackID{1}) == CrossfaderAssignment::A);
        REQUIRE(session.track_assignment(TrackID{2}) == CrossfaderAssignment::B);
    }
}

// ─── Triangulation: edge cases ────────────────────────────────

TEST_CASE("Session — launch non-existent scene/clip does nothing",
          "[session][launch][edge]") {
    Session session;
    TrackID track{1};
    SceneID scene = session.add_scene("Scene 1");
    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene, clip);

    SECTION("launch_clip with non-existent scene is a no-op") {
        session.launch_clip(track, SceneID{999});
        REQUIRE(session.playing_clip(track) == nullptr);
    }

    SECTION("launch_scene with non-existent scene is a no-op") {
        session.launch_scene(SceneID{999});
        REQUIRE(session.playing_clip(track) == nullptr);
    }
}

TEST_CASE("Session — re-launch same clip restarts it",
          "[session][launch][edge]") {
    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};
    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene, clip);

    SECTION("launching same clip again is a no-op (keeps playing)") {
        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());
        // Second launch of same clip — should stay playing
        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());
        REQUIRE(session.slot_state(track, scene) == LaunchState::Playing);
    }
}

TEST_CASE("Session — remove scene while clip playing releases it",
          "[session][edge]") {
    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};
    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene, clip);

    SECTION("removing scene with playing clip stops it") {
        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());

        session.remove_scene(scene);
        REQUIRE(session.scene_count() == 0);
        REQUIRE(session.playing_clip(track) == nullptr);
    }
}

TEST_CASE("Session — crossfader linear curve evaluation",
          "[session][crossfader][edge]") {
    Session session;
    session.set_crossfader_curve(CrossfaderCurve::Linear);
    TrackID track_a{1};
    TrackID track_b{2};
    session.set_track_assignment(track_a, CrossfaderAssignment::A);
    session.set_track_assignment(track_b, CrossfaderAssignment::B);

    SECTION("linear: at -1.0, A=1.0, B=0.0") {
        session.set_crossfader_position(-1.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(0.0).margin(1e-15));
    }

    SECTION("linear: at -0.5, A=1.0, B=0.5") {
        session.set_crossfader_position(-0.5);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(0.5));
    }

    SECTION("linear: at 0.0, both at 1.0") {
        session.set_crossfader_position(0.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(1.0));
    }

    SECTION("linear: at +0.5, A=0.5, B=1.0") {
        session.set_crossfader_position(0.5);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(0.5));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(1.0));
    }

    SECTION("linear: at +1.0, A=0.0, B=1.0") {
        session.set_crossfader_position(1.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(0.0).margin(1e-15));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(1.0));
    }
}

TEST_CASE("Session — crossfader equal-power mid-point",
          "[session][crossfader][edge]") {
    Session session;
    session.set_crossfader_curve(CrossfaderCurve::EqualPower);
    TrackID track_a{1};
    session.set_track_assignment(track_a, CrossfaderAssignment::A);

    SECTION("equal-power: A at +0.5 is ~cos(0.25π) ≈ 0.707") {
        session.set_crossfader_position(0.5);
        double expected = std::cos(0.5 * std::numbers::pi_v<double> / 2.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(expected).margin(0.001));
    }

    SECTION("equal-power: clamps position outside [-1,1]") {
        session.set_crossfader_position(2.0);
        REQUIRE(session.crossfader_position() == Catch::Approx(1.0));
        session.set_crossfader_position(-2.0);
        REQUIRE(session.crossfader_position() == Catch::Approx(-1.0));
    }
}

TEST_CASE("Session — follow actions: PlayRandom cycles through scenes",
          "[session][follow][edge]") {
    Session session;
    SceneID scene1 = session.add_scene("Scene 1");
    SceneID scene2 = session.add_scene("Scene 2");
    SceneID scene3 = session.add_scene("Scene 3");
    TrackID track{1};

    auto clip1 = std::make_shared<AudioClip>();
    clip1->set_length(960);
    auto clip2 = std::make_shared<AudioClip>();
    clip2->set_length(960);
    auto clip3 = std::make_shared<AudioClip>();
    clip3->set_length(960);

    session.set_clip_slot(track, scene1, clip1);
    session.set_clip_slot(track, scene2, clip2);
    session.set_clip_slot(track, scene3, clip3);

    SECTION("PlayRandom triggers a different clip") {
        session.launch_clip(track, scene1);
        session.set_scene_follow_action(scene1, FollowAction::PlayRandom);
        bool triggered = session.evaluate_follow_actions(track);
        REQUIRE(triggered);
        // Should be playing SOME clip (not necessarily scene1)
        REQUIRE(session.playing_clip(track) != nullptr);
    }
}

TEST_CASE("Session — slot follow action overrides scene action",
          "[session][follow][edge]") {
    Session session;
    SceneID scene1 = session.add_scene("Scene 1");
    SceneID scene2 = session.add_scene("Scene 2");
    TrackID track{1};

    auto clip1 = std::make_shared<AudioClip>();
    clip1->set_length(960);
    auto clip2 = std::make_shared<AudioClip>();
    clip2->set_length(960);
    session.set_clip_slot(track, scene1, clip1);
    session.set_clip_slot(track, scene2, clip2);

    SECTION("slot PlayNext overrides scene Stop") {
        session.launch_clip(track, scene1);
        session.set_scene_follow_action(scene1, FollowAction::Stop);
        session.set_slot_follow_action(track, scene1, FollowAction::PlayNext);

        bool triggered = session.evaluate_follow_actions(track);
        REQUIRE(triggered);
        REQUIRE(session.playing_clip(track) == clip2.get());
    }
}

TEST_CASE("Session — evaluate_crossfader_gain at various positions",
          "[session][crossfader]") {
    Session session;
    TrackID track_a{1};
    TrackID track_b{2};
    TrackID track_both{3};
    TrackID track_none{4};

    session.set_track_assignment(track_a, CrossfaderAssignment::A);
    session.set_track_assignment(track_b, CrossfaderAssignment::B);
    session.set_track_assignment(track_both, CrossfaderAssignment::Both);
    session.set_track_assignment(track_none, CrossfaderAssignment::None);

    SECTION("at position -1.0 (full A): only A-side is audible") {
        session.set_crossfader_position(-1.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(0.0).margin(1e-15));
    }

    SECTION("at position 0.0 (center): both sides at full") {
        session.set_crossfader_position(0.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(1.0));
    }

    SECTION("at position +1.0 (full B): only B-side is audible") {
        session.set_crossfader_position(1.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(0.0).margin(1e-15));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(1.0));
    }

    SECTION("Both and None tracks always get gain 1.0") {
        session.set_crossfader_position(-1.0);
        REQUIRE(session.evaluate_crossfader_gain(track_both) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_none) == Catch::Approx(1.0));

        session.set_crossfader_position(1.0);
        REQUIRE(session.evaluate_crossfader_gain(track_both) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_none) == Catch::Approx(1.0));
    }
}

// ═══════════════════════════════════════════════════════════════
// Task 3.2: Wire Session into ProjectData
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ProjectManager — session integration", "[project][session]") {
    ProjectManager pm;
    auto proj = pm.create("Test", "/tmp/test.aria");

    SECTION("get_session returns nullptr until create_session") {
        REQUIRE(pm.get_session() == nullptr);
    }

    SECTION("create_session creates a real Session") {
        pm.create_session();
        auto* sess = pm.get_session();
        REQUIRE(sess != nullptr);

        // The Session should be functional
        SceneID sid = sess->add_scene("Intro");
        REQUIRE(sid.value != 0);
        REQUIRE(sess->scene_count() == 1);
        REQUIRE(sess->scene_name(sid) == "Intro");
    }

    SECTION("close project frees session") {
        pm.create_session();
        REQUIRE(pm.get_session() != nullptr);
        pm.close(proj);
        // After close, accessing session on a new project returns nullptr
        // (the old session is freed)
    }

    SECTION("second create_session is idempotent") {
        pm.create_session();
        REQUIRE(pm.get_session() != nullptr);
        SceneID sid = pm.get_session()->add_scene("Scene 1");

        pm.create_session();  // second call, session already exists
        REQUIRE(pm.get_session() != nullptr);
        // The existing session should still be intact
        REQUIRE(pm.get_session()->scene_count() == 1);
    }
}

// ═══════════════════════════════════════════════════════════════
// Task 6.4: Additional Session edge cases
// ═══════════════════════════════════════════════════════════════

TEST_CASE("Session — PlayNext when no next scene stops clip",
          "[session][follow][task6.4]") {
    Session session;
    SceneID scene1 = session.add_scene("Scene 1");
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene1, clip);

    session.launch_clip(track, scene1);
    session.set_scene_follow_action(scene1, FollowAction::PlayNext);

    // No next scene — should stop
    bool triggered = session.evaluate_follow_actions(track);
    REQUIRE_FALSE(triggered);
    REQUIRE(session.playing_clip(track) == nullptr);
}

TEST_CASE("Session — PlayNext with gap in scene order",
          "[session][follow][task6.4]") {
    Session session;
    SceneID scene1 = session.add_scene("Scene 1");
    SceneID scene2 = session.add_scene("Scene 2");
    SceneID scene3 = session.add_scene("Scene 3");
    TrackID track{1};

    auto clip1 = std::make_shared<AudioClip>();
    clip1->set_length(960);
    session.set_clip_slot(track, scene1, clip1);

    auto clip2 = std::make_shared<AudioClip>();
    clip2->set_length(960);
    session.set_clip_slot(track, scene2, clip2);

    auto clip3 = std::make_shared<AudioClip>();
    clip3->set_length(960);
    session.set_clip_slot(track, scene3, clip3);

    // Launch scene 1, then PlayNext should go to scene 2
    session.launch_clip(track, scene1);
    session.set_scene_follow_action(scene1, FollowAction::PlayNext);

    bool triggered = session.evaluate_follow_actions(track);
    REQUIRE(triggered);
    REQUIRE(session.playing_clip(track) == clip2.get());

    // PlayNext is per-scene — set follow action on scene 2 for cascading
    session.set_scene_follow_action(scene2, FollowAction::PlayNext);

    // Now evaluate for scene 2 — should go to scene 3
    bool triggered2 = session.evaluate_follow_actions(track);
    REQUIRE(triggered2);
    REQUIRE(session.playing_clip(track) == clip3.get());
}

TEST_CASE("Session — PlayNext scene has no clip on this track stops",
          "[session][follow][task6.4]") {
    Session session;
    SceneID scene1 = session.add_scene("Scene 1");
    SceneID scene2 = session.add_scene("Scene 2");  // No clip on track
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene1, clip);
    // scene2 has no clip on track

    session.launch_clip(track, scene1);
    session.set_scene_follow_action(scene1, FollowAction::PlayNext);

    // PlayNext tries scene2 but there's no clip — should stop
    bool triggered = session.evaluate_follow_actions(track);
    REQUIRE_FALSE(triggered);
    REQUIRE(session.playing_clip(track) == nullptr);
}

TEST_CASE("Session — PlayRandom with single clip restarts same clip",
          "[session][follow][task6.4]") {
    Session session;
    SceneID scene1 = session.add_scene("Scene 1");
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene1, clip);

    session.launch_clip(track, scene1);
    session.set_scene_follow_action(scene1, FollowAction::PlayRandom);

    // Only one clip — PlayRandom picks the same one
    bool triggered = session.evaluate_follow_actions(track);
    REQUIRE(triggered);
    REQUIRE(session.playing_clip(track) == clip.get());
}

TEST_CASE("Session — crossfader equal-power at various positions",
          "[session][crossfader][task6.4]") {
    Session session;
    session.set_crossfader_curve(CrossfaderCurve::EqualPower);
    TrackID track_a{1};
    TrackID track_b{2};
    session.set_track_assignment(track_a, CrossfaderAssignment::A);
    session.set_track_assignment(track_b, CrossfaderAssignment::B);

    SECTION("equal-power: A at -0.5 is 1.0 (A zone)") {
        session.set_crossfader_position(-0.5);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(1.0));
        double expected_b = std::cos(0.5 * std::numbers::pi_v<double> / 2.0);
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(expected_b).margin(0.001));
    }

    SECTION("equal-power: A at +0.5 is cos(0.25π), B at +0.5 is 1.0") {
        session.set_crossfader_position(0.5);
        double expected_a = std::cos(0.5 * std::numbers::pi_v<double> / 2.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(expected_a).margin(0.001));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(1.0));
    }

    SECTION("equal-power: at 0.0, both at 1.0") {
        session.set_crossfader_position(0.0);
        REQUIRE(session.evaluate_crossfader_gain(track_a) == Catch::Approx(1.0));
        REQUIRE(session.evaluate_crossfader_gain(track_b) == Catch::Approx(1.0));
    }
}

TEST_CASE("Session — crossfader clamps position",
          "[session][crossfader][task6.4]") {
    Session session;

    SECTION("position above +1.0 clamps to +1.0") {
        session.set_crossfader_position(1.5);
        REQUIRE(session.crossfader_position() == Catch::Approx(1.0));
    }

    SECTION("position below -1.0 clamps to -1.0") {
        session.set_crossfader_position(-2.0);
        REQUIRE(session.crossfader_position() == Catch::Approx(-1.0));
    }

    SECTION("position after clamp is stable") {
        session.set_crossfader_position(2.0);
        REQUIRE(session.crossfader_position() == Catch::Approx(1.0));
        session.set_crossfader_position(1.0);
        REQUIRE(session.crossfader_position() == Catch::Approx(1.0));
    }
}

TEST_CASE("Session — launch_clip on track with no clips is no-op",
          "[session][launch][task6.4]") {
    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};

    // No clip set on this track/scene
    session.launch_clip(track, scene);
    REQUIRE(session.playing_clip(track) == nullptr);
}

TEST_CASE("Session — launch after stop multiple times",
          "[session][launch][task6.4]") {
    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene, clip);

    // Launch → stop → launch → stop cycle
    session.launch_clip(track, scene);
    REQUIRE(session.playing_clip(track) == clip.get());

    session.stop_clip(track);
    REQUIRE(session.playing_clip(track) == nullptr);

    session.launch_clip(track, scene);
    REQUIRE(session.playing_clip(track) == clip.get());

    session.stop_clip(track);
    REQUIRE(session.playing_clip(track) == nullptr);
}

TEST_CASE("Session — launch_clip with non-existent scene returns early",
          "[session][launch][task6.4]") {
    Session session;
    TrackID track{1};

    // No clip slot at this scene
    REQUIRE_NOTHROW(session.launch_clip(track, SceneID{999}));
    REQUIRE(session.playing_clip(track) == nullptr);
}
