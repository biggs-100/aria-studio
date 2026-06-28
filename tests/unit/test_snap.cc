#include <catch2/catch_test_macros.hpp>
#include "pianoroll/snap_system.h"
#include "pianoroll/piano_roll_canvas.h"  // For ViewTransform

#include <cmath>

using namespace aria;

// ═══════════════════════════════════════════════════════════════
// SnapSystem basic tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SnapSystem defaults", "[pianoroll][snap]") {
    SnapSystem snap;

    SECTION("default is enabled") {
        REQUIRE(snap.is_enabled());
    }

    SECTION("default mode is Grid") {
        REQUIRE(snap.mode() == SnapSystem::Grid);
    }

    SECTION("default resolution is 240 (16th notes)") {
        REQUIRE(snap.resolution() == 240);
    }
}

TEST_CASE("SnapSystem enable/disable", "[pianoroll][snap]") {
    SnapSystem snap;

    SECTION("can disable snapping") {
        snap.set_enabled(false);
        REQUIRE_FALSE(snap.is_enabled());
    }

    SECTION("can re-enable snapping") {
        snap.set_enabled(false);
        snap.set_enabled(true);
        REQUIRE(snap.is_enabled());
    }
}

TEST_CASE("SnapSystem mode switching", "[pianoroll][snap]") {
    SnapSystem snap;

    SECTION("can set all modes") {
        snap.set_mode(SnapSystem::None);
        REQUIRE(snap.mode() == SnapSystem::None);

        snap.set_mode(SnapSystem::Grid);
        REQUIRE(snap.mode() == SnapSystem::Grid);

        snap.set_mode(SnapSystem::Relative);
        REQUIRE(snap.mode() == SnapSystem::Relative);

        snap.set_mode(SnapSystem::Magnetic);
        REQUIRE(snap.mode() == SnapSystem::Magnetic);

        snap.set_mode(SnapSystem::Adaptive);
        REQUIRE(snap.mode() == SnapSystem::Adaptive);
    }
}

TEST_CASE("SnapSystem resolution", "[pianoroll][snap]") {
    SnapSystem snap;

    SECTION("can set resolution") {
        snap.set_resolution(960);
        REQUIRE(snap.resolution() == 960);
    }

    SECTION("cycle_resolution advances through levels") {
        snap.set_resolution(3840);  // Start at bar level
        snap.cycle_resolution();
        REQUIRE(snap.resolution() == 960);  // Next: beat
        snap.cycle_resolution();
        REQUIRE(snap.resolution() == 480);  // Next: 8th
        snap.cycle_resolution();
        REQUIRE(snap.resolution() == 240);  // Next: 16th
    }

    SECTION("cycle_resolution wraps around") {
        snap.set_resolution(15);  // Last level
        snap.cycle_resolution();
        REQUIRE(snap.resolution() == 3840);  // Wraps to first
    }
}

// ═══════════════════════════════════════════════════════════════
// Snap to grid
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SnapSystem snap to grid", "[pianoroll][snap][grid]") {
    SnapSystem snap;
    snap.set_resolution(240);  // 16th notes

    SECTION("snap aligns to grid") {
        REQUIRE(snap.snap(0) == 0);
        REQUIRE(snap.snap(100) == 0);     // 100 < 120 (half of 240) → snaps to 0
        REQUIRE(snap.snap(120) == 0);     // 120 is exactly half → rounds down to 0
        REQUIRE(snap.snap(240) == 240);
        REQUIRE(snap.snap(241) == 240);   // 241+120=361 → 361/240=1 → 1*240=240
        REQUIRE(snap.snap(359) == 240);   // 359+120=479 → 479/240=1 → 1*240=240
        REQUIRE(snap.snap(360) == 480);   // 360+120=480 → 480/240=2 → 2*240=480
    }

    SECTION("snap disabled returns original value") {
        snap.set_enabled(false);
        REQUIRE(snap.snap(100) == 100);
        REQUIRE(snap.snap(241) == 241);
    }

    SECTION("snap in None mode returns original value") {
        snap.set_mode(SnapSystem::None);
        REQUIRE(snap.snap(100) == 100);
    }

    SECTION("snap with different resolutions") {
        snap.set_resolution(960);  // Quarter notes
        REQUIRE(snap.snap(479) == 0);    // 479+480=959 → 959/960=0 → 0
        REQUIRE(snap.snap(480) == 960);  // 480+480=960 → 960/960=1 → 960
        REQUIRE(snap.snap(1000) == 960);

        snap.set_resolution(3840);  // Bars
        // (1920 + 1920) / 3840 = 3840 / 3840 = 1 → 1 * 3840 = 3840
        REQUIRE(snap.snap(1920) == 3840);
        REQUIRE(snap.snap(5000) == 3840);
    }
}

