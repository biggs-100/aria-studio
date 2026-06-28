#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/expression_lane.h"
#include "pianoroll/piano_roll_canvas.h"  // For ViewTransform

#include <cmath>
#include <cstdint>
#include <vector>

using Catch::Approx;
using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve construction and defaults
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve construction", "[pianoroll][expression][curve]") {
    ExpressionCurve curve;

    SECTION("starts empty") {
        REQUIRE(curve.empty());
        REQUIRE(curve.size() == 0);
    }

    SECTION("evaluate on empty returns 0.0") {
        REQUIRE(curve.evaluate(0) == Approx(0.0));
        REQUIRE(curve.evaluate(960) == Approx(0.0));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve add_point
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve add_point", "[pianoroll][expression][add]") {
    ExpressionCurve curve;

    SECTION("adds points in order") {
        curve.add_point(960, 1.0);
        curve.add_point(0, 0.0);
        curve.add_point(480, 0.5);

        REQUIRE(curve.size() == 3);
        REQUIRE(curve.points()[0].ppqn == 0);
        REQUIRE(curve.points()[1].ppqn == 480);
        REQUIRE(curve.points()[2].ppqn == 960);
    }

    SECTION("replaces existing point at same PPQN") {
        curve.add_point(480, 0.5);
        curve.add_point(480, 1.0);
        REQUIRE(curve.size() == 1);
        REQUIRE(curve.points()[0].value == Approx(1.0));
    }

    SECTION("clamps value to [0, 1]") {
        curve.add_point(0, 1.5);
        REQUIRE(curve.points()[0].value == Approx(1.0));

        curve.add_point(480, -0.5);
        REQUIRE(curve.points()[1].value == Approx(0.0));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve remove_point
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve remove_point", "[pianoroll][expression][remove]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0);
    curve.add_point(480, 0.5);
    curve.add_point(960, 1.0);

    SECTION("removes exact match") {
        bool ok = curve.remove_point(480);
        REQUIRE(ok);
        REQUIRE(curve.size() == 2);
        REQUIRE(curve.points()[0].ppqn == 0);
        REQUIRE(curve.points()[1].ppqn == 960);
    }

    SECTION("removes closest within tolerance") {
        bool ok = curve.remove_point(478, 48);  // Within 48 PPQN of 480
        REQUIRE(ok);
        REQUIRE(curve.size() == 2);
    }

    SECTION("fails when outside tolerance") {
        bool ok = curve.remove_point(400, 10);  // No point within 10 PPQN
        REQUIRE(!ok);
        REQUIRE(curve.size() == 3);
    }

    SECTION("returns false on empty curve") {
        ExpressionCurve empty;
        REQUIRE(!empty.remove_point(0));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve move_point
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve move_point", "[pianoroll][expression][move]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0);
    curve.add_point(480, 0.5);
    curve.add_point(960, 1.0);

    SECTION("moves to new PPQN and value") {
        bool ok = curve.move_point(480, 500, 0.75);
        REQUIRE(ok);
        REQUIRE(curve.size() == 3);
        // Check ordering is preserved
        REQUIRE(curve.points()[0].ppqn == 0);
        REQUIRE(curve.points()[1].ppqn == 500);
        REQUIRE(curve.points()[2].ppqn == 960);
        REQUIRE(curve.points()[1].value == Approx(0.75));
    }

    SECTION("returns false for non-existent point") {
        REQUIRE(!curve.move_point(999, 1000, 0.5));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve evaluate — Linear
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve evaluate linear", "[pianoroll][expression][evaluate]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0);
    curve.add_point(960, 1.0);

    SECTION("evaluates at start point") {
        REQUIRE(curve.evaluate(0) == Approx(0.0));
    }

    SECTION("evaluates at end point") {
        REQUIRE(curve.evaluate(960) == Approx(1.0));
    }

    SECTION("evaluates at midpoint") {
        REQUIRE(curve.evaluate(480) == Approx(0.5));
    }

    SECTION("evaluates at quarter point") {
        REQUIRE(curve.evaluate(240) == Approx(0.25));
    }

    SECTION("before first point holds first value") {
        REQUIRE(curve.evaluate(0) == Approx(0.0));
    }

    SECTION("single point holds constant") {
        ExpressionCurve single;
        single.add_point(480, 0.75);
        REQUIRE(single.evaluate(0) == Approx(0.75));
        REQUIRE(single.evaluate(480) == Approx(0.75));
        REQUIRE(single.evaluate(960) == Approx(0.75));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve evaluate — Step
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve evaluate step", "[pianoroll][expression][evaluate]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0, InterpolationType::Step);
    curve.add_point(480, 1.0, InterpolationType::Step);

    SECTION("before step holds previous value") {
        REQUIRE(curve.evaluate(0) == Approx(0.0));
        REQUIRE(curve.evaluate(240) == Approx(0.0));
    }

    SECTION("at step point returns step value") {
        REQUIRE(curve.evaluate(479) == Approx(0.0));
        REQUIRE(curve.evaluate(480) == Approx(1.0));
        REQUIRE(curve.evaluate(960) == Approx(1.0));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve evaluate — EaseIn, EaseOut, EaseInOut
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve evaluate ease", "[pianoroll][expression][evaluate]") {
    SECTION("EaseIn (quadratic in)") {
        ExpressionCurve curve;
        curve.add_point(0, 0.0, InterpolationType::EaseIn);
        curve.add_point(960, 1.0, InterpolationType::EaseIn);

        // At t=0.5, t^2 = 0.25
        REQUIRE(curve.evaluate(480) == Approx(0.25));
    }

    SECTION("EaseOut (quadratic out)") {
        ExpressionCurve curve;
        curve.add_point(0, 0.0, InterpolationType::EaseOut);
        curve.add_point(960, 1.0, InterpolationType::EaseOut);

        // At t=0.5, t*(2-t) = 0.75
        REQUIRE(curve.evaluate(480) == Approx(0.75));
    }

    SECTION("EaseInOut (smoothstep)") {
        ExpressionCurve curve;
        curve.add_point(0, 0.0, InterpolationType::EaseInOut);
        curve.add_point(960, 1.0, InterpolationType::EaseInOut);

        // At t=0.5, t^2*(3-2t) = 0.5
        REQUIRE(curve.evaluate(480) == Approx(0.5));
        // At t=0.25, t^2*(3-2t) = 0.15625
        REQUIRE(curve.evaluate(240) == Approx(0.15625));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve evaluate — Hold
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve evaluate hold", "[pianoroll][expression][evaluate]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0, InterpolationType::Hold);
    curve.add_point(960, 1.0, InterpolationType::Hold);

    SECTION("holds value until very close to next point") {
        REQUIRE(curve.evaluate(0) == Approx(0.0));
        REQUIRE(curve.evaluate(480) == Approx(0.0));
        REQUIRE(curve.evaluate(959) == Approx(0.0));
        REQUIRE(curve.evaluate(960) == Approx(1.0));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve evaluate — Smooth (Catmull-Rom)
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve evaluate smooth", "[pianoroll][expression][evaluate]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0, InterpolationType::Smooth);
    curve.add_point(480, 0.5, InterpolationType::Smooth);
    curve.add_point(960, 1.0, InterpolationType::Smooth);

    SECTION("evaluates at endpoints") {
        REQUIRE(curve.evaluate(0) == Approx(0.0));
        REQUIRE(curve.evaluate(480) == Approx(0.5));
        REQUIRE(curve.evaluate(960) == Approx(1.0));
    }

    SECTION("midpoint is approximately linear for evenly spaced") {
        double v = curve.evaluate(240);
        // With Catmull-Rom and evenly spaced equal-valued points,
        // the midpoint should be roughly halfway.
        REQUIRE(v == Approx(0.25).margin(0.1));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve generate_line_strip
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve generate_line_strip", "[pianoroll][expression][strip]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0);
    curve.add_point(960, 1.0);

    SECTION("returns points for visible range") {
        auto strip = curve.generate_line_strip(0, 960, 1.0, 100.0);
        REQUIRE(!strip.empty());

        // First point should be at x=0, y=100 (value 0 flipped)
        // Last point should be at x=960, y=0 (value 1 flipped)
        REQUIRE(strip.front().x == Approx(0.0));
        REQUIRE(strip.front().y == Approx(100.0));
        REQUIRE(strip.back().x == Approx(960.0));
        REQUIRE(strip.back().y == Approx(0.0));
    }

    SECTION("empty curve returns empty strip") {
        ExpressionCurve empty;
        auto strip = empty.generate_line_strip(0, 960, 1.0, 100.0);
        REQUIRE(strip.empty());
    }

    SECTION("capped at 8192 segments") {
        auto strip = curve.generate_line_strip(0, 9600000, 0.001, 100.0);
        // Should not have more than 8193 points (8192 segments + 1)
        REQUIRE(strip.size() <= 8193);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionLane construction and management
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionLane construction", "[pianoroll][expression][lane]") {
    ExpressionLane lane;

    SECTION("default type is PitchBend") {
        REQUIRE(lane.type() == ExpressionLane::PitchBend);
    }

    SECTION("default height is 60") {
        REQUIRE(lane.height() == Approx(60.0f));
    }

    SECTION("set_type changes type") {
        lane.set_type(ExpressionLane::CC, 74);
        REQUIRE(lane.type() == ExpressionLane::CC);
        REQUIRE(lane.cc_number() == 74);
    }

    SECTION("set_type doesn't store cc for non-CC types") {
        lane.set_type(ExpressionLane::PitchBend, 99);
        REQUIRE(lane.cc_number() == 0);
    }

    SECTION("set_height changes height") {
        lane.set_height(80.0f);
        REQUIRE(lane.height() == Approx(80.0f));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionLane label()
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionLane label", "[pianoroll][expression][label]") {
    ExpressionLane lane;

    SECTION("PitchBend label") {
        lane.set_type(ExpressionLane::PitchBend);
        REQUIRE(lane.label() == "Pitch Bend");
    }

    SECTION("CC label includes number") {
        lane.set_type(ExpressionLane::CC, 74);
        REQUIRE(lane.label() == "CC 74");
    }

    SECTION("ChannelPressure label") {
        lane.set_type(ExpressionLane::ChannelPressure);
        REQUIRE(lane.label() == "Channel Pressure");
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionLane mouse interaction
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionLane mouse input", "[pianoroll][expression][mouse]") {
    ExpressionLane lane;
    lane.set_height(60.0f);

    ViewTransform view;

    SECTION("mouse_down on empty lane creates first point") {
        lane.on_mouse_down(0.0f, 30.0f, view);
        REQUIRE(lane.curve().size() == 1);
        REQUIRE(lane.curve().points()[0].ppqn == 0);
        // y=30 in a 60-height lane → value = 1 - 30/60 = 0.5
        REQUIRE(lane.curve().points()[0].value == Approx(0.5));
    }

    SECTION("mouse_down at top creates point at value 1.0") {
        lane.on_mouse_down(0.0f, 0.0f, view);
        REQUIRE(lane.curve().points()[0].value == Approx(1.0));
    }

    SECTION("mouse_down at bottom creates point at value 0.0") {
        lane.on_mouse_down(0.0f, 60.0f, view);
        REQUIRE(lane.curve().points()[0].value == Approx(0.0));
    }

    SECTION("mouse_move updates point position") {
        lane.on_mouse_down(0.0f, 30.0f, view);
        lane.on_mouse_move(480.0f, 15.0f, view);
        REQUIRE(lane.curve().size() == 1);
        REQUIRE(lane.curve().points()[0].ppqn == 480);
        // y=15 in a 60-height lane → value = 1 - 15/60 = 0.75
        REQUIRE(lane.curve().points()[0].value == Approx(0.75));
    }

    SECTION("mouse_up resets drag state") {
        lane.on_mouse_down(0.0f, 30.0f, view);
        lane.on_mouse_up();
        // After mouse_up, subsequent moves should not crash
        lane.on_mouse_move(100.0f, 50.0f, view);
        REQUIRE(lane.curve().size() == 1);  // Point still exists
    }

    SECTION("mouse_down near existing point starts drag") {
        lane.on_mouse_down(0.0f, 30.0f, view);  // Create point at ppqn=0, value=0.5
        // Click near the existing point (within 10px)
        lane.on_mouse_down(0.0f, 30.0f, view);  // Same position should hit-test it
        REQUIRE(lane.curve().size() == 1);       // No new point created
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionLaneManager
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionLaneManager", "[pianoroll][expression][manager]") {
    ExpressionLaneManager mgr;

    SECTION("starts with no lanes") {
        REQUIRE(mgr.lane_count() == 0);
    }

    SECTION("add_lane creates a lane") {
        mgr.add_lane(ExpressionLane::PitchBend);
        REQUIRE(mgr.lane_count() == 1);

        ExpressionLane* lane = mgr.get_lane(0);
        REQUIRE(lane != nullptr);
        REQUIRE(lane->type() == ExpressionLane::PitchBend);
    }

    SECTION("add multiple lanes") {
        mgr.add_lane(ExpressionLane::PitchBend);
        mgr.add_lane(ExpressionLane::CC, 74);
        mgr.add_lane(ExpressionLane::ChannelPressure);

        REQUIRE(mgr.lane_count() == 3);
        REQUIRE(mgr.get_lane(0)->type() == ExpressionLane::PitchBend);
        REQUIRE(mgr.get_lane(1)->type() == ExpressionLane::CC);
        REQUIRE(mgr.get_lane(1)->cc_number() == 74);
        REQUIRE(mgr.get_lane(2)->type() == ExpressionLane::ChannelPressure);
    }

    SECTION("remove_lane removes by index") {
        mgr.add_lane(ExpressionLane::PitchBend);
        mgr.add_lane(ExpressionLane::CC, 74);
        mgr.remove_lane(0);

        REQUIRE(mgr.lane_count() == 1);
        REQUIRE(mgr.get_lane(0)->type() == ExpressionLane::CC);
    }

    SECTION("remove_lane out of bounds is no-op") {
        mgr.remove_lane(5);
        REQUIRE(mgr.lane_count() == 0);
    }

    SECTION("get_lane returns nullptr for out of bounds") {
        REQUIRE(mgr.get_lane(0) == nullptr);
        REQUIRE(mgr.get_lane(100) == nullptr);
    }

    SECTION("clear removes all lanes") {
        mgr.add_lane(ExpressionLane::PitchBend);
        mgr.add_lane(ExpressionLane::CC, 1);
        mgr.clear();
        REQUIRE(mgr.lane_count() == 0);
    }

    SECTION("total_height sums all lane heights") {
        mgr.add_lane(ExpressionLane::PitchBend);
        mgr.add_lane(ExpressionLane::CC, 74);
        // Default height is 60 each
        REQUIRE(mgr.total_height() == Approx(120.0f));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionLaneManager input routing
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionLaneManager input routing", "[pianoroll][expression][routing]") {
    ExpressionLaneManager mgr;
    mgr.add_lane(ExpressionLane::PitchBend);  // height 60
    mgr.add_lane(ExpressionLane::CC, 74);     // height 60

    ViewTransform view;

    SECTION("handle_mouse_down on first lane") {
        // y=30 is in the middle of the first lane (0-60)
        int idx = mgr.handle_mouse_down(0.0f, 30.0f, view);
        REQUIRE(idx == 0);
        // First lane should have a point
        REQUIRE(mgr.get_lane(0)->curve().size() == 1);
        // Second lane should be empty
        REQUIRE(mgr.get_lane(1)->curve().size() == 0);
    }

    SECTION("handle_mouse_down on second lane") {
        // y=90 is in the middle of the second lane (60-120)
        int idx = mgr.handle_mouse_down(0.0f, 90.0f, view);
        REQUIRE(idx == 1);
        REQUIRE(mgr.get_lane(0)->curve().size() == 0);
        REQUIRE(mgr.get_lane(1)->curve().size() == 1);
    }

    SECTION("handle_mouse_down outside all lanes returns -1") {
        int idx = mgr.handle_mouse_down(0.0f, 200.0f, view);
        REQUIRE(idx == -1);
    }

    SECTION("mouse move/up on active lane") {
        mgr.handle_mouse_down(0.0f, 30.0f, view);  // First lane
        mgr.handle_mouse_move(480.0f, 15.0f, view);
        REQUIRE(mgr.get_lane(0)->curve().points()[0].ppqn == 480);
        REQUIRE(mgr.get_lane(0)->curve().points()[0].value == Approx(0.75));

        mgr.handle_mouse_up();
        // After mouse_up, move is no-op
        mgr.handle_mouse_move(960.0f, 0.0f, view);
        // Point should still be at previous position
        REQUIRE(mgr.get_lane(0)->curve().points()[0].ppqn == 480);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: Interpolation constants and values
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve interpolation types", "[pianoroll][expression][interp]") {
    SECTION("enum values are distinct") {
        REQUIRE(static_cast<int>(InterpolationType::Step) !=
                static_cast<int>(InterpolationType::Linear));
        REQUIRE(static_cast<int>(InterpolationType::Linear) !=
                static_cast<int>(InterpolationType::Smooth));
    }

    SECTION("multi-point curve evaluated between points") {
        ExpressionCurve curve;
        curve.add_point(0, 0.0, InterpolationType::Step);
        curve.add_point(480, 0.5, InterpolationType::Linear);
        curve.add_point(960, 1.0, InterpolationType::EaseInOut);

        // Before first point
        REQUIRE(curve.evaluate(0) == Approx(0.0));

        // Step segment: should hold at first value
        REQUIRE(curve.evaluate(240) == Approx(0.0));
        REQUIRE(curve.evaluate(479) == Approx(0.0));

        // Linear segment from 480 to 960
        REQUIRE(curve.evaluate(480) == Approx(0.5));
        REQUIRE(curve.evaluate(720) == Approx(0.75));
        REQUIRE(curve.evaluate(960) == Approx(1.0));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve set_interpolation
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve set_interpolation", "[pianoroll][expression][interp]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0);
    curve.add_point(960, 1.0);

    SECTION("changes interpolation type") {
        REQUIRE(curve.set_interpolation(0, InterpolationType::Step));
        // After step change, midpoint should hold previous value
        REQUIRE(curve.evaluate(480) == Approx(0.0));
    }

    SECTION("returns false for non-existent point") {
        REQUIRE(!curve.set_interpolation(500, InterpolationType::Smooth));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ExpressionCurve clear
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ExpressionCurve clear", "[pianoroll][expression][clear]") {
    ExpressionCurve curve;
    curve.add_point(0, 0.0);
    curve.add_point(960, 1.0);

    REQUIRE(curve.size() == 2);
    curve.clear();
    REQUIRE(curve.empty());
    REQUIRE(curve.evaluate(0) == Approx(0.0));
}
