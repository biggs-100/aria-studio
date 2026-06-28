#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/note.h"

#include <unordered_set>

using Catch::Approx;
using namespace aria;

// ─── Test: NoteID ──────────────────────────────────────────────

TEST_CASE("NoteID construction and comparison", "[pianoroll][note][id]") {
    SECTION("default NoteID is zero") {
        NoteID id;
        REQUIRE(id.value == 0);
    }

    SECTION("NoteIDs with same value are equal") {
        NoteID a{42};
        NoteID b{42};
        REQUIRE(a == b);
        REQUIRE_FALSE(a != b);
    }

    SECTION("NoteIDs with different values are not equal") {
        NoteID a{1};
        NoteID b{2};
        REQUIRE(a != b);
        REQUIRE_FALSE(a == b);
    }

    SECTION("NoteIDs can be ordered") {
        NoteID a{10};
        NoteID b{20};
        REQUIRE(a < b);
        REQUIRE_FALSE(b < a);
    }

    SECTION("NoteID works with unordered_set") {
        std::unordered_set<NoteID> ids;
        ids.insert(NoteID{1});
        ids.insert(NoteID{2});
        ids.insert(NoteID{1});  // duplicate
        REQUIRE(ids.size() == 2);
    }
}

// ─── Test: MPEExpression ───────────────────────────────────────

TEST_CASE("MPEExpression defaults and activity", "[pianoroll][note][mpe]") {
    SECTION("default values are zero or centre") {
        MPEExpression expr;
        REQUIRE(expr.pitch_bend == 0);
        REQUIRE(expr.timbre == 64);
        REQUIRE(expr.pressure == 0);
    }

    SECTION("has_data returns false for defaults") {
        MPEExpression expr;
        REQUIRE_FALSE(expr.has_data());
    }

    SECTION("non-zero pitch_bend is active") {
        MPEExpression expr;
        expr.pitch_bend = 100;
        REQUIRE(expr.has_data());
    }

    SECTION("timbre != 64 is active") {
        MPEExpression expr;
        expr.timbre = 0;
        REQUIRE(expr.has_data());
    }

    SECTION("non-zero pressure is active") {
        MPEExpression expr;
        expr.pressure = 50;
        REQUIRE(expr.has_data());
    }
}

// ─── Test: NoteExpressionEvent ─────────────────────────────────

TEST_CASE("NoteExpressionEvent defaults", "[pianoroll][note][expression]") {
    SECTION("default event has zero fields") {
        NoteExpressionEvent ev;
        REQUIRE(ev.offset_ppqn == 0);
        REQUIRE(ev.type == 0);
        REQUIRE(ev.cc_number == 0);
        REQUIRE(ev.value == 0);
        REQUIRE(ev.interpolation == 0);
    }

    SECTION("can set all fields") {
        NoteExpressionEvent ev;
        ev.offset_ppqn   = 480;
        ev.type          = 1;    // CC
        ev.cc_number     = 74;   // Timbre
        ev.value         = 8192;
        ev.interpolation = 2;    // Bezier
        REQUIRE(ev.offset_ppqn   == 480);
        REQUIRE(ev.type          == 1);
        REQUIRE(ev.cc_number     == 74);
        REQUIRE(ev.value         == 8192);
        REQUIRE(ev.interpolation == 2);
    }
}

// ─── Test: Rgba ────────────────────────────────────────────────

TEST_CASE("Rgba construction", "[pianoroll][note][color]") {
    SECTION("default colour is light grey, fully opaque") {
        Rgba c;
        REQUIRE(c.r == Approx(0.8f));
        REQUIRE(c.g == Approx(0.8f));
        REQUIRE(c.b == Approx(0.8f));
        REQUIRE(c.a == Approx(1.0f));
    }

    SECTION("from_u8 converts 0–255 range to 0–1") {
        auto c = Rgba::from_u8(255, 128, 64, 255);
        REQUIRE(c.r == Approx(1.0f));
        REQUIRE(c.g == Approx(128.0f / 255.0f));
        REQUIRE(c.b == Approx(64.0f / 255.0f));
        REQUIRE(c.a == Approx(1.0f));
    }

    SECTION("from_argb parses ARGB hex") {
        auto c = Rgba::from_argb(0xFFFF7A00);  // Fully opaque orange
        REQUIRE(c.r == Approx(1.0f));
        REQUIRE(c.g == Approx(0x7A / 255.0f));
        REQUIRE(c.b == Approx(0.0f));
        REQUIRE(c.a == Approx(1.0f));
    }

    SECTION("from_argb handles alpha") {
        auto c = Rgba::from_argb(0x804A9EFF);  // 50% transparent blue
        REQUIRE(c.a == Approx(128.0f / 255.0f));
        REQUIRE(c.r == Approx(0x4A / 255.0f));
        REQUIRE(c.g == Approx(0x9E / 255.0f));
        REQUIRE(c.b == Approx(0xFF / 255.0f));
    }
}

