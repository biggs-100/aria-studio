#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/velocity_lane.h"
#include "pianoroll/note_collection.h"
#include "pianoroll/piano_roll_canvas.h"  // For ViewTransform

#include <cmath>
#include <cstdint>
#include <vector>

using Catch::Approx;
using namespace aria;

// ─── Helpers ─────────────────────────────────────────────────────

static Note make_note(uint64_t start, uint64_t dur, uint8_t key,
                       uint8_t vel = 100)
{
    Note n;
    n.start_ppqn    = start;
    n.duration_ppqn = dur;
    n.key           = key;
    n.velocity      = vel;
    return n;
}

static ViewTransform default_view() {
    ViewTransform v;
    v.ppqn_per_pixel_x      = 1.0;
    v.pixels_per_semitone_y = 8.0;
    v.scroll_ppqn           = 0.0;
    v.scroll_key            = 0.0;
    return v;
}

// ═══════════════════════════════════════════════════════════════
// Test: Construction and defaults
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane construction", "[pianoroll][velocity][construction]") {
    VelocityLane lane;

    SECTION("default height is 80") {
        REQUIRE(lane.height() == Approx(80.0f));
    }

    SECTION("notes is null by default") {
        REQUIRE(lane.notes() == nullptr);
    }

    SECTION("set_height changes height") {
        lane.set_height(100.0f);
        REQUIRE(lane.height() == Approx(100.0f));
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: set_velocity
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane set_velocity", "[pianoroll][velocity][set]") {
    NoteCollection col;
    auto id = col.add(make_note(0, 960, 60, 100));

    VelocityLane lane;
    lane.set_notes(&col);

    SECTION("sets velocity on existing note") {
        lane.set_velocity(id, 127);
        Note* n = col.find(id);
        REQUIRE(n != nullptr);
        REQUIRE(n->velocity == 127);
    }

    SECTION("clamps to 127") {
        lane.set_velocity(id, 255);
        Note* n = col.find(id);
        REQUIRE(n->velocity == 127);
    }

    SECTION("no crash on unknown ID") {
        lane.set_velocity(NoteID{999}, 64);
        // No crash = pass
        REQUIRE(true);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: ramp_selection
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane ramp_selection", "[pianoroll][velocity][ramp]") {
    VelocityLane lane;

    SECTION("ramp across 5 notes") {
        std::vector<Note> notes;
        for (int i = 0; i < 5; ++i) {
            Note n;
            n.start_ppqn    = static_cast<uint64_t>(i) * 960;
            n.duration_ppqn = 480;
            n.key           = 60;
            n.velocity      = 100;
            notes.push_back(n);
        }

        std::vector<Note*> ptrs;
        for (auto& n : notes) ptrs.push_back(&n);

        lane.ramp_selection(ptrs, 0, 127);

        REQUIRE(notes[0].velocity == 0);
        REQUIRE(notes[1].velocity == 32);   // 127/4 ≈ 31.75 → 32
        REQUIRE(notes[2].velocity == 64);   // 127/2 = 63.5 → 64
        REQUIRE(notes[3].velocity == 95);   // 127*3/4 = 95.25 → 95
        REQUIRE(notes[4].velocity == 127);
    }

    SECTION("single note ramp uses start_vel") {
        std::vector<Note> notes;
        Note n;
        n.start_ppqn = 0;
        n.velocity   = 100;
        notes.push_back(n);

        std::vector<Note*> ptrs = {&notes[0]};
        lane.ramp_selection(ptrs, 64, 127);
        REQUIRE(notes[0].velocity == 64);
    }

    SECTION("empty list does not crash") {
        std::vector<Note*> empty;
        lane.ramp_selection(empty, 0, 127);
        REQUIRE(true);  // No crash
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: scale_selection
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane scale_selection", "[pianoroll][velocity][scale]") {
    VelocityLane lane;

    std::vector<Note> notes;
    for (int v = 10; v <= 100; v += 30) {
        Note n;
        n.velocity = static_cast<uint8_t>(v);
        notes.push_back(n);
    }

    std::vector<Note*> ptrs;
    for (auto& n : notes) ptrs.push_back(&n);

    SECTION("scale by 0.5") {
        lane.scale_selection(ptrs, 0.5);
        REQUIRE(notes[0].velocity == 5);   // 10 * 0.5
        REQUIRE(notes[1].velocity == 20);  // 40 * 0.5
        REQUIRE(notes[2].velocity == 35);  // 70 * 0.5
        REQUIRE(notes[3].velocity == 50);  // 100 * 0.5
    }

    SECTION("scale by 2.0 (clamped)") {
        lane.scale_selection(ptrs, 2.0);
        REQUIRE(notes[0].velocity == 20);   // 10 * 2
        REQUIRE(notes[1].velocity == 80);   // 40 * 2
        REQUIRE(notes[2].velocity == 127);  // 70 * 2 = 140 → clamped to 127
        REQUIRE(notes[3].velocity == 127);  // 100 * 2 = 200 → clamped to 127
    }

    SECTION("scale factor clamped to [0, 2]") {
        lane.scale_selection(ptrs, -1.0);
        REQUIRE(notes[0].velocity == 0);   // clamped to 0
    }

    SECTION("empty list does not crash") {
        std::vector<Note*> empty;
        lane.scale_selection(empty, 0.5);
        REQUIRE(true);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: randomize_selection
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane randomize_selection", "[pianoroll][velocity][randomize]") {
    VelocityLane lane;

    std::vector<Note> notes(100);
    for (auto& n : notes) {
        n.velocity = 64;
    }

    std::vector<Note*> ptrs;
    for (auto& n : notes) ptrs.push_back(&n);

    SECTION("all values are within [20, 40]") {
        lane.randomize_selection(ptrs, 20, 40);
        for (const auto& n : notes) {
            REQUIRE(n.velocity >= 20);
            REQUIRE(n.velocity <= 40);
        }
    }

    SECTION("all values are within [0, 0] (always 0)") {
        lane.randomize_selection(ptrs, 0, 0);
        for (const auto& n : notes) {
            REQUIRE(n.velocity == 0);
        }
    }

    SECTION("clamps max to 127") {
        lane.randomize_selection(ptrs, 0, 200);
        for (const auto& n : notes) {
            REQUIRE(n.velocity <= 127);
        }
    }

    SECTION("empty list does not crash") {
        std::vector<Note*> empty;
        lane.randomize_selection(empty, 0, 127);
        REQUIRE(true);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: reverse_selection
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane reverse_selection", "[pianoroll][velocity][reverse]") {
    VelocityLane lane;

    SECTION("reverses 5 notes") {
        std::vector<Note> notes(5);
        uint8_t vels[] = {10, 30, 50, 80, 127};
        for (size_t i = 0; i < 5; ++i) {
            notes[i].start_ppqn = static_cast<uint64_t>(i) * 960;
            notes[i].velocity   = vels[i];
        }

        std::vector<Note*> ptrs;
        for (auto& n : notes) ptrs.push_back(&n);

        lane.reverse_selection(ptrs);

        REQUIRE(notes[0].velocity == 127);
        REQUIRE(notes[1].velocity == 80);
        REQUIRE(notes[2].velocity == 50);
        REQUIRE(notes[3].velocity == 30);
        REQUIRE(notes[4].velocity == 10);
    }

    SECTION("single note stays the same") {
        Note n;
        n.velocity = 64;
        std::vector<Note*> ptrs = {&n};
        lane.reverse_selection(ptrs);
        REQUIRE(n.velocity == 64);
    }

    SECTION("empty list does not crash") {
        std::vector<Note*> empty;
        lane.reverse_selection(empty);
        REQUIRE(true);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: apply_curve
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane apply_curve", "[pianoroll][velocity][curve]") {
    VelocityLane lane;

    std::vector<Note> notes(5);
    for (int i = 0; i < 5; ++i) {
        notes[static_cast<size_t>(i)].start_ppqn = static_cast<uint64_t>(i) * 960;
        notes[static_cast<size_t>(i)].velocity   = 100;
    }

    std::vector<Note*> ptrs;
    for (auto& n : notes) ptrs.push_back(&n);

    SECTION("Linear curve from 0 to 127") {
        lane.apply_curve(ptrs, 0, 127, VelocityLane::CurveType::Linear);
        REQUIRE(notes[0].velocity == 0);
        REQUIRE(notes[2].velocity == 64);
        REQUIRE(notes[4].velocity == 127);
    }

    SECTION("Exponential curve (start weighted)") {
        lane.apply_curve(ptrs, 0, 127, VelocityLane::CurveType::Exponential);
        // t: 0.0, 0.25, 0.5, 0.75, 1.0
        // t^2: 0.0, 0.0625, 0.25, 0.5625, 1.0
        REQUIRE(notes[0].velocity == 0);
        REQUIRE(notes[1].velocity == 8);    // 127 * 0.0625 = 7.9375 → 8
        REQUIRE(notes[2].velocity == 32);   // 127 * 0.25 = 31.75 → 32
        REQUIRE(notes[3].velocity == 71);   // 127 * 0.5625 = 71.4375 → 71
        REQUIRE(notes[4].velocity == 127);
    }

    SECTION("Logarithmic curve (end weighted)") {
        lane.apply_curve(ptrs, 0, 127, VelocityLane::CurveType::Logarithmic);
        // sqrt(t): 0.0, 0.5, 0.707, 0.866, 1.0
        REQUIRE(notes[0].velocity == 0);
        REQUIRE(notes[1].velocity == 64);   // 127 * 0.5 = 63.5 → 64
        REQUIRE(notes[2].velocity == 90);   // 127 * 0.707 ≈ 89.8 → 90
        REQUIRE(notes[3].velocity == 110);  // 127 * 0.866 ≈ 110.0 → 110
        REQUIRE(notes[4].velocity == 127);
    }

    SECTION("SCurve (smoothstep)") {
        lane.apply_curve(ptrs, 0, 127, VelocityLane::CurveType::SCurve);
        // t: 0.0, 0.25, 0.5, 0.75, 1.0
        // t^2(3-2t): 0.0, 0.15625, 0.5, 0.84375, 1.0
        REQUIRE(notes[0].velocity == 0);
        REQUIRE(notes[1].velocity == 20);   // 127 * 0.15625 = 19.84 → 20
        REQUIRE(notes[2].velocity == 64);   // 127 * 0.5 = 63.5 → 64
        REQUIRE(notes[3].velocity == 107);  // 127 * 0.84375 = 107.16 → 107
        REQUIRE(notes[4].velocity == 127);
    }

    SECTION("single note") {
        std::vector<Note> single(1);
        single[0].start_ppqn = 0;
        std::vector<Note*> single_ptr = {&single[0]};
        lane.apply_curve(single_ptr, 64, 127, VelocityLane::CurveType::Linear);
        REQUIRE(single[0].velocity == 64);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: mouse interaction
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane mouse input", "[pianoroll][velocity][mouse]") {
    NoteCollection col;
    auto id = col.add(make_note(0, 960, 60, 64));

    VelocityLane lane;
    lane.set_notes(&col);
    lane.set_height(80.0f);

    ViewTransform view = default_view();

    SECTION("mouse_down on note sets velocity") {
        // Click at x=0 (note start), y=40 (halfway → velocity 64)
        lane.on_mouse_down(0.0f, 40.0f, view);
        Note* n = col.find(id);
        REQUIRE(n != nullptr);
        REQUIRE(n->velocity == 64);  // 1.0 - (40/80) = 0.5 → 63.5 → 64
    }

    SECTION("mouse_down at top of lane = velocity 127") {
        lane.on_mouse_down(0.0f, 0.0f, view);
        Note* n = col.find(id);
        REQUIRE(n->velocity == 127);
    }

    SECTION("mouse_down at bottom of lane = velocity 0") {
        lane.on_mouse_down(0.0f, 80.0f, view);
        Note* n = col.find(id);
        REQUIRE(n->velocity == 0);
    }

    SECTION("no crash when no notes bound") {
        VelocityLane empty_lane;
        empty_lane.on_mouse_down(0.0f, 40.0f, view);
        REQUIRE(true);
    }

    SECTION("mouse_up resets drag state") {
        lane.on_mouse_down(0.0f, 40.0f, view);
        lane.on_mouse_up();
        // After mouse_up, drag is inactive — no crash on subsequent move
        lane.on_mouse_move(10.0f, 20.0f, view);
        REQUIRE(true);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test: velocity_color helper
// ═══════════════════════════════════════════════════════════════

TEST_CASE("VelocityLane::velocity_color", "[pianoroll][velocity][color]") {
    SECTION("low velocity = blue-ish") {
        Rgba c = VelocityLane::velocity_color(0);
        // Blue channel should dominate
        REQUIRE(c.b > c.r);
        REQUIRE(c.b > c.g);
        REQUIRE(c.a == Approx(1.0f));
    }

    SECTION("mid velocity = green-ish") {
        Rgba c = VelocityLane::velocity_color(64);
        // At middle, green is strongest
        REQUIRE(c.g > c.r);
        REQUIRE(c.g > c.b);
    }

    SECTION("high velocity = red-ish") {
        Rgba c = VelocityLane::velocity_color(127);
        // Red channel should dominate
        REQUIRE(c.r > c.g);
        REQUIRE(c.r > c.b);
    }

    SECTION("always fully opaque") {
        for (int v = 0; v <= 127; v += 10) {
            Rgba c = VelocityLane::velocity_color(static_cast<uint8_t>(v));
            REQUIRE(c.a == Approx(1.0f));
        }
    }
}
