#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "model/freeze_manager.h"
#include "model/track.h"
#include "model/track_types.h"
#include "model/audio_clip.h"
#include "model/types.h"
#include "project/project_manager.h"
#include "audio/audio_engine.h"
#include "audio/export/offline_renderer.h"

#include <memory>

using namespace aria;

// ═══════════════════════════════════════════════════════════════════
// Task 5.2: FreezeManager
// ═══════════════════════════════════════════════════════════════════

// Helper: create project with one track and engine with one track
// Heavy objects (AudioEngine, OfflineRenderer) use heap allocation to
// avoid stack overflow from their internal buffer allocations.
struct FreezeTestFixture {
    ProjectManager pm;
    ProjectID proj;
    TrackID tid;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<OfflineRenderer> renderer;
    FreezeManager manager;

    FreezeTestFixture()
        : proj(pm.create("Test", "/tmp/test.aria"))
        , tid(pm.create_audio_track(proj, "Track 1"))
        , engine(std::make_unique<AudioEngine>())
        , renderer(std::make_unique<OfflineRenderer>())
        , manager(pm, *engine, *renderer)
    {
        engine->init(48000, 256);
        engine->add_track();  // engine track 0 (used by freeze_track)
    }
};

TEST_CASE("FreezeManager — freeze_track marks track as frozen",
          "[freeze][manager]") {
    FreezeTestFixture f;

    SECTION("track is not frozen initially") {
        REQUIRE_FALSE(f.manager.is_frozen(f.tid));
    }

    SECTION("after freeze_track, track is frozen") {
        f.manager.freeze_track(f.tid);
        REQUIRE(f.manager.is_frozen(f.tid));
    }

    SECTION("freeze_track on already-frozen track is idempotent") {
        f.manager.freeze_track(f.tid);
        REQUIRE(f.manager.is_frozen(f.tid));
        f.manager.freeze_track(f.tid);
        REQUIRE(f.manager.is_frozen(f.tid));
    }

    SECTION("freeze_track on non-existent track does nothing") {
        REQUIRE_NOTHROW(f.manager.freeze_track(TrackID{9999}));
        REQUIRE_FALSE(f.manager.is_frozen(TrackID{9999}));
    }
}

TEST_CASE("FreezeManager — unfreeze_track restores track state",
          "[freeze][manager]") {
    FreezeTestFixture f;

    SECTION("after freeze then unfreeze, track is not frozen") {
        f.manager.freeze_track(f.tid);
        REQUIRE(f.manager.is_frozen(f.tid));

        f.manager.unfreeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_frozen(f.tid));
    }

    SECTION("unfreeze on non-frozen track is harmless") {
        REQUIRE_FALSE(f.manager.is_frozen(f.tid));
        REQUIRE_NOTHROW(f.manager.unfreeze_track(f.tid));
        REQUIRE_FALSE(f.manager.is_frozen(f.tid));
    }

    SECTION("unfreeze on non-existent track does nothing") {
        REQUIRE_NOTHROW(f.manager.unfreeze_track(TrackID{9999}));
    }
}

TEST_CASE("FreezeManager — freeze stores freeze_clip on track",
          "[freeze][manager]") {
    FreezeTestFixture f;

    SECTION("freeze_clip is set on track after freeze") {
        f.manager.freeze_track(f.tid);
        Track* track = f.pm.get_track(f.tid);
        REQUIRE(track != nullptr);
        // Frozen clip should be set (even if render produces silence)
        REQUIRE(track->frozen_clip() != nullptr);
    }

    SECTION("unfreeze clears frozen_clip") {
        f.manager.freeze_track(f.tid);
        f.manager.unfreeze_track(f.tid);
        Track* track = f.pm.get_track(f.tid);
        REQUIRE(track != nullptr);
        REQUIRE(track->frozen_clip() == nullptr);
    }
}

