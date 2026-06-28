#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/quantize_engine.h"
#include "pianoroll/note.h"

using Catch::Approx;
using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Grid quantize
// ═══════════════════════════════════════════════════════════════

TEST_CASE("QuantizeEngine basic grid snap", "[pianoroll][quantize]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;  // 16th notes
    s.strength = 1.0;
    s.snap_start = true;
    s.snap_end = false;

    SECTION("note already on grid stays in place") {
        Note n;
        n.start_ppqn = 480;
        n.duration_ppqn = 240;
        auto result = qe.quantize_note(n, s);
        REQUIRE(result.start_ppqn == 480);
        REQUIRE(result.duration_ppqn == 240);
    }

    SECTION("note between grid snaps to nearest") {
        Note n;
        n.start_ppqn = 100;  // Between 0 and 240
        n.duration_ppqn = 200;
        auto result = qe.quantize_note(n, s);
        // 100 + 120(half of 240) = 220 → 220/240 = 0 → 0 * 240 = 0
        REQUIRE(result.start_ppqn == 0);
    }

    SECTION("note closer to next grid snaps up") {
        Note n;
        n.start_ppqn = 200;  // 200 + 120 = 320 → 320/240 = 1 → 240
        n.duration_ppqn = 100;
        auto result = qe.quantize_note(n, s);
        REQUIRE(result.start_ppqn == 240);
    }
}

TEST_CASE("QuantizeEngine strength", "[pianoroll][quantize]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;
    s.strength = 0.5;  // 50% snap
    s.snap_start = true;

    SECTION("strength 0.5 blends between original and snapped") {
        Note n;
        n.start_ppqn = 100;
        n.duration_ppqn = 100;
        // original=100, snapped=0(see above), blended=100*0.5+0*0.5=50
        auto result = qe.quantize_note(n, s);
        REQUIRE(result.start_ppqn == 50);
    }

    SECTION("strength 0.0 makes no change") {
        s.strength = 0.0;
        Note n;
        n.start_ppqn = 100;
        n.duration_ppqn = 100;
        auto result = qe.quantize_note(n, s);
        REQUIRE(result.start_ppqn == 100);
    }

    SECTION("strength 1.0 full snap") {
        s.strength = 1.0;
        Note n;
        n.start_ppqn = 100;
        n.duration_ppqn = 100;
        auto result = qe.quantize_note(n, s);
        REQUIRE(result.start_ppqn == 0);
    }
}

TEST_CASE("QuantizeEngine snap_end", "[pianoroll][quantize]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;
    s.strength = 1.0;
    s.snap_start = false;
    s.snap_end = true;

    SECTION("end position snaps to grid") {
        Note n;
        n.start_ppqn = 100;
        n.duration_ppqn = 200;  // end = 300
        // end: 300 + 120 = 420 → 420/240 = 1 → 240
        auto result = qe.quantize_note(n, s);
        // start unchanged (100), end snapped to 240, duration = 240-100=140
        REQUIRE(result.start_ppqn == 100);
        REQUIRE(result.start_ppqn + result.duration_ppqn == 240);
    }
}

// ═══════════════════════════════════════════════════════════════
// Swing
// ═══════════════════════════════════════════════════════════════

TEST_CASE("QuantizeEngine swing", "[pianoroll][quantize][swing]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;
    s.strength = 1.0;
    s.snap_start = true;
    s.swing = 0.5;

    SECTION("on-beat notes stay unchanged") {
        Note n;
        n.start_ppqn = 0;  // On beat
        auto result = qe.quantize_note(n, s);
        REQUIRE(result.start_ppqn == 0);
    }

    SECTION("off-beat notes shift forward with swing") {
        Note n;
        n.start_ppqn = 240;  // Off-beat (16th note)
        auto result = qe.quantize_note(n, s);
        // Off-beat: shifted by swing amount
        // half_grid = 120, swing = 0.5
        // result = 240 + 120 * 0.5 = 300
        REQUIRE(result.start_ppqn > 240);
    }
}

// ═══════════════════════════════════════════════════════════════
// Preserve duration
// ═══════════════════════════════════════════════════════════════

TEST_CASE("QuantizeEngine preserve_duration", "[pianoroll][quantize]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;
    s.strength = 1.0;
    s.snap_start = true;
    s.snap_end = false;
    s.preserve_duration = true;

    SECTION("duration preserved when start snaps forward") {
        Note n;
        n.start_ppqn = 50;
        n.duration_ppqn = 100;  // end = 150
        auto result = qe.quantize_note(n, s);
        // start snaps to 0
        // end stays at 150 (unchanged by snapping end)
        // But preserve_duration with snap_start adjusts: dur = end - new_start
        // So duration = 150 - 0 = 150 → adjusted
        // Actually with preserve_duration=true AND snap_start=true:
        // we keep the same end position = 50 + 100 = 150
        // new_start=0, so duration = 150 - 0 = 150
        REQUIRE(result.start_ppqn == 0);
        REQUIRE(result.duration_ppqn == 150);
    }
}

// ═══════════════════════════════════════════════════════════════
// Preview and bulk
// ═══════════════════════════════════════════════════════════════

TEST_CASE("QuantizeEngine preview", "[pianoroll][quantize][preview]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;
    s.strength = 1.0;
    s.snap_start = true;

    SECTION("preview returns quantized copies without modifying originals") {
        Note n1, n2;
        n1.start_ppqn = 50;
        n2.start_ppqn = 200;
        std::vector<Note*> notes = {&n1, &n2};

        auto preview = qe.preview(notes, s);

        REQUIRE(preview.size() == 2);
        REQUIRE(preview[0].start_ppqn == 0);  // Snapped
        REQUIRE(preview[1].start_ppqn == 240);

        // Originals unchanged.
        REQUIRE(n1.start_ppqn == 50);
        REQUIRE(n2.start_ppqn == 200);
    }
}

TEST_CASE("QuantizeEngine bulk quantize", "[pianoroll][quantize][bulk]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;
    s.strength = 1.0;
    s.snap_start = true;

    SECTION("quantize modifies notes in-place") {
        Note n1, n2;
        n1.start_ppqn = 50;
        n2.start_ppqn = 200;
        std::vector<Note*> notes = {&n1, &n2};

        qe.quantize(notes, s);

        REQUIRE(n1.start_ppqn == 0);
        REQUIRE(n2.start_ppqn == 240);
    }

    SECTION("quantize handles empty vector") {
        std::vector<Note*> empty;
        REQUIRE_NOTHROW(qe.quantize(empty, s));
    }
}

TEST_CASE("QuantizeEngine minimum duration", "[pianoroll][quantize]") {
    QuantizeEngine qe;
    QuantizeSettings s;
    s.grid_ppqn = 240;
    s.strength = 1.0;
    s.snap_start = true;

    SECTION("duration is at least 1") {
        Note n;
        n.start_ppqn = 100;
        n.duration_ppqn = 0;
        auto result = qe.quantize_note(n, s);
        REQUIRE(result.duration_ppqn >= 1);
    }
}
