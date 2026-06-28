#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/humanize_engine.h"
#include "pianoroll/note.h"

#include <algorithm>

using Catch::Approx;
using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Timing randomization
// ═══════════════════════════════════════════════════════════════

TEST_CASE("HumanizeEngine timing", "[pianoroll][humanize]") {
    HumanizeEngine he;
    HumanizeSettings s;
    s.randomize_start = true;
    s.randomize_duration = false;
    s.randomize_velocity = false;
    s.timing_amount = 30;
    s.seed = 42;  // Fixed seed for reproducibility

    SECTION("timing offset shifts note start") {
        Note n;
        n.start_ppqn = 480;
        n.duration_ppqn = 240;
        n.velocity = 100;

        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);

        // With seed=42, the offset should produce a predictable shift.
        // We can't check exact value (RNG), but start should differ from 480.
        REQUIRE(n.start_ppqn != 480);
    }

    SECTION("timing does not go negative") {
        Note n;
        n.start_ppqn = 5;  // Very small — humanization might push below 0
        n.duration_ppqn = 100;

        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);

        REQUIRE(n.start_ppqn == 0);  // Clamped to 0
    }

    SECTION("zero timing amount makes no change") {
        s.timing_amount = 0.0;
        Note n;
        n.start_ppqn = 480;
        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);
        REQUIRE(n.start_ppqn == 480);
    }

    SECTION("randomize_start disabled keeps timing") {
        s.randomize_start = false;
        Note n;
        n.start_ppqn = 480;
        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);
        REQUIRE(n.start_ppqn == 480);
    }
}

// ═══════════════════════════════════════════════════════════════
// Velocity randomization
// ═══════════════════════════════════════════════════════════════

TEST_CASE("HumanizeEngine velocity", "[pianoroll][humanize][velocity]") {
    HumanizeEngine he;
    HumanizeSettings s;
    s.randomize_start = false;
    s.randomize_duration = false;
    s.randomize_velocity = true;
    s.velocity_amount = 15;
    s.seed = 99;

    SECTION("velocity changes within 0-127 range") {
        Note n;
        n.velocity = 100;

        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);

        REQUIRE(n.velocity >= 0);
        REQUIRE(n.velocity <= 127);
    }

    SECTION("velocity at boundary stays clamped") {
        Note n;
        n.velocity = 5;  // Near minimum

        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);

        REQUIRE(n.velocity >= 0);
    }

    SECTION("zero velocity amount makes no change") {
        s.velocity_amount = 0.0;
        Note n;
        n.velocity = 100;

        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);

        REQUIRE(n.velocity == 100);
    }
}

// ═══════════════════════════════════════════════════════════════
// Duration randomization
// ═══════════════════════════════════════════════════════════════

TEST_CASE("HumanizeEngine duration", "[pianoroll][humanize][duration]") {
    HumanizeEngine he;
    HumanizeSettings s;
    s.randomize_start = false;
    s.randomize_duration = true;
    s.randomize_velocity = false;
    s.duration_amount = 10;
    s.seed = 77;

    SECTION("duration changes but stays >= 1") {
        Note n;
        n.duration_ppqn = 240;

        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);

        REQUIRE(n.duration_ppqn >= 1);
    }
}

// ═══════════════════════════════════════════════════════════════
// Groove templates
// ═══════════════════════════════════════════════════════════════