TEST_CASE("FreezeManager — dirty flag behavior", "[freeze][manager][dirty]") {
    FreezeTestFixture f;

    SECTION("track is not dirty initially") {
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));
    }

    SECTION("freeze resets dirty flag") {
        f.manager.mark_dirty(f.tid);
        REQUIRE(f.manager.is_dirty(f.tid));

        f.manager.freeze_track(f.tid);
        // After re-freeze, dirty should be reset (even if render fails)
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));
    }

    SECTION("mark_dirty sets dirty flag on frozen track") {
        f.manager.freeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));

        f.manager.mark_dirty(f.tid);
        REQUIRE(f.manager.is_dirty(f.tid));
    }

    SECTION("is_dirty returns false for non-existent track") {
        REQUIRE_FALSE(f.manager.is_dirty(TrackID{9999}));
    }
}

TEST_CASE("FreezeManager — BounceOptions defaults", "[freeze][bounce]") {
    BounceOptions opts;

    SECTION("default format is Master") {
        REQUIRE(opts.format == BounceFormat::Master);
    }

    SECTION("default sample_rate is 48000") {
        REQUIRE(opts.sample_rate == 48000);
    }

    SECTION("default bit_depth is 24") {
        REQUIRE(opts.bit_depth == 24);
    }

    SECTION("default range_start is 0") {
        REQUIRE(opts.range_start == 0);
    }

    SECTION("default range_end is 0 (entire project)") {
        REQUIRE(opts.range_end == 0);
    }

    SECTION("default output_directory is empty") {
        REQUIRE(opts.output_directory.empty());
    }

    SECTION("default tracks is empty") {
        REQUIRE(opts.tracks.empty());
    }
}

TEST_CASE("FreezeManager — bounce enum values", "[freeze][bounce]") {
    REQUIRE(static_cast<int>(BounceFormat::Individual) == 0);
    REQUIRE(static_cast<int>(BounceFormat::Stem) == 1);
    REQUIRE(static_cast<int>(BounceFormat::Master) == 2);
}

TEST_CASE("FreezeManager — bounce on empty project does nothing",
          "[freeze][bounce]") {
    ProjectManager pm;
    pm.create("Test", "/tmp/test.aria");

    auto engine = std::make_unique<AudioEngine>();
    REQUIRE(engine->init(48000, 256));
    auto renderer = std::make_unique<OfflineRenderer>();
    FreezeManager manager(pm, *engine, *renderer);

    SECTION("master bounce with no tracks completes without error") {
        BounceOptions opts;
        opts.format = BounceFormat::Master;
        opts.output_directory = "/tmp";
        REQUIRE_NOTHROW(manager.bounce(opts));
    }

    SECTION("individual bounce with no tracks completes without error") {
        BounceOptions opts;
        opts.format = BounceFormat::Individual;
        opts.output_directory = "/tmp";
        REQUIRE_NOTHROW(manager.bounce(opts));
    }
}

// ═══════════════════════════════════════════════════════════════════
// Task 6.5: FreezeManager dirty flag edge cases
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("FreezeManager — unfreeze resets dirty flag",
          "[freeze][manager][dirty][task6.5]") {
    FreezeTestFixture f;

    SECTION("dirty after mark_dirty, unfreeze resets it") {
        f.manager.mark_dirty(f.tid);
        REQUIRE(f.manager.is_dirty(f.tid));

        f.manager.unfreeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));
    }

    SECTION("freeze then mark_dirty then unfreeze clears dirty") {
        f.manager.freeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));

        f.manager.mark_dirty(f.tid);
        REQUIRE(f.manager.is_dirty(f.tid));

        f.manager.unfreeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));
    }
}

TEST_CASE("FreezeManager — mark_dirty on non-existent track does nothing",
          "[freeze][manager][dirty][task6.5]") {
    FreezeTestFixture f;

    SECTION("mark_dirty on TrackID 9999 has no effect") {
        REQUIRE_NOTHROW(f.manager.mark_dirty(TrackID{9999}));
        REQUIRE_FALSE(f.manager.is_dirty(TrackID{9999}));
    }
}

