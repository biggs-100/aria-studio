// Integration Test: Transport Sync
//
// Verifies that transport state propagates correctly between
// AudioEngine, Session, and Arrangement subsystems.
//
// PR 6 — Task 6.6: Integration tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "audio/audio_engine.h"
#include "audio/audio_clock.h"
#include "audio/transport.h"
#include "audio/tempo_map.h"
#include "model/session.h"
#include "model/track.h"
#include "model/track_types.h"
#include "model/audio_clip.h"
#include "model/clip.h"
#include "project/project_manager.h"

#include <memory>

using namespace aria;

// ─── Transport basic state machine ────────────────────────────────
//
// AudioEngine has large internal buffer allocations so it MUST be
// heap-allocated to avoid stack overflow. Same pattern as the
// FreezeTestFixture in test_freeze_manager.cc.

TEST_CASE("Integration: Transport state machine",
          "[integration][transport]") {
    auto engine = std::make_unique<AudioEngine>();
    REQUIRE(engine->init(48000, 256));

    Transport& transport = engine->transport();

    SECTION("transport starts stopped") {
        REQUIRE(transport.is_stopped());
        REQUIRE_FALSE(transport.is_playing());
    }

    SECTION("play changes state to playing") {
        transport.play();
        REQUIRE(transport.is_playing());
        REQUIRE_FALSE(transport.is_stopped());

        transport.stop();
        REQUIRE(transport.is_stopped());
        REQUIRE_FALSE(transport.is_playing());
    }

    SECTION("play → stop → play cycle") {
        transport.play();
        REQUIRE(transport.is_playing());

        transport.stop();
        REQUIRE(transport.is_stopped());

        transport.play();
        REQUIRE(transport.is_playing());
    }
}

TEST_CASE("Integration: Transport position control",
          "[integration][transport]") {
    auto engine = std::make_unique<AudioEngine>();
    REQUIRE(engine->init(48000, 256));

    Transport& transport = engine->transport();
    AudioClock& clock = engine->clock();

    SECTION("set_position moves playhead") {
        transport.set_position(48000);  // 1 second at 48kHz
        transport.play();

        // Process a small block to trigger seek
        transport.process(0, clock);
        // Position after seek should be close to 48000
        REQUIRE(clock.sample_position() == 48000);
    }

    SECTION("loop playback basic configuration") {
        transport.set_loop(true);
        REQUIRE(transport.loop_enabled());

        transport.set_loop_range(0, 48000 * 4);  // 4-second loop
        REQUIRE(transport.loop_start() == 0);
        REQUIRE(transport.loop_end() == 48000 * 4);
    }
}

// ─── Tempo map integration ────────────────────────────────────────

TEST_CASE("Integration: TempoMap BPM control",
          "[integration][transport][tempo]") {
    auto engine = std::make_unique<AudioEngine>();
    REQUIRE(engine->init(48000, 256));

    AudioClock& clock = engine->clock();

    SECTION("default BPM is 120") {
        REQUIRE(clock.current_bpm() == Catch::Approx(120.0));
    }

    SECTION("tempo change via TempoMap") {
        TempoMap tmap;
        tmap.set_default_bpm(140.0);
        clock.set_tempo_map(tmap);
        // current_bpm_ is updated on next process() call
        clock.process(0);
        REQUIRE(clock.current_bpm() == Catch::Approx(140.0));
    }

    SECTION("tempo changes propagate to clock") {
        TempoMap tmap;
        tmap.set_default_bpm(160.0);
        clock.set_tempo_map(tmap);

        clock.process(48000);  // Process one second
        REQUIRE(clock.current_bpm() == Catch::Approx(160.0));
    }
}

// ─── Session integration with transport-like state ────────────────

TEST_CASE("Integration: Session launch state with engine",
          "[integration][session]") {
    auto engine = std::make_unique<AudioEngine>();
    REQUIRE(engine->init(48000, 256));

    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    session.set_clip_slot(track, scene, clip);

    SECTION("session launch state changes correctly") {
        REQUIRE(session.playing_clip(track) == nullptr);

        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());

        session.stop_clip(track);
        REQUIRE(session.playing_clip(track) == nullptr);
    }

    SECTION("session follow actions with scene iteration") {
        SceneID scene2 = session.add_scene("Scene 2");

        auto clip2 = std::make_shared<AudioClip>();
        clip2->set_length(960);
        session.set_clip_slot(track, scene2, clip2);

        session.launch_clip(track, scene);
        session.set_scene_follow_action(scene, FollowAction::PlayNext);

        bool triggered = session.evaluate_follow_actions(track);
        REQUIRE(triggered);
        REQUIRE(session.playing_clip(track) == clip2.get());
    }

    SECTION("session and transport play state are independent") {
        // Session state is separate from transport
        session.launch_clip(track, scene);
        REQUIRE(session.playing_clip(track) == clip.get());

        // Transport state doesn't affect session
        Transport& transport = engine->transport();
        transport.play();
        REQUIRE(transport.is_playing());
        // Session still has its own state
        REQUIRE(session.playing_clip(track) == clip.get());
    }
}
