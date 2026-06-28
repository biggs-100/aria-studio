#include <catch2/catch_test_macros.hpp>
#include "pianoroll/spatial_grid.h"

#include <vector>

using namespace aria;

// ─── Helpers ───────────────────────────────────────────────────

static Note make_note(uint64_t start, uint64_t dur, uint8_t key) {
    Note n;
    n.start_ppqn    = start;
    n.duration_ppqn = dur;
    n.key           = key;
    return n;
}

// ─── Test: rebuild and query ───────────────────────────────────

TEST_CASE("SpatialGrid rebuild and query", "[pianoroll][spatial]") {
    SpatialGrid grid;

    std::vector<Note> notes;
    notes.push_back(make_note(0, 960, 60));    // cell (0, 5)
    notes.push_back(make_note(960, 480, 64));  // cell (0, 5) — same col
    notes.push_back(make_note(3840, 960, 72)); // cell (1, 6) — next bar, next octave

    SECTION("rebuild creates cells") {
        grid.rebuild(notes);
        REQUIRE(grid.cell_count() > 0);
    }

    SECTION("query returns notes in a cell") {
        grid.rebuild(notes);
        Rect r1{0.0, 60.0, 3840.0, 12.0};  // cell (0, 5)
        auto results = grid.query(r1);
        REQUIRE(results.size() == 2);  // notes at key 60 and 64
    }

    SECTION("query returns notes in another cell") {
        grid.rebuild(notes);
        Rect r2{3840.0, 72.0, 3840.0, 12.0};  // cell (1, 6)
        auto results = grid.query(r2);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0]->key == 72);
    }

    SECTION("empty grid query returns nothing") {
        grid.rebuild(notes);
        Rect r{99999, 0, 100, 127};
        auto results = grid.query(r);
        REQUIRE(results.empty());
    }

    SECTION("clear removes all cells") {
        grid.rebuild(notes);
        REQUIRE(grid.cell_count() > 0);
        grid.clear();
        REQUIRE(grid.cell_count() == 0);
    }
}

// ─── Test: culling (notes outside viewport are excluded) ───────

TEST_CASE("SpatialGrid culling", "[pianoroll][spatial][culling]") {
    SpatialGrid grid;

    std::vector<Note> notes;
    // Two bars worth of notes.
    for (int i = 0; i < 16; ++i) {
        uint64_t ppqn = static_cast<uint64_t>(i) * 480;
        notes.push_back(make_note(ppqn, 240, static_cast<uint8_t>(60 + (i % 12))));
    }

    grid.rebuild(notes);

    SECTION("query over first bar returns only notes in that bar") {
        Rect first_bar{0.0, 0.0, 3840.0, 127.0};
        auto results = grid.query(first_bar);
        // First 8 notes (0, 480, 960, ..., 3360) fall in bar 0
        REQUIRE(results.size() == 8);
    }

    SECTION("query over second bar returns next set") {
        Rect second_bar{3840.0, 0.0, 3840.0, 127.0};
        auto results = grid.query(second_bar);
        REQUIRE(results.size() == 8);
    }

    SECTION("query over narrow key range returns subset") {
        Rect narrow{0.0, 60.0, 3840.0, 1.0};  // only key 60
        auto results = grid.query(narrow);
        for (auto* n : results) {
            REQUIRE(n->key == 60);
        }
    }
}

// ─── Test: long notes spanning multiple cells ──────────────────

TEST_CASE("SpatialGrid long notes span multiple cells", "[pianoroll][spatial][long]") {
    SpatialGrid grid;

    std::vector<Note> notes;
    // A note spanning 4 bars.
    notes.push_back(make_note(0, 3840 * 4, 60));  // spans bars 0-3

    grid.rebuild(notes);

    SECTION("each spanned cell contains the note") {
        auto results = grid.query(Rect{0.0, 60.0, 3840.0, 1.0});
        REQUIRE(results.size() == 1);

        results = grid.query(Rect{3840.0, 60.0, 3840.0, 1.0});
        REQUIRE(results.size() == 1);

        results = grid.query(Rect{7680.0, 60.0, 3840.0, 1.0});
        REQUIRE(results.size() == 1);

        results = grid.query(Rect{11520.0, 60.0, 3840.0, 1.0});
        REQUIRE(results.size() == 1);
    }

    SECTION("cell_count reflects multiple cells") {
        REQUIRE(grid.cell_count() == 4);  // one per bar spanned
    }
}

// ─── Test: empty collection ────────────────────────────────────

TEST_CASE("SpatialGrid empty collection", "[pianoroll][spatial][empty]") {
    SpatialGrid grid;
    std::vector<Note> empty;

    SECTION("rebuild with empty notes produces no cells") {
        grid.rebuild(empty);
        REQUIRE(grid.cell_count() == 0);
    }

    SECTION("query on empty grid returns empty") {
        grid.rebuild(empty);
        auto results = grid.query(Rect{0, 0, 3840, 127});
        REQUIRE(results.empty());
    }
}

// ─── Test: note at cell boundaries ────────────────────────────

TEST_CASE("SpatialGrid cell boundary notes", "[pianoroll][spatial][boundary]") {
    SpatialGrid grid;
    std::vector<Note> notes;

    // Note starting exactly at a cell boundary.
    notes.push_back(make_note(3840, 480, 60));  // starts at bar 1

    grid.rebuild(notes);

    SECTION("query for previous cell does not include it") {
        auto results = grid.query(Rect{0.0, 60.0, 3840.0, 1.0});
        // The note at 3840 starts at the right edge of bar 0.
        // If the query is [0, 3840) (exclusive right), the note should NOT be included.
        // Our query uses inclusive boundaries.
        REQUIRE(results.empty());
    }

    SECTION("query for containing cell includes it") {
        auto results = grid.query(Rect{3840.0, 60.0, 3840.0, 1.0});
        REQUIRE(results.size() == 1);
    }
}
