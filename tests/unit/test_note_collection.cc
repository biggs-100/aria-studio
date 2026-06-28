#include <catch2/catch_test_macros.hpp>
#include "pianoroll/note_collection.h"

using namespace aria;

// ─── Helper ─────────────────────────────────────────────────────

static Note make_note(uint64_t start, uint64_t dur, uint8_t key,
                       uint8_t vel = 100, uint8_t ch = 0)
{
    Note n;
    n.start_ppqn    = start;
    n.duration_ppqn = dur;
    n.key           = key;
    n.velocity      = vel;
    n.channel       = ch;
    return n;
}

// ─── Test: CRUD ────────────────────────────────────────────────

TEST_CASE("NoteCollection CRUD", "[pianoroll][collection][crud]") {
    NoteCollection col;

    SECTION("starts empty") {
        REQUIRE(col.size() == 0);
        REQUIRE(col.empty());
    }

    SECTION("add returns a valid ID") {
        auto id = col.add(make_note(0, 960, 60));
        REQUIRE(id.value != 0);
        REQUIRE(col.size() == 1);
    }

    SECTION("find returns the added note") {
        auto id = col.add(make_note(0, 960, 60));
        Note* n = col.find(id);
        REQUIRE(n != nullptr);
        REQUIRE(n->key == 60);
        REQUIRE(n->start_ppqn == 0);
        REQUIRE(n->duration_ppqn == 960);
    }

    SECTION("find returns nullptr for unknown ID") {
        REQUIRE(col.find(NoteID{999}) == nullptr);
    }

    SECTION("remove deletes a note") {
        auto id = col.add(make_note(0, 960, 60));
        REQUIRE(col.size() == 1);
        col.remove(id);
        REQUIRE(col.empty());
        REQUIRE(col.find(id) == nullptr);
    }

    SECTION("remove on unknown ID is a no-op") {
        col.remove(NoteID{999});
        REQUIRE(col.empty());
    }

    SECTION("update replaces note data") {
        auto id = col.add(make_note(0, 960, 60));
        Note updated = make_note(480, 480, 64, 127);
        updated.id = id;
        col.update(id, updated);

        Note* n = col.find(id);
        REQUIRE(n->start_ppqn == 480);
        REQUIRE(n->duration_ppqn == 480);
        REQUIRE(n->key == 64);
        REQUIRE(n->velocity == 127);
    }

    SECTION("update on unknown ID is a no-op") {
        col.update(NoteID{999}, make_note(0, 0, 0));
        REQUIRE(col.empty());
    }

    SECTION("clear removes everything") {
        col.add(make_note(0, 960, 60));
        col.add(make_note(960, 480, 64));
        REQUIRE(col.size() == 2);
        col.clear();
        REQUIRE(col.empty());
    }

    SECTION("reserve pre-allocates memory") {
        col.reserve(100);
        REQUIRE(col.empty());
        for (int i = 0; i < 100; ++i) {
            col.add(make_note(static_cast<uint64_t>(i) * 960, 960, 60));
        }
        REQUIRE(col.size() == 100);
    }
}

// ─── Test: find_in_range ───────────────────────────────────────

TEST_CASE("NoteCollection find_in_range", "[pianoroll][collection][query]") {
    NoteCollection col;

    col.add(make_note(0,    960, 60));  // note 0
    col.add(make_note(960,  480, 64));  // note 1
    col.add(make_note(1920, 240, 72));  // note 2
    col.add(make_note(0,    3840, 48)); // note 3 (long note)

    SECTION("returns notes within time range") {
        auto results = col.find_in_range(0, 1000, 0, 127);
        // note 0 (0-960, key 60), note 1 (960-1440, key 64), note 3 (0-3840, key 48)
        REQUIRE(results.size() == 3);
    }

    SECTION("returns notes within key range") {
        auto results = col.find_in_range(0, 4000, 60, 70);
        REQUIRE(results.size() == 2);  // note 0 (60) and note 1 (64)
        // note 2 (72) is above range, note 3 (48) is below
    }

    SECTION("returns notes within both time and key range") {
        auto results = col.find_in_range(0, 1000, 60, 65);
        // note 0 (0-960, key 60) AND note 1 (960-1440, key 64)
        // note 3 (key 48) is filtered by key range
        REQUIRE(results.size() == 2);
        REQUIRE(results[0]->key == 60);
        REQUIRE(results[1]->key == 64);
    }

    SECTION("empty range returns no results") {
        auto results = col.find_in_range(5000, 6000, 0, 127);
        REQUIRE(results.empty());
    }
}

