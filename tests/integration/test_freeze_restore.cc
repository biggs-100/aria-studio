// Integration Test: Freeze → Unfreeze Clip Restoration
//
// Verifies that freeze stores a frozen clip on the track, and
// unfreeze restores the track to its original state with clips intact.
//
// PR 6 — Task 6.6: Integration tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "model/freeze_manager.h"
#include "model/track.h"
#include "model/track_types.h"
#include "model/audio_clip.h"
#include "model/clip.h"
#include "model/types.h"
#include "project/project_manager.h"
#include "audio/audio_engine.h"
#include "audio/export/offline_renderer.h"

#include <memory>

using namespace aria;

// ─── Freeze → Unfreeze lifecycle ─────────────────────────────────
//
// AudioEngine has large internal buffers so it MUST be heap-allocated
// to avoid stack overflow. Same pattern as the FreezeTestFixture in
// test_freeze_manager.cc.

TEST_CASE("Integration: Freeze → unfreeze preserves arrangement clips",
          "[integration][freeze][restore]") {
    ProjectManager pm;
    ProjectID proj = pm.create("FreezeTest", "/tmp/test.aria");
    TrackID tid = pm.create_audio_track(proj, "Guitar");

    auto engine = std::make_unique<AudioEngine>();
    REQUIRE(engine->init(48000, 256));
    engine->add_track();

    OfflineRenderer renderer;
    FreezeManager manager(pm, *engine, renderer);

    SECTION("track has original clips before freeze") {
        Track* track = pm.get_track(tid);
        REQUIRE(track != nullptr);

        auto orig_clip = std::make_shared<AudioClip>();
        orig_clip->set_length(960);
        orig_clip->set_name("Original");
        track->add_clip(orig_clip, 0);

        REQUIRE(track->clip_at(480) != nullptr);
        REQUIRE_FALSE(track->is_frozen());
    }

    SECTION("freeze stores frozen clip, unfreeze clears it") {
        Track* track = pm.get_track(tid);
        REQUIRE(track != nullptr);

        // Add a clip first
        auto orig_clip = std::make_shared<AudioClip>();
        orig_clip->set_length(960);
        orig_clip->set_name("Original");
        track->add_clip(orig_clip, 0);

        // Freeze
        manager.freeze_track(tid);
        REQUIRE(track->is_frozen());
        REQUIRE(track->frozen_clip() != nullptr);

        // Unfreeze
        manager.unfreeze_track(tid);
        REQUIRE_FALSE(track->is_frozen());
        REQUIRE(track->frozen_clip() == nullptr);
    }

    SECTION("original clips remain after freeze → unfreeze cycle") {
        Track* track = pm.get_track(tid);
        REQUIRE(track != nullptr);

        auto orig_clip = std::make_shared<AudioClip>();
        orig_clip->set_length(960);
        orig_clip->set_name("Original");
        track->add_clip(orig_clip, 0);

        REQUIRE(track->clip_at(480) != nullptr);
        REQUIRE(track->clips().size() == 1);

        // Freeze
        manager.freeze_track(tid);
        REQUIRE(track->is_frozen());

        // Unfreeze
        manager.unfreeze_track(tid);
        REQUIRE_FALSE(track->is_frozen());

        // Original clip should still be there
        REQUIRE(track->clip_at(480) != nullptr);
        REQUIRE(track->clips().size() == 1);
    }

    SECTION("freeze then unfreeze allows adding new clips") {
        Track* track = pm.get_track(tid);
        REQUIRE(track != nullptr);

        auto orig = std::make_shared<AudioClip>();
        orig->set_length(960);
        track->add_clip(orig, 0);

        // Freeze → unfreeze
        manager.freeze_track(tid);
        manager.unfreeze_track(tid);

        // Should be able to add clips again
        auto new_clip = std::make_shared<AudioClip>();
        new_clip->set_length(480);
        bool added = track->add_clip(new_clip, 960);
        REQUIRE(added);
        REQUIRE(track->clips().size() == 2);
    }

    SECTION("freeze prevents add_clip, unfreeze allows it") {
        Track* track = pm.get_track(tid);
        REQUIRE(track != nullptr);

        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        track->add_clip(clip, 0);

        // Freeze — should block new clips
        manager.freeze_track(tid);
        auto blocked = std::make_shared<AudioClip>();
        blocked->set_length(480);
        REQUIRE_FALSE(track->add_clip(blocked, 960));

        // Unfreeze — should allow
        manager.unfreeze_track(tid);
        auto after = std::make_shared<AudioClip>();
        after->set_length(480);
        REQUIRE(track->add_clip(after, 960));
    }
}