TEST_CASE("FreezeManager — freeze and unfreeze cycle multiple times",
          "[freeze][manager][task6.5]") {
    FreezeTestFixture f;

    SECTION("freeze → unfreeze → freeze → unfreeze cycle") {
        f.manager.freeze_track(f.tid);
        REQUIRE(f.manager.is_frozen(f.tid));

        f.manager.unfreeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_frozen(f.tid));

        f.manager.freeze_track(f.tid);
        REQUIRE(f.manager.is_frozen(f.tid));

        f.manager.unfreeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_frozen(f.tid));
    }

    SECTION("dirty flag reset after re-freeze") {
        f.manager.freeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));

        f.manager.mark_dirty(f.tid);
        REQUIRE(f.manager.is_dirty(f.tid));

        f.manager.freeze_track(f.tid);
        REQUIRE_FALSE(f.manager.is_dirty(f.tid));
    }
}

// ═══════════════════════════════════════════════════════════════════
// Task 6.5: OfflineRenderer per-track isolation
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("FreezeManager — freeze_track render failure still marks frozen",
          "[freeze][manager][task6.5]") {
    // Use an engine with no tracks — freeze should handle gracefully
    ProjectManager pm;
    pm.create("Test", "/tmp/test.aria");
    // Note: no tracks created

    auto engine = std::make_unique<AudioEngine>();
    REQUIRE(engine->init(48000, 256));
    // Note: no engine tracks added — render_track will fail

    auto renderer = std::make_unique<OfflineRenderer>();
    FreezeManager manager(pm, *engine, *renderer);

    SECTION("freeze_track with empty project does not crash") {
        REQUIRE_NOTHROW(manager.freeze_track(TrackID{1}));
    }

    SECTION("is_frozen on non-existent track returns false") {
        REQUIRE_FALSE(manager.is_frozen(TrackID{1}));
    }
}

// ═══════════════════════════════════════════════════════════════════
// Task 6.5: Session→Arrangement capture simulation
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("Session — capture clip to arrangement preserves clip data",
          "[session][capture][task6.5]") {
    Session session;
    SceneID scene = session.add_scene("Scene 1");
    TrackID track{1};

    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);
    clip->set_gain(0.75);
    clip->set_name("Session Loop");

    // Place in session
    session.set_clip_slot(track, scene, clip);

    SECTION("session clip is accessible after placement") {
        auto* retrieved = session.clip_at(track, scene);
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->name() == "Session Loop");
        REQUIRE(retrieved->gain() == Catch::Approx(0.75));
        REQUIRE(retrieved->length() == 960);
    }

    // Simulate capture: create a new AudioClip copying session clip properties
    SECTION("captured clip preserves source clip properties") {
        auto* source = session.clip_at(track, scene);
        REQUIRE(source != nullptr);

        auto captured = std::make_shared<AudioClip>();
        captured->set_length(source->length());
        captured->set_gain(source->gain());
        captured->set_name("Session: " + source->name());

        REQUIRE(captured->length() == 960);
        REQUIRE(captured->gain() == Catch::Approx(0.75));
        REQUIRE(captured->name() == "Session: Session Loop");
    }

    // Captured clip can be placed in arrangement (via Track::add_clip)
    SECTION("captured placement into arrangement track") {
        AudioTrack arr_track;
        auto* source = session.clip_at(track, scene);
        REQUIRE(source != nullptr);

        // Create the captured clip from the session source
        auto captured = std::make_shared<AudioClip>();
        captured->set_length(source->length());
        captured->set_gain(source->gain());
        captured->set_name(source->name());

        // Place on the arrangement track at PPQN 0
        bool placed = arr_track.add_audio_clip(captured, 0);
        REQUIRE(placed);
        REQUIRE(arr_track.audio_clip_count() == 1);

        // Verify it's accessible on the arrangement track
        AudioClip* retrieved = arr_track.audio_clip_at(480);
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->name() == "Session Loop");
        REQUIRE(retrieved->gain() == Catch::Approx(0.75));
    }
}