// ─── Test: find_by_key ─────────────────────────────────────────

TEST_CASE("NoteCollection find_by_key", "[pianoroll][collection][query]") {
    NoteCollection col;

    col.add(make_note(0, 960, 60));
    col.add(make_note(960, 480, 64));
    col.add(make_note(1920, 240, 60));

    SECTION("returns all notes at given key") {
        auto results = col.find_by_key(60);
        REQUIRE(results.size() == 2);
    }

    SECTION("returns empty for key with no notes") {
        auto results = col.find_by_key(72);
        REQUIRE(results.empty());
    }
}

// ─── Test: Selection ───────────────────────────────────────────

TEST_CASE("NoteCollection selection", "[pianoroll][collection][selection]") {
    NoteCollection col;
    auto id1 = col.add(make_note(0, 960, 60));
    auto id2 = col.add(make_note(960, 480, 64));
    auto id3 = col.add(make_note(1920, 240, 72));

    SECTION("starts with empty selection") {
        REQUIRE(col.selected().empty());
    }

    SECTION("select adds to selection") {
        col.select(id1);
        REQUIRE(col.selected().size() == 1);
        REQUIRE(col.selected().count(id1) == 1);
    }

    SECTION("deselect removes from selection") {
        col.select(id1);
        col.deselect(id1);
        REQUIRE(col.selected().empty());
    }

    SECTION("select_all selects every note") {
        col.select_all();
        REQUIRE(col.selected().size() == 3);
    }

    SECTION("clear_selection empties selection") {
        col.select_all();
        col.clear_selection();
        REQUIRE(col.selected().empty());
    }

    SECTION("toggle_selection toggles state") {
        col.toggle_selection(id1);
        REQUIRE(col.selected().count(id1) == 1);
        col.toggle_selection(id1);
        REQUIRE(col.selected().empty());
    }

    SECTION("remove clears the note from selection") {
        col.select_all();
        col.remove(id1);
        REQUIRE(col.selected().size() == 2);
        REQUIRE(col.selected().count(id1) == 0);
    }
}

// ─── Test: Transpose ───────────────────────────────────────────

TEST_CASE("NoteCollection transpose", "[pianoroll][collection][transpose]") {
    NoteCollection col;
    auto id1 = col.add(make_note(0, 960, 60));
    auto id2 = col.add(make_note(960, 480, 64));

    SECTION("transpose all notes up") {
        col.transpose(5);
        REQUIRE(col.find(id1)->key == 65);
        REQUIRE(col.find(id2)->key == 69);
    }

    SECTION("transpose all notes down") {
        col.transpose(-5);
        REQUIRE(col.find(id1)->key == 55);
        REQUIRE(col.find(id2)->key == 59);
    }

    SECTION("transpose affects only selected notes") {
        col.select(id1);
        col.transpose(12);
        REQUIRE(col.find(id1)->key == 72);
        REQUIRE(col.find(id2)->key == 64);  // unchanged
    }

    SECTION("transpose clamps to valid range") {
        col.add(make_note(1920, 240, 0));   // lowest note
        col.add(make_note(2880, 240, 127)); // highest note
        col.transpose(-10);
        REQUIRE(col.find(id1)->key == 50);   // 60-10
        // Note at key 0 stays at 0 (already at bottom)
        // Note at key 127 stays at 127
    }

    SECTION("zero transpose is a no-op") {
        col.transpose(0);
        REQUIRE(col.find(id1)->key == 60);
    }
}

// ─── Test: Shift horizontal ────────────────────────────────────

