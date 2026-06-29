#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "model/track.h"
#include "model/track_types.h"
#include "model/audio_clip.h"
#include "model/routing.h"
#include "project/project_manager.h"

using namespace aria;

// ─── GroupTrack ───────────────────────────────────────────────────

TEST_CASE("GroupTrack — children CRUD", "[model][track][group]") {
    GroupTrack group;

    SECTION("initially empty children") {
        REQUIRE(group.children().empty());
    }

    SECTION("add_child appends to children list") {
        TrackID child{42};
        group.add_child(child);
        REQUIRE(group.children().size() == 1);
        REQUIRE(group.children()[0] == child);
    }

    SECTION("remove_child removes from list") {
        TrackID child{42};
        group.add_child(child);
        REQUIRE(group.children().size() == 1);

        group.remove_child(child);
        REQUIRE(group.children().empty());
    }

    SECTION("remove_child that doesn't exist is a no-op") {
        TrackID child{42};
        group.remove_child(child);
        REQUIRE(group.children().empty());  // no crash
    }

    SECTION("multiple children") {
        group.add_child(TrackID{1});
        group.add_child(TrackID{2});
        group.add_child(TrackID{3});
        REQUIRE(group.children().size() == 3);
        REQUIRE(group.children()[0] == TrackID{1});
        REQUIRE(group.children()[1] == TrackID{2});
        REQUIRE(group.children()[2] == TrackID{3});
    }
}

TEST_CASE("GroupTrack — fold state", "[model][track][group]") {
    GroupTrack group;

    SECTION("default is unfolded") {
        REQUIRE_FALSE(group.is_folded());
    }

    SECTION("set_folded true") {
        group.set_folded(true);
        REQUIRE(group.is_folded());
    }

    SECTION("toggle fold") {
        group.set_folded(true);
        REQUIRE(group.is_folded());
        group.set_folded(false);
        REQUIRE_FALSE(group.is_folded());
    }
}

TEST_CASE("GroupTrack — GroupMode", "[model][track][group]") {
    GroupTrack group;

    SECTION("default mode is Summing") {
        REQUIRE(group.group_mode() == GroupMode::Summing);
    }

    SECTION("set Summing mode") {
        group.set_group_mode(GroupMode::Summing);
        REQUIRE(group.group_mode() == GroupMode::Summing);
    }

    SECTION("set Folder mode") {
        group.set_group_mode(GroupMode::Folder);
        REQUIRE(group.group_mode() == GroupMode::Folder);
    }

    SECTION("set Routing mode") {
        group.set_group_mode(GroupMode::Routing);
        REQUIRE(group.group_mode() == GroupMode::Routing);
    }
}

// ─── VCATrack ────────────────────────────────────────────────────

TEST_CASE("VCATrack — slave management", "[model][track][vca]") {
    VCATrack vca;

    SECTION("initially empty slaves") {
        REQUIRE(vca.slaves().empty());
    }

    SECTION("add_slave appends to list") {
        TrackID slave{100};
        vca.add_slave(slave);
        REQUIRE(vca.slaves().size() == 1);
        REQUIRE(vca.slaves()[0] == slave);
    }

    SECTION("remove_slave removes from list") {
        TrackID slave{100};
        vca.add_slave(slave);
        vca.remove_slave(slave);
        REQUIRE(vca.slaves().empty());
    }
}

TEST_CASE("VCATrack — effective volume propagation",
          "[model][track][vca]") {
    // VCA at -6 dB, slave at -6 dB → effective = -12 dB
    VCATrack vca;
    vca.set_volume(-6.0);

    // Create a slave track with its own volume
    Track slave(TrackType::Audio);
    slave.set_volume(-6.0);

    // Wire VCA contribution
    vca.link_slave(slave.id());
    vca.apply_to(slave.id(), -6.0);

    // Effective volume = slave volume + VCA contribution
    double effective = slave.volume() + vca.vca_contribution(slave.id());
    REQUIRE(effective == Catch::Approx(-12.0));
}

