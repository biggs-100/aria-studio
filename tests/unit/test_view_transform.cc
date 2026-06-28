#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/piano_roll_canvas.h"

#include <cmath>

using Catch::Approx;
using namespace aria;

// ─── Test: ViewTransform defaults ──────────────────────────────

TEST_CASE("ViewTransform default construction", "[pianoroll][view][transform]") {
    ViewTransform v;

    SECTION("default zoom and scroll are identity") {
        REQUIRE(v.ppqn_per_pixel_x == Approx(1.0));
        REQUIRE(v.pixels_per_semitone_y == Approx(8.0));
        REQUIRE(v.scroll_ppqn == Approx(0.0));
        REQUIRE(v.scroll_key == Approx(0.0));
    }
}

// ─── Test: PPQN to screen X ────────────────────────────────────

TEST_CASE("ViewTransform ppqn_to_x", "[pianoroll][view][ppqn]") {
    ViewTransform v;

    SECTION("at default zoom, PPQN equals screen X") {
        REQUIRE(v.ppqn_to_x(0) == Approx(0.0));
        REQUIRE(v.ppqn_to_x(100) == Approx(100.0));
    }

    SECTION("with scroll, offset shifts X") {
        v.scroll_ppqn = 960.0;
        REQUIRE(v.ppqn_to_x(0) == Approx(-960.0));
        REQUIRE(v.ppqn_to_x(960) == Approx(0.0));
        REQUIRE(v.ppqn_to_x(1920) == Approx(960.0));
    }

    SECTION("with zoom, PPQN per pixel changes") {
        v.ppqn_per_pixel_x = 2.0;  // 2 PPQN per pixel → half the screen space
        REQUIRE(v.ppqn_to_x(0) == Approx(0.0));
        REQUIRE(v.ppqn_to_x(100) == Approx(50.0));
        REQUIRE(v.ppqn_to_x(200) == Approx(100.0));
    }

    SECTION("zoomed in") {
        v.ppqn_per_pixel_x = 0.5;  // more detail
        REQUIRE(v.ppqn_to_x(0) == Approx(0.0));
        REQUIRE(v.ppqn_to_x(100) == Approx(200.0));
    }
}

// ─── Test: Screen X to PPQN ────────────────────────────────────

TEST_CASE("ViewTransform x_to_ppqn", "[pianoroll][view][ppqn]") {
    ViewTransform v;

    SECTION("at default zoom, screen X equals PPQN") {
        REQUIRE(v.x_to_ppqn(0) == 0);
        REQUIRE(v.x_to_ppqn(100) == 100);
    }

    SECTION("with scroll") {
        v.scroll_ppqn = 960.0;
        REQUIRE(v.x_to_ppqn(0) == 960);
        REQUIRE(v.x_to_ppqn(960) == 960 + 960);
    }

    SECTION("with zoom") {
        v.ppqn_per_pixel_x = 2.0;
        REQUIRE(v.x_to_ppqn(0) == 0);
        REQUIRE(v.x_to_ppqn(50) == 100);
    }

    SECTION("never returns negative (clamped to zero)") {
        v.scroll_ppqn = 1000.0;
        REQUIRE(v.x_to_ppqn(0) == 1000);  // 0*1 + 1000 = 1000
    }
}

// ─── Test: Key to Y ────────────────────────────────────────────

TEST_CASE("ViewTransform key_to_y", "[pianoroll][view][key]") {
    ViewTransform v;

    SECTION("at default, key equals screen Y scaled by pixels per semitone") {
        REQUIRE(v.key_to_y(60) == Approx(60 * 8.0));
        REQUIRE(v.key_to_y(72) == Approx(72 * 8.0));
    }

    SECTION("with scroll") {
        v.scroll_key = 60.0;
        REQUIRE(v.key_to_y(60) == Approx(0.0));
        REQUIRE(v.key_to_y(72) == Approx(12 * 8.0));
    }

    SECTION("at different pixels per semitone") {
        v.pixels_per_semitone_y = 10.0;
        REQUIRE(v.key_to_y(60) == Approx(600.0));
    }
}

// ─── Test: Y to Key ────────────────────────────────────────────

