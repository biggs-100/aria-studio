#include <catch2/catch_test_macros.hpp>
#include "midi/note_tracker.h"

using namespace aria;

TEST_CASE("NoteTracker basic note on/off", "[midi][notetracker]") {
    NoteTracker tracker;

    SECTION("starts with no active notes") {
        REQUIRE(tracker.active_note_count() == 0);
    }

    SECTION("note_on makes a note active") {
        tracker.note_on(60, 0, 100, 0);
        REQUIRE(tracker.active_note_count() == 1);
        REQUIRE(tracker.is_note_active(60, 0));
    }

    SECTION("note_off removes a note") {
        tracker.note_on(60, 0, 100, 0);
        tracker.note_off(60, 0, 1000);
        REQUIRE(tracker.active_note_count() == 0);
        REQUIRE_FALSE(tracker.is_note_active(60, 0));
    }

    SECTION("multiple notes on different channels") {
        tracker.note_on(60, 0, 100, 0);
        tracker.note_on(60, 1, 100, 0);
        REQUIRE(tracker.active_note_count() == 2);
    }

    SECTION("same note on different channels are independent") {
        tracker.note_on(60, 0, 100, 0);
        tracker.note_off(60, 0, 1000);
        REQUIRE(tracker.active_note_count() == 0);

        tracker.note_on(60, 1, 100, 0);
        REQUIRE(tracker.active_note_count() == 1);
        REQUIRE(tracker.is_note_active(60, 1));
    }
}

TEST_CASE("NoteTracker sustain pedal", "[midi][notetracker]") {
    NoteTracker tracker;

    SECTION("sustain holds notes after note-off") {
        tracker.note_on(60, 0, 100, 0);
        tracker.sustain(true);
        tracker.note_off(60, 0, 1000);

        // Note should still be active while sustain is held
        REQUIRE(tracker.active_note_count() == 0);  // marked inactive in map
    }

    SECTION("releasing sustain removes sustained notes") {
        tracker.note_on(60, 0, 100, 0);
        tracker.sustain(true);
        tracker.note_off(60, 0, 1000);

        // Release sustain
        tracker.sustain(false);

        // Note should be removed
        REQUIRE_FALSE(tracker.is_note_active(60, 0));
    }

    SECTION("sustain without notes does nothing") {
        tracker.sustain(true);
        tracker.sustain(false);
        REQUIRE(tracker.active_note_count() == 0);
    }

    SECTION("notes added while sustain active work normally") {
        tracker.sustain(true);
        tracker.note_on(60, 0, 100, 0);
        REQUIRE(tracker.is_note_active(60, 0));

        tracker.sustain(false);
        REQUIRE(tracker.is_note_active(60, 0));  // note is still held

        tracker.note_off(60, 0, 2000);
        REQUIRE_FALSE(tracker.is_note_active(60, 0));
    }
}

TEST_CASE("NoteTracker all_notes_off", "[midi][notetracker]") {
    NoteTracker tracker;

    tracker.note_on(60, 0, 100, 0);
    tracker.note_on(64, 0, 100, 100);
    tracker.note_on(67, 1, 100, 200);

    REQUIRE(tracker.active_note_count() == 3);

    SECTION("all_notes_off clears everything") {
        tracker.all_notes_off();
        REQUIRE(tracker.active_note_count() == 0);
        REQUIRE_FALSE(tracker.is_note_active(60, 0));
        REQUIRE_FALSE(tracker.is_note_active(64, 0));
        REQUIRE_FALSE(tracker.is_note_active(67, 1));
    }

    SECTION("all_notes_off works with sustain active") {
        tracker.sustain(true);
        tracker.note_off(60, 0, 300);

        tracker.all_notes_off();
        REQUIRE(tracker.active_note_count() == 0);
    }
}

TEST_CASE("NoteTracker active_note access", "[midi][notetracker]") {
    NoteTracker tracker;

    SECTION("active_note returns nullptr for inactive note") {
        REQUIRE(tracker.active_note(60, 0) == nullptr);
    }

    SECTION("active_note returns valid pointer for active note") {
        tracker.note_on(60, 0, 100, 500);
        const NoteState* state = tracker.active_note(60, 0);
        REQUIRE(state != nullptr);
        REQUIRE(state->note == 60);
        REQUIRE(state->channel == 0);
        REQUIRE(state->velocity == 100);
        REQUIRE(state->start_sample == 500);
        REQUIRE(state->active);
    }

    SECTION("active_notes list matches count") {
        tracker.note_on(60, 0, 100, 0);
        tracker.note_on(64, 1, 80, 100);

        REQUIRE(tracker.active_notes().size() == 2);
        REQUIRE(tracker.active_note_count() == 2);
    }
}

TEST_CASE("NoteTracker edge cases", "[midi][notetracker]") {
    NoteTracker tracker;

    SECTION("double note-on overwrites") {
        tracker.note_on(60, 0, 100, 0);
        tracker.note_on(60, 0, 50, 100);

        const NoteState* state = tracker.active_note(60, 0);
        REQUIRE(state != nullptr);
        REQUIRE(state->velocity == 50);  // last write wins
        REQUIRE(state->start_sample == 100);  // last write wins
    }

    SECTION("note-off without note-on is a no-op") {
        tracker.note_off(60, 0, 1000);
        REQUIRE(tracker.active_note_count() == 0);
    }

    SECTION("note-off twice is a no-op") {
        tracker.note_on(60, 0, 100, 0);
        tracker.note_off(60, 0, 1000);
        tracker.note_off(60, 0, 2000);  // second off should do nothing
        REQUIRE(tracker.active_note_count() == 0);
    }
}