TEST_CASE("VCATrack — slave at -∞ stays at -∞",
          "[model][track][vca]") {
    VCATrack vca;
    vca.set_volume(-6.0);

    Track slave(TrackType::Audio);
    slave.set_volume(-std::numeric_limits<double>::infinity());

    // Wire VCA contribution
    vca.link_slave(slave.id());
    vca.apply_to(slave.id(), -6.0);

    // Slave at -∞ stays at -∞ regardless of VCA
    REQUIRE(slave.volume() == -std::numeric_limits<double>::infinity());
}

TEST_CASE("VCATrack — VCAAffects defaults to All",
          "[model][track][vca]") {
    VCATrack vca;
    REQUIRE(vca.affects() == VCAAffects::All);
}

// ─── ReturnTrack ─────────────────────────────────────────────────

TEST_CASE("ReturnTrack — solo-safe defaults to true",
          "[model][track][return]") {
    ReturnTrack ret;

    SECTION("default solo_safe is true") {
        REQUIRE(ret.is_solo_safe());
    }

    SECTION("solo-safe persists through toggling") {
        ret.set_solo_safe(false);
        REQUIRE_FALSE(ret.is_solo_safe());
        ret.set_solo_safe(true);
        REQUIRE(ret.is_solo_safe());
    }
}

TEST_CASE("ReturnTrack — has no clips", "[model][track][return]") {
    ReturnTrack ret;

    SECTION("type is Return") {
        REQUIRE(ret.type() == TrackType::Return);
    }

    SECTION("has_clips returns false") {
        REQUIRE_FALSE(ret.has_clips());
    }
}

// ─── MasterTrack ─────────────────────────────────────────────────

TEST_CASE("MasterTrack — cannot be deleted", "[model][track][master]") {
    ProjectManager pm;
    auto proj = pm.create("Test", "/tmp/test.aria");

    SECTION("delete master track returns false") {
        auto master_id = pm.create_track(proj, TrackType::Master, "Master");
        REQUIRE(master_id.value != 0);

        bool deleted = pm.delete_track(proj, master_id);
        REQUIRE_FALSE(deleted);  // cannot delete master
    }

    SECTION("master track type is Master") {
        auto master_id = pm.create_track(proj, TrackType::Master, "Master");
        auto* master = pm.get_track(master_id);
        REQUIRE(master != nullptr);
        REQUIRE(master->type() == TrackType::Master);
    }
}

TEST_CASE("MasterTrack — only one master per project",
          "[model][track][master]") {
    ProjectManager pm;
    auto proj = pm.create("Test", "/tmp/test.aria");

    auto master1 = pm.create_track(proj, TrackType::Master, "Master 1");
    auto master2 = pm.create_track(proj, TrackType::Master, "Master 2");

    // Second master creation should fail or be a no-op
    // Only the first master should be valid
    REQUIRE(master1.value != 0);
    REQUIRE(master2.value == 0);  // second master creation fails
}

// ═══════════════════════════════════════════════════════════════
// Task 1.1: Freeze state (Track base)
// ═══════════════════════════════════════════════════════════════

TEST_CASE("Track — freeze state defaults", "[model][track][freeze]") {
    Track t(TrackType::Audio);

    SECTION("is_frozen defaults to false") {
        REQUIRE_FALSE(t.is_frozen());
    }

    SECTION("show_frozen_audio defaults to false") {
        REQUIRE_FALSE(t.show_frozen_audio());
    }

    SECTION("frozen_clip returns nullptr by default") {
        REQUIRE(t.frozen_clip() == nullptr);
    }

    SECTION("is_freeze_dirty defaults to false") {
        REQUIRE_FALSE(t.is_freeze_dirty());
    }
}

TEST_CASE("Track — freeze state toggling", "[model][track][freeze]") {
    Track t(TrackType::Audio);

    SECTION("set_frozen(true) persists") {
        t.set_frozen(true);
        REQUIRE(t.is_frozen());
        t.set_frozen(false);
        REQUIRE_FALSE(t.is_frozen());
    }

    SECTION("set_show_frozen_audio toggles") {
        t.set_show_frozen_audio(true);
        REQUIRE(t.show_frozen_audio());
        t.set_show_frozen_audio(false);
        REQUIRE_FALSE(t.show_frozen_audio());
    }
}