// ─── Test: Note defaults ───────────────────────────────────────

TEST_CASE("Note default construction", "[pianoroll][note]") {
    SECTION("default note has zero ID") {
        Note n;
        REQUIRE(n.id.value == 0);
    }

    SECTION("default timing is zero") {
        Note n;
        REQUIRE(n.start_ppqn == 0);
        REQUIRE(n.duration_ppqn == 0);
    }

    SECTION("default key is middle C") {
        Note n;
        REQUIRE(n.key == 60);
    }

    SECTION("default velocity is 100") {
        Note n;
        REQUIRE(n.velocity == 100);
    }

    SECTION("default channel is 0") {
        Note n;
        REQUIRE(n.channel == 0);
    }

    SECTION("default visual properties") {
        Note n;
        REQUIRE(n.opacity == Approx(1.0f));
        REQUIRE_FALSE(n.muted);
        REQUIRE_FALSE(n.locked);
    }

    SECTION("default MPE has no expression") {
        Note n;
        REQUIRE_FALSE(n.mpe.has_data());
    }

    SECTION("default expressions vector is empty") {
        Note n;
        REQUIRE(n.expressions.empty());
    }

    SECTION("default label is empty") {
        Note n;
        REQUIRE(n.label.empty());
    }
}

// ─── Test: Rect ────────────────────────────────────────────────

TEST_CASE("Rect operations", "[pianoroll][rect]") {
    SECTION("default rect is empty at origin") {
        Rect r;
        REQUIRE(r.x == 0.0);
        REQUIRE(r.y == 0.0);
        REQUIRE(r.width == 0.0);
        REQUIRE(r.height == 0.0);
    }

    SECTION("from_ltrb computes width and height") {
        auto r = Rect::from_ltrb(10, 20, 50, 80);
        REQUIRE(r.x == 10.0);
        REQUIRE(r.y == 20.0);
        REQUIRE(r.width == 40.0);
        REQUIRE(r.height == 60.0);
    }

    SECTION("contains point") {
        Rect r{0, 0, 100, 100};
        REQUIRE(r.contains(50, 50));
        REQUIRE(r.contains(0, 0));
        REQUIRE(r.contains(100, 100));
        REQUIRE_FALSE(r.contains(-1, 50));
        REQUIRE_FALSE(r.contains(50, 101));
    }

    SECTION("overlapping rectangles") {
        Rect a{0, 0, 100, 100};
        Rect b{50, 50, 100, 100};
        REQUIRE(a.overlaps(b));
        REQUIRE(b.overlaps(a));
    }

    SECTION("non-overlapping rectangles") {
        Rect a{0, 0, 10, 10};
        Rect b{20, 20, 10, 10};
        REQUIRE_FALSE(a.overlaps(b));
    }
}

// ─── Test: NoteColor ───────────────────────────────────────────

TEST_CASE("NoteColor from_note", "[pianoroll][note][color]") {
    SECTION("from_note uses note colour if set") {
        Note n;
        n.color = Rgba{1.0f, 0.0f, 0.0f, 1.0f};  // explicit red
        auto nc = NoteColor::from_note(n, Rgba{0.0f, 0.0f, 1.0f, 1.0f});
        REQUIRE(nc.body.r == Approx(1.0f));
        REQUIRE(nc.body.g == Approx(0.0f));
    }
}

// ─── NoteColor implementation (defined here for the test) ──────

NoteColor NoteColor::from_note(const Note& note, const Rgba& track_color) {
    NoteColor nc;
    // If the note has a non-default colour, use it; otherwise use
    // the track colour.
    bool is_default = (note.color.r == 0.8f && note.color.g == 0.8f &&
                       note.color.b == 0.8f && note.color.a == 1.0f);
    nc.body = is_default ? track_color : note.color;
    nc.body.a = note.opacity;

    nc.velocity   = nc.body;
    nc.velocity.a = static_cast<float>(note.velocity) / 127.0f;

    nc.outline     = Rgba::from_argb(0xFFFFFFFF);
    nc.shadow      = Rgba{0.0f, 0.0f, 0.0f, 0.3f};
    nc.muted_overlay = Rgba{1.0f, 1.0f, 1.0f, 0.1f};
    nc.opacity     = note.opacity;
    return nc;
}