TEST_CASE("NoteCollection shift_horizontal", "[pianoroll][collection][shift]") {
    NoteCollection col;
    auto id1 = col.add(make_note(0, 960, 60));
    auto id2 = col.add(make_note(960, 480, 64));

    SECTION("shifts all notes forward") {
        col.shift_horizontal(480);
        REQUIRE(col.find(id1)->start_ppqn == 480);
        REQUIRE(col.find(id2)->start_ppqn == 1440);
    }

    SECTION("shifts all notes backward") {
        col.shift_horizontal(-240);
        REQUIRE(col.find(id1)->start_ppqn == 0);   // clamped at 0
        REQUIRE(col.find(id2)->start_ppqn == 720);
    }

    SECTION("shift affects only selected notes") {
        col.select(id1);
        col.shift_horizontal(480);
        REQUIRE(col.find(id1)->start_ppqn == 480);
        REQUIRE(col.find(id2)->start_ppqn == 960);  // unchanged
    }

    SECTION("zero shift is a no-op") {
        col.shift_horizontal(0);
        REQUIRE(col.find(id1)->start_ppqn == 0);
    }

    SECTION("shift clamps to zero") {
        col.shift_horizontal(-100);
        REQUIRE(col.find(id1)->start_ppqn == 0);  // would be -100, clamped to 0
    }
}

// ─── Test: Multiply velocity ───────────────────────────────────

TEST_CASE("NoteCollection multiply_velocity", "[pianoroll][collection][velocity]") {
    NoteCollection col;
    auto id1 = col.add(make_note(0, 960, 60, 100));
    auto id2 = col.add(make_note(960, 480, 64, 80));

    SECTION("doubles velocity") {
        col.multiply_velocity(2.0);
        REQUIRE(col.find(id1)->velocity == 127);  // clamped
        REQUIRE(col.find(id2)->velocity == 127);  // 160 → 127
    }

    SECTION("halves velocity") {
        col.multiply_velocity(0.5);
        REQUIRE(col.find(id1)->velocity == 50);
        REQUIRE(col.find(id2)->velocity == 40);
    }

    SECTION("factor 1.0 leaves velocity unchanged") {
        col.multiply_velocity(1.0);
        REQUIRE(col.find(id1)->velocity == 100);
        REQUIRE(col.find(id2)->velocity == 80);
    }

    SECTION("affects only selected notes") {
        col.select(id1);
        col.multiply_velocity(0.5);
        REQUIRE(col.find(id1)->velocity == 50);
        REQUIRE(col.find(id2)->velocity == 80);  // unchanged
    }

    SECTION("factor is clamped to [0, 2]") {
        col.multiply_velocity(5.0);
        REQUIRE(col.find(id1)->velocity == 127);  // 100*2=200→127
        REQUIRE(col.find(id2)->velocity == 127);  // 80*2=160→127
    }
}

// ─── Test: sort_by_start ───────────────────────────────────────

TEST_CASE("NoteCollection sort_by_start", "[pianoroll][collection][sort]") {
    NoteCollection col;
    auto id1 = col.add(make_note(1920, 240, 72));
    auto id2 = col.add(make_note(0, 960, 60));
    auto id3 = col.add(make_note(960, 480, 64));

    SECTION("sorts notes by start time") {
        col.sort_by_start();
        const auto& notes = col.notes();
        REQUIRE(notes[0].start_ppqn == 0);
        REQUIRE(notes[0].key == 60);
        REQUIRE(notes[1].start_ppqn == 960);
        REQUIRE(notes[1].key == 64);
        REQUIRE(notes[2].start_ppqn == 1920);
        REQUIRE(notes[2].key == 72);
    }

    SECTION("find still works after sort") {
        col.sort_by_start();
        REQUIRE(col.find(id2) != nullptr);
        REQUIRE(col.find(id2)->key == 60);
    }

    SECTION("tie-breaking by key") {
        NoteCollection col2;
        col2.add(make_note(0, 480, 72));
        col2.add(make_note(0, 480, 60));
        col2.sort_by_start();
        REQUIRE(col2.notes()[0].key == 60);
        REQUIRE(col2.notes()[1].key == 72);
    }
}

// ─── Test: ID uniqueness ───────────────────────────────────────

TEST_CASE("NoteCollection ID uniqueness", "[pianoroll][collection][id]") {
    NoteCollection col;

    SECTION("multiple adds produce unique IDs") {
        auto id1 = col.add(make_note(0, 960, 60));
        auto id2 = col.add(make_note(960, 480, 64));
        REQUIRE(id1.value != id2.value);
    }

    SECTION("removed IDs are not reused") {
        auto id1 = col.add(make_note(0, 960, 60));
        auto id2 = col.add(make_note(960, 480, 64));
        col.remove(id1);
        auto id3 = col.add(make_note(1920, 240, 72));
        REQUIRE(id3.value != id2.value);
    }
}