TEST_CASE("Track — freeze dirty flag", "[model][track][freeze]") {
    Track t(TrackType::Audio);

    SECTION("set_freeze_dirty marks dirty") {
        t.set_freeze_dirty(true);
        REQUIRE(t.is_freeze_dirty());
        t.set_freeze_dirty(false);
        REQUIRE_FALSE(t.is_freeze_dirty());
    }
}

TEST_CASE("Track — frozen clip pointer lifecycle", "[model][track][freeze]") {
    Track t(TrackType::Audio);

    SECTION("set_frozen_clip stores clip, frozen_clip returns it") {
        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        t.set_frozen_clip(clip);
        REQUIRE(t.frozen_clip() == clip.get());
    }

    SECTION("frozen_clip returns nullptr after clearing") {
        auto clip = std::make_shared<AudioClip>();
        t.set_frozen_clip(clip);
        REQUIRE(t.frozen_clip() != nullptr);
        t.set_frozen_clip(nullptr);
        REQUIRE(t.frozen_clip() == nullptr);
    }
}

TEST_CASE("Track — frozen track blocks add_clip", "[model][track][freeze]") {
    Track t(TrackType::Audio);
    auto clip = std::make_shared<AudioClip>();
    clip->set_length(960);

    SECTION("unfrozen track allows add_clip") {
        bool result = t.add_clip(clip, 0);
        REQUIRE(result);  // clip was added
        REQUIRE(t.clip_at(480) != nullptr);
    }

    SECTION("frozen track rejects add_clip") {
        t.set_frozen(true);
        bool result = t.add_clip(clip, 0);
        REQUIRE_FALSE(result);  // clip was rejected
        REQUIRE(t.clip_at(480) == nullptr);
    }
}

// ═══════════════════════════════════════════════════════════════
// Task 1.2 + 1.3: AudioTrack typed clip storage and methods
// ═══════════════════════════════════════════════════════════════

TEST_CASE("AudioTrack — construction and type", "[model][track][audio]") {
    AudioTrack at;

    SECTION("type is Audio") {
        REQUIRE(at.type() == TrackType::Audio);
    }

    SECTION("initially has no audio clips") {
        REQUIRE(at.audio_clip_count() == 0);
    }
}

TEST_CASE("AudioTrack — add and retrieve audio clips", "[model][track][audio]") {
    AudioTrack at;

    SECTION("add_audio_clip stores clip") {
        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        at.add_audio_clip(clip, 0);
        REQUIRE(at.audio_clip_count() == 1);
    }

    SECTION("audio_clip_at returns clip at position") {
        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        at.add_audio_clip(clip, 0);

        auto* found = at.audio_clip_at(480);
        REQUIRE(found != nullptr);
        REQUIRE(found == clip.get());
    }

    SECTION("audio_clip_at returns nullptr for empty position") {
        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        at.add_audio_clip(clip, 0);

        // Position 1920 is past the clip's end (start 0 + length 960)
        REQUIRE(at.audio_clip_at(1920) == nullptr);
    }

    SECTION("add_audio_clip returns false when track is frozen") {
        at.set_frozen(true);
        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        REQUIRE_FALSE(at.add_audio_clip(clip, 0));
    }
}

TEST_CASE("AudioTrack — clips_in_range", "[model][track][audio]") {
    AudioTrack at;

    // Add clips at non-overlapping positions
    auto clip1 = std::make_shared<AudioClip>();
    clip1->set_length(480);  // [0, 480)
    at.add_audio_clip(clip1, 0);

    auto clip2 = std::make_shared<AudioClip>();
    clip2->set_length(480);  // [960, 1440)
    at.add_audio_clip(clip2, 960);

    SECTION("range spanning first clip returns clip1") {
        auto result = at.clips_in_range(0, 480);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == clip1.get());
    }

    SECTION("range spanning both clips returns both") {
        auto result = at.clips_in_range(0, 1440);
        REQUIRE(result.size() == 2);
    }

    SECTION("range with no clips returns empty") {
        auto result = at.clips_in_range(480, 960);
        REQUIRE(result.empty());
    }

    SECTION("range starting mid-clip still finds that clip") {
        auto result = at.clips_in_range(100, 300);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == clip1.get());
    }
}