// ═══════════════════════════════════════════════════════════════
// Snap with strength
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SnapSystem snap with strength", "[pianoroll][snap][strength]") {
    SnapSystem snap;
    snap.set_resolution(240);

    SECTION("strength=0.0 returns original value") {
        REQUIRE(snap.snap(100, 0.0) == 100);
    }

    SECTION("strength=1.0 returns full snap") {
        REQUIRE(snap.snap(100, 1.0) == snap.snap(100));
    }

    SECTION("strength=0.5 blends between original and snapped") {
        // original=100, snapped=240, blended=100*0.5+240*0.5=170
        uint64_t result = snap.snap(100, 0.5);
        REQUIRE(result == 170);
    }

    SECTION("disabled snap ignores strength") {
        snap.set_enabled(false);
        REQUIRE(snap.snap(100, 0.5) == 100);
    }
}

// ═══════════════════════════════════════════════════════════════
// Adaptive resolution
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SnapSystem adaptive resolution", "[pianoroll][snap][adaptive]") {
    SnapSystem snap;

    SECTION("very zoomed out → bar level") {
        REQUIRE(snap.adaptive_resolution(64.0) == 3840);
        REQUIRE(snap.adaptive_resolution(32.0) == 3840);
    }

    SECTION("moderately zoomed out → beat level") {
        REQUIRE(snap.adaptive_resolution(16.0) == 960);
    }

    SECTION("moderate zoom → eighth notes") {
        REQUIRE(snap.adaptive_resolution(8.0) == 480);
    }

    SECTION("typical zoom → sixteenth notes") {
        REQUIRE(snap.adaptive_resolution(4.0) == 240);
    }

    SECTION("zoomed in → 32nd notes") {
        REQUIRE(snap.adaptive_resolution(2.0) == 120);
    }

    SECTION("very zoomed in → 64th notes") {
        REQUIRE(snap.adaptive_resolution(1.0) == 60);
    }

    SECTION("extremely zoomed in → 256th notes") {
        REQUIRE(snap.adaptive_resolution(0.1) == 15);
    }
}

// ═══════════════════════════════════════════════════════════════
// Grid lines
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SnapSystem grid lines", "[pianoroll][snap][gridlines]") {
    SnapSystem snap;
    snap.set_resolution(240);  // 16th notes

    ViewTransform view;
    view.ppqn_per_pixel_x = 4.0;   // 4 PPQN/pixel → moderate zoom
    view.scroll_ppqn = 0.0;

    SECTION("generates lines for visible range") {
        auto lines = snap.get_lines(view, 3840 * 2);  // 2 bars

        // Should have bar lines, beat lines, and subdivision lines.
        REQUIRE_FALSE(lines.empty());

        // First line should be at ppqn=0 (bar 0).
        REQUIRE(lines[0].ppqn == 0);
        REQUIRE(lines[0].type == 0);  // bar

        // Check that there are beat lines.
        bool has_beat = false;
        for (const auto& l : lines) {
            if (l.type == 1) {
                has_beat = true;
                break;
            }
        }
        REQUIRE(has_beat);
    }

    SECTION("lines are sorted by PPQN") {
        auto lines = snap.get_lines(view, 3840 * 4);
        for (size_t i = 1; i < lines.size(); ++i) {
            REQUIRE(lines[i].ppqn >= lines[i - 1].ppqn);
        }
    }

    SECTION("no lines for empty duration") {
        auto lines = snap.get_lines(view, 0);
        // Should still have some lines (viewport-based).
        // The method uses the viewport to determine range, falling back
        // to duration if provided.
        REQUIRE(lines.empty() == false);
    }
}

// ═══════════════════════════════════════════════════════════════
// Resolution index
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SnapSystem resolution_index", "[pianoroll][snap][resolution]") {
    SECTION("known resolutions return correct index") {
        REQUIRE(SnapSystem::resolution_index(3840) == 0);
        REQUIRE(SnapSystem::resolution_index(960) == 1);
        REQUIRE(SnapSystem::resolution_index(480) == 2);
        REQUIRE(SnapSystem::resolution_index(240) == 3);
        REQUIRE(SnapSystem::resolution_index(120) == 4);
        REQUIRE(SnapSystem::resolution_index(60) == 5);
        REQUIRE(SnapSystem::resolution_index(30) == 6);
        REQUIRE(SnapSystem::resolution_index(15) == 7);
    }

    SECTION("unknown resolution returns default (3)") {
        REQUIRE(SnapSystem::resolution_index(100) == 3);
    }
}