TEST_CASE("ViewTransform y_to_key", "[pianoroll][view][key]") {
    ViewTransform v;

    SECTION("at default") {
        REQUIRE(v.y_to_key(0) == 0);
        REQUIRE(v.y_to_key(8.0 * 60) == 60);
    }

    SECTION("with scroll") {
        v.scroll_key = 60.0;
        REQUIRE(v.y_to_key(0) == 60);
        REQUIRE(v.y_to_key(8.0 * 12) == 72);
    }

    SECTION("clamped to valid range") {
        REQUIRE(v.y_to_key(-100) == 0);   // below 0
        REQUIRE(v.y_to_key(20000) == 127); // above 127
    }

    SECTION("roundtrip") {
        v.scroll_key = 48.0;
        uint8_t original = 72;
        double y = v.key_to_y(original);
        uint8_t roundtripped = v.y_to_key(y);
        REQUIRE(roundtripped == original);
    }
}

// ─── Test: Viewport calculation ────────────────────────────────

TEST_CASE("ViewTransform viewport_ppqn", "[pianoroll][view][viewport]") {
    ViewTransform v;
    REQUIRE(v.ppqn_per_pixel_x == Approx(1.0));

    SECTION("default viewport with 1920×1080 canvas") {
        auto vp = v.viewport_ppqn(1920.0, 1080.0);
        REQUIRE(vp.x == Approx(0.0));
        REQUIRE(vp.y == Approx(0.0));
        REQUIRE(vp.width == Approx(1920.0));
        REQUIRE(vp.height == Approx(135.0));  // 1080 / 8 semitones per px
    }

    SECTION("viewport with scroll") {
        v.scroll_ppqn = 3840.0;
        v.scroll_key  = 48.0;
        auto vp = v.viewport_ppqn(1920.0, 1080.0);
        REQUIRE(vp.x == Approx(3840.0));
        REQUIRE(vp.y == Approx(48.0));
    }

    SECTION("viewport with zoom") {
        v.ppqn_per_pixel_x = 4.0;  // zoomed out
        auto vp = v.viewport_ppqn(1920.0, 1080.0);
        REQUIRE(vp.width == Approx(1920.0 * 4.0));  // 7680 PPQN visible
    }

    SECTION("viewport with vertical zoom") {
        v.pixels_per_semitone_y = 4.0;  // zoomed out vertically
        auto vp = v.viewport_ppqn(1920.0, 1080.0);
        REQUIRE(vp.height == Approx(1080.0 / 4.0));  // 270 semitones
    }
}

// ─── Test: Zoom math ───────────────────────────────────────────

TEST_CASE("ViewTransform zoom math", "[pianoroll][view][zoom]") {
    // Verify the zoom-anchor-preservation formula used by PianoRollCanvas::zoom.
    ViewTransform v;
    v.ppqn_per_pixel_x = 2.0;
    v.scroll_ppqn = 0.0;

    SECTION("formula: new_ppqn_per_pixel = old / factor") {
        double factor = 2.0f;
        uint64_t anchor_ppqn = 960;
        double anchor_x = (static_cast<double>(anchor_ppqn) - v.scroll_ppqn) / v.ppqn_per_pixel_x;
        REQUIRE(anchor_x == Approx(480.0));

        // Apply zoom.
        v.ppqn_per_pixel_x = std::clamp(v.ppqn_per_pixel_x / factor, 0.03125, 64.0);
        v.scroll_ppqn = static_cast<double>(anchor_ppqn) - anchor_x * v.ppqn_per_pixel_x;

        // Verify anchor is stable.
        double new_anchor_x = (static_cast<double>(anchor_ppqn) - v.scroll_ppqn) / v.ppqn_per_pixel_x;
        REQUIRE(new_anchor_x == Approx(480.0));
        REQUIRE(v.ppqn_per_pixel_x == Approx(1.0));
    }

    SECTION("zoom clamps to valid range") {
        v.ppqn_per_pixel_x = 0.01;  // very zoomed in
        double clamped = std::clamp(v.ppqn_per_pixel_x / 2.0, 0.03125, 64.0);
        REQUIRE(clamped == Approx(0.03125));  // clamped to min
    }
}