TEST_CASE("AudioTrack — crossfade management", "[model][track][audio][crossfade]") {
    AudioTrack at;

    auto clip_a = std::make_shared<AudioClip>();
    clip_a->set_length(960);
    at.add_audio_clip(clip_a, 0);

    auto clip_b = std::make_shared<AudioClip>();
    clip_b->set_length(960);
    at.add_audio_clip(clip_b, 864);  // overlaps with clip_a by 96 PPQN

    SECTION("add_crossfade stores crossfade") {
        at.add_crossfade(clip_a->id(), clip_b->id(), 96, FadeShape::EqualPowerOut);
        REQUIRE(at.crossfade_count() == 1);
    }

    SECTION("remove_crossfade removes stored crossfade") {
        at.add_crossfade(clip_a->id(), clip_b->id(), 96, FadeShape::EqualPowerOut);
        REQUIRE(at.crossfade_count() == 1);
        at.remove_crossfade(clip_a->id(), clip_b->id());
        REQUIRE(at.crossfade_count() == 0);
    }
}

TEST_CASE("AudioTrack — evaluate_crossfade at overlap region",
          "[model][track][audio][crossfade]") {
    AudioTrack at;

    auto clip_a = std::make_shared<AudioClip>();
    clip_a->set_length(960);
    at.add_audio_clip(clip_a, 0);

    auto clip_b = std::make_shared<AudioClip>();
    clip_b->set_length(960);
    at.add_audio_clip(clip_b, 864);  // overlap: 864-960 (96 PPQN)

    // Crossfade using EqualPower shapes
    at.add_crossfade(clip_a->id(), clip_b->id(), 96, FadeShape::EqualPowerOut);

    SECTION("at start of overlap (ppqn=864): clip_a at unity, clip_b at zero") {
        auto result = at.evaluate_crossfade(864);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a == Catch::Approx(1.0));
        REQUIRE(result->gain_b == Catch::Approx(0.0));
    }

    SECTION("at mid-overlap (ppqn=912): both at ~0.707") {
        auto result = at.evaluate_crossfade(912);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a == Catch::Approx(0.70710678).margin(0.001));
        REQUIRE(result->gain_b == Catch::Approx(0.70710678).margin(0.001));
    }

    SECTION("at end of overlap (ppqn=960): clip_a at zero, clip_b at unity") {
        auto result = at.evaluate_crossfade(960);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a == Catch::Approx(0.0));
        REQUIRE(result->gain_b == Catch::Approx(1.0));
    }
}