TEST_CASE("HumanizeEngine groove templates", "[pianoroll][humanize][groove]") {
    SECTION("shuffle_swing creates 16-position template") {
        auto gt = HumanizeEngine::shuffle_swing(0.5);
        REQUIRE(gt.name == "Shuffle/Swing");
        REQUIRE(gt.timing_offsets.size() == 16);
        REQUIRE(gt.velocity_multipliers.size() == 16);

        // Off-beats (odd indices) should have positive timing offsets.
        for (int i = 0; i < 16; ++i) {
            if (i % 2 == 1) {
                REQUIRE(gt.timing_offsets[i] > 0);
            }
        }
    }

    SECTION("latin template has 16 positions") {
        auto gt = HumanizeEngine::latin();
        REQUIRE(gt.name == "Latin");
        REQUIRE(gt.timing_offsets.size() == 16);
        REQUIRE(gt.velocity_multipliers.size() == 16);
    }

    SECTION("funk template has 16 positions") {
        auto gt = HumanizeEngine::funk();
        REQUIRE(gt.name == "Funk");
        REQUIRE(gt.timing_offsets.size() == 16);
    }

    SECTION("hiphop template has 16 positions") {
        auto gt = HumanizeEngine::hiphop();
        REQUIRE(gt.name == "Hip-Hop");
        REQUIRE(gt.timing_offsets.size() == 16);
    }

    SECTION("shuffle swing amount parameter changes offsets") {
        auto gt_light = HumanizeEngine::shuffle_swing(0.3);
        auto gt_heavy = HumanizeEngine::shuffle_swing(0.9);

        // Heavier swing should have larger off-beat offsets.
        bool any_larger = false;
        for (int i = 1; i < 16; i += 2) {
            if (gt_heavy.timing_offsets[i] > gt_light.timing_offsets[i]) {
                any_larger = true;
                break;
            }
        }
        REQUIRE(any_larger);
    }
}

TEST_CASE("HumanizeEngine apply_groove", "[pianoroll][humanize][groove]") {
    HumanizeEngine he;

    SECTION("groove applies timing offsets to notes") {
        auto gt = HumanizeEngine::shuffle_swing(0.5);

        Note n;
        n.start_ppqn = 240;  // 16th note position (index 1)
        n.duration_ppqn = 120;
        n.velocity = 100;

        std::vector<Note*> notes = {&n};
        he.apply_groove(notes, gt);

        // Off-beat position (index 1) should have positive offset.
        REQUIRE(n.start_ppqn > 240);
    }

    SECTION("empty notes vector doesn't crash") {
        auto gt = HumanizeEngine::shuffle_swing(0.5);
        std::vector<Note*> empty;
        REQUIRE_NOTHROW(he.apply_groove(empty, gt));
    }

    SECTION("empty groove template doesn't crash") {
        HumanizeEngine::GrooveTemplate empty_gt;
        empty_gt.name = "Empty";

        Note n;
        n.start_ppqn = 240;
        std::vector<Note*> notes = {&n};
        REQUIRE_NOTHROW(he.apply_groove(notes, empty_gt));
    }
}

// ═══════════════════════════════════════════════════════════════
// Combined humanization
// ═══════════════════════════════════════════════════════════════

TEST_CASE("HumanizeEngine combined effects", "[pianoroll][humanize][combined]") {
    HumanizeEngine he;
    HumanizeSettings s;
    s.randomize_start = true;
    s.randomize_velocity = true;
    s.randomize_duration = true;
    s.timing_amount = 20;
    s.velocity_amount = 10;
    s.duration_amount = 8;
    s.seed = 12345;

    SECTION("all randomization types applied together") {
        Note n;
        n.start_ppqn = 480;
        n.duration_ppqn = 240;
        n.velocity = 100;

        std::vector<Note*> notes = {&n};
        he.humanize(notes, s);

        // At least something should have changed.
        bool any_change = (n.start_ppqn != 480 ||
                           n.duration_ppqn != 240 ||
                           n.velocity != 100);
        REQUIRE(any_change);
    }

    SECTION("reproducible with same seed") {
        Note n1, n2;
        n1.start_ppqn = 500;
        n1.duration_ppqn = 200;
        n1.velocity = 90;
        n2 = n1;

        std::vector<Note*> notes1 = {&n1};
        std::vector<Note*> notes2 = {&n2};

        he.humanize(notes1, s);
        he.humanize(notes2, s);

        // Same seed should produce same results.
        REQUIRE(n1.start_ppqn == n2.start_ppqn);
        REQUIRE(n1.duration_ppqn == n2.duration_ppqn);
        REQUIRE(n1.velocity == n2.velocity);
    }
}