TEST_CASE("AudioTrack — evaluate_crossfade outside overlap returns nullopt",
          "[model][track][audio][crossfade]") {
    AudioTrack at;

    auto clip_a = std::make_shared<AudioClip>();
    clip_a->set_length(960);
    at.add_audio_clip(clip_a, 0);

    auto clip_b = std::make_shared<AudioClip>();
    clip_b->set_length(960);
    at.add_audio_clip(clip_b, 864);

    at.add_crossfade(clip_a->id(), clip_b->id(), 96, FadeShape::EqualPowerOut);

    SECTION("before overlap returns nullopt") {
        auto result = at.evaluate_crossfade(0);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("after overlap returns nullopt") {
        auto result = at.evaluate_crossfade(1500);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("AudioTrack — session slot management", "[model][track][audio]") {
    AudioTrack at;

    SECTION("set_clip_in_slot stores clip, clip_in_slot retrieves") {
        auto clip = std::make_shared<AudioClip>();
        clip->set_length(960);
        at.set_clip_in_slot(SceneID{1}, clip);
        auto* retrieved = at.clip_in_slot(SceneID{1});
        REQUIRE(retrieved == clip.get());
    }

    SECTION("clip_in_slot for empty slot returns nullptr") {
        REQUIRE(at.clip_in_slot(SceneID{99}) == nullptr);
    }

    SECTION("different scene slots are independent") {
        auto clip1 = std::make_shared<AudioClip>();
        auto clip2 = std::make_shared<AudioClip>();
        at.set_clip_in_slot(SceneID{1}, clip1);
        at.set_clip_in_slot(SceneID{2}, clip2);
        REQUIRE(at.clip_in_slot(SceneID{1}) == clip1.get());
        REQUIRE(at.clip_in_slot(SceneID{2}) == clip2.get());
    }
}

// ═══════════════════════════════════════════════════════════════
// Task 1.4 + 1.5: MidiTrack
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiTrack — construction and type", "[model][track][midi]") {
    MidiTrack mt;

    SECTION("type is MIDI") {
        REQUIRE(mt.type() == TrackType::MIDI);
    }

    SECTION("default transpose is 0") {
        REQUIRE(mt.transpose() == 0);
    }

    SECTION("default instrument is empty") {
        REQUIRE(mt.instrument().empty());
    }
}

TEST_CASE("MidiTrack — instrument assignment", "[model][track][midi]") {
    MidiTrack mt;

    SECTION("set_instrument stores value") {
        mt.set_instrument("aria.analog-synth");
        REQUIRE(mt.instrument() == "aria.analog-synth");
    }

    SECTION("set_instrument can be changed") {
        mt.set_instrument("aria.piano");
        mt.set_instrument("aria.bass");
        REQUIRE(mt.instrument() == "aria.bass");
    }
}

TEST_CASE("MidiTrack — transpose", "[model][track][midi]") {
    MidiTrack mt;

    SECTION("set_transpose stores positive value") {
        mt.set_transpose(5);
        REQUIRE(mt.transpose() == 5);
    }

    SECTION("set_transpose stores negative value") {
        mt.set_transpose(-12);
        REQUIRE(mt.transpose() == -12);
    }

    SECTION("set_transpose stores zero") {
        mt.set_transpose(7);
        mt.set_transpose(0);
        REQUIRE(mt.transpose() == 0);
    }
}

TEST_CASE("MidiTrack — drum map", "[model][track][midi]") {
    MidiTrack mt;

    SECTION("default drum map returns identity mapping") {
        REQUIRE(mt.map_drum_note(36) == 36);  // default: pass through
        REQUIRE(mt.map_drum_note(60) == 60);
    }

    SECTION("set_drum_mapping stores custom mapping") {
        std::unordered_map<uint8_t, uint8_t> mapping;
        mapping[36] = 42;  // kick → closed hat
        mt.set_drum_mapping(mapping);
        REQUIRE(mt.map_drum_note(36) == 42);
    }

    SECTION("unmapped notes pass through") {
        std::unordered_map<uint8_t, uint8_t> mapping;
        mapping[36] = 42;
        mt.set_drum_mapping(mapping);
        REQUIRE(mt.map_drum_note(60) == 60);  // unchanged
    }
}

TEST_CASE("MidiTrack — session slot management", "[model][track][midi]") {
    MidiTrack mt;

    auto clip = std::make_shared<MidiClip>();
    mt.set_clip_in_slot(SceneID{3}, clip);

    SECTION("clip_in_slot retrieves stored clip") {
        auto* retrieved = mt.clip_in_slot(SceneID{3});
        REQUIRE(retrieved == clip.get());
    }

    SECTION("empty slot returns nullptr") {
        REQUIRE(mt.clip_in_slot(SceneID{99}) == nullptr);
    }
}

// ═══════════════════════════════════════════════════════════════
// Task 6.4: Additional crossfade edge cases
// ═══════════════════════════════════════════════════════════════

TEST_CASE("AudioTrack — crossfade with Linear shape",
          "[model][track][audio][crossfade][task6.4]") {
    AudioTrack at;

    auto clip_a = std::make_shared<AudioClip>();
    clip_a->set_length(960);
    at.add_audio_clip(clip_a, 0);

    auto clip_b = std::make_shared<AudioClip>();
    clip_b->set_length(960);
    at.add_audio_clip(clip_b, 864);

    at.add_crossfade(clip_a->id(), clip_b->id(), 96, FadeShape::LinearOut);

    SECTION("at start (864): gain_a=1.0, gain_b=0.0") {
        auto result = at.evaluate_crossfade(864);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a == Catch::Approx(1.0));
        REQUIRE(result->gain_b == Catch::Approx(0.0));
    }

    SECTION("at midpoint (912): gain_a=0.5, gain_b=0.5") {
        auto result = at.evaluate_crossfade(912);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a == Catch::Approx(0.5));
        REQUIRE(result->gain_b == Catch::Approx(0.5));
    }

    SECTION("at end (960): gain_a=0.0, gain_b=1.0") {
        auto result = at.evaluate_crossfade(960);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a == Catch::Approx(0.0));
        REQUIRE(result->gain_b == Catch::Approx(1.0));
    }
}

TEST_CASE("AudioTrack — multiple crossfades on same track",
          "[model][track][audio][crossfade][task6.4]") {
    AudioTrack at;

    auto clip_a = std::make_shared<AudioClip>();
    clip_a->set_length(960);
    at.add_audio_clip(clip_a, 0);

    auto clip_b = std::make_shared<AudioClip>();
    clip_b->set_length(960);
    at.add_audio_clip(clip_b, 864);

    auto clip_c = std::make_shared<AudioClip>();
    clip_c->set_length(960);
    at.add_audio_clip(clip_c, 1728);

    // Two separate crossfades
    at.add_crossfade(clip_a->id(), clip_b->id(), 96, FadeShape::EqualPowerOut);
    at.add_crossfade(clip_b->id(), clip_c->id(), 96, FadeShape::EqualPowerOut);

    SECTION("two crossfades stored") {
        REQUIRE(at.crossfade_count() == 2);
    }

    SECTION("first crossfade evaluated independently") {
        auto result = at.evaluate_crossfade(912);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a == Catch::Approx(0.70710678).margin(0.001));
        REQUIRE(result->gain_b == Catch::Approx(0.70710678).margin(0.001));
    }

    SECTION("second crossfade evaluated independently") {
        auto result = at.evaluate_crossfade(1776);
        REQUIRE(result.has_value());
        REQUIRE(result->gain_a >= 0.0);
        REQUIRE(result->gain_b >= 0.0);
    }

    SECTION("position outside all crossfades returns nullopt") {
        auto result = at.evaluate_crossfade(100);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("AudioTrack — crossfade for non-overlapping clips returns nullopt",
          "[model][track][audio][crossfade][task6.4]") {
    AudioTrack at;

    auto clip_a = std::make_shared<AudioClip>();
    clip_a->set_length(480);
    at.add_audio_clip(clip_a, 0);

    auto clip_b = std::make_shared<AudioClip>();
    clip_b->set_length(480);
    at.add_audio_clip(clip_b, 960);

    // These clips don't overlap (gap at 480-960), but we still add a crossfade
    at.add_crossfade(clip_a->id(), clip_b->id(), 96, FadeShape::EqualPowerOut);

    SECTION("position at gap returns nullopt") {
        auto result = at.evaluate_crossfade(720);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("position at clip_a area returns nullopt") {
        auto result = at.evaluate_crossfade(240);
        REQUIRE_FALSE(result.has_value());
    }
}

// ═══════════════════════════════════════════════════════════════
// Task 6.4: Additional MidiTrack transpose edge cases
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiTrack — transpose edge cases", "[model][track][midi][task6.4]") {
    MidiTrack mt;

    SECTION("max int8 transpose") {
        mt.set_transpose(127);
        REQUIRE(mt.transpose() == 127);
    }

    SECTION("min int8 transpose") {
        mt.set_transpose(-128);
        REQUIRE(mt.transpose() == -128);
    }

    SECTION("multiple transpose updates") {
        mt.set_transpose(3);
        mt.set_transpose(-5);
        mt.set_transpose(12);
        REQUIRE(mt.transpose() == 12);
    }

    SECTION("transpose preserves original after drum map change") {
        mt.set_transpose(7);
        std::unordered_map<uint8_t, uint8_t> mapping;
        mapping[36] = 42;
        mt.set_drum_mapping(mapping);
        REQUIRE(mt.transpose() == 7);
    }
}
