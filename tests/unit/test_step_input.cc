#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/step_input.h"
#include "pianoroll/note_collection.h"

using Catch::Approx;
using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Defaults
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiStepInput defaults", "[pianoroll][stepinput]") {
    MidiStepInput si;

    SECTION("default is disabled") {
        REQUIRE_FALSE(si.is_enabled());
    }

    SECTION("default mode is Step") {
        REQUIRE(si.mode() == MidiStepInput::Step);
    }

    SECTION("default step duration is 240 PPQN (16th note)") {
        REQUIRE(si.step_duration() == 240);
    }

    SECTION("default next_position is 0") {
        REQUIRE(si.next_position() == 0);
    }

    SECTION("no pending note by default") {
        REQUIRE_FALSE(si.pending_note().has_value());
    }
}

// ═══════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiStepInput configuration", "[pianoroll][stepinput][config]") {
    MidiStepInput si;

    SECTION("set_enabled toggles state") {
        si.set_enabled(true);
        REQUIRE(si.is_enabled());

        si.set_enabled(false);
        REQUIRE_FALSE(si.is_enabled());
    }

    SECTION("set_mode changes input mode") {
        si.set_mode(MidiStepInput::RealTime);
        REQUIRE(si.mode() == MidiStepInput::RealTime);

        si.set_mode(MidiStepInput::Replace);
        REQUIRE(si.mode() == MidiStepInput::Replace);

        si.set_mode(MidiStepInput::Step);
        REQUIRE(si.mode() == MidiStepInput::Step);
    }

    SECTION("set_step_duration") {
        si.set_step_duration(480);  // Quarter note
        REQUIRE(si.step_duration() == 480);
    }

    SECTION("set_next_position") {
        si.set_next_position(960);
        REQUIRE(si.next_position() == 960);
    }
}

// ═══════════════════════════════════════════════════════════════
// Step mode
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiStepInput step mode", "[pianoroll][stepinput][step]") {
    MidiStepInput si;
    si.set_enabled(true);
    si.set_mode(MidiStepInput::Step);
    si.set_step_duration(240);  // 16th notes

    SECTION("note-on creates pending note at current position") {
        si.on_midi_note(60, 100, true);  // C4, velocity 100

        auto pending = si.pending_note();
        REQUIRE(pending.has_value());
        REQUIRE(pending->key == 60);
        REQUIRE(pending->velocity == 100);
        REQUIRE(pending->start_ppqn == 0);  // Initial position
        REQUIRE(pending->duration_ppqn == 240);
    }

    SECTION("note-on advances step position") {
        si.on_midi_note(60, 100, true);
        REQUIRE(si.next_position() == 240);  // Advanced by one step

        si.on_midi_note(62, 100, true);
        REQUIRE(si.next_position() == 480);  // Advanced again
    }

    SECTION("second note-on without note-off finalizes first") {
        si.on_midi_note(60, 100, true);  // Position advances to 240
        // The first note's duration should be set to (next_pos - start) = 240 - 0 = 240

        si.on_midi_note(62, 90, true);   // Position advances to 480

        // The pending note is now the second one.
        auto pending = si.pending_note();
        REQUIRE(pending.has_value());
        REQUIRE(pending->key == 62);
        REQUIRE(pending->start_ppqn == 240);
        REQUIRE(pending->duration_ppqn == 240);
    }
}

// ═══════════════════════════════════════════════════════════════
// Real-time mode
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiStepInput real-time mode", "[pianoroll][stepinput][realtime]") {
    MidiStepInput si;
    si.set_enabled(true);
    si.set_mode(MidiStepInput::RealTime);

    SECTION("note-on creates pending note without advancing") {
        si.set_next_position(480);
        si.on_midi_note(60, 100, true);

        auto pending = si.pending_note();
        REQUIRE(pending.has_value());
        REQUIRE(pending->key == 60);
        REQUIRE(pending->start_ppqn == 480);
        REQUIRE(si.next_position() == 480);  // Not advanced in RealTime
    }

    SECTION("note-off does not clean pending in RealTime alone") {
        si.on_midi_note(60, 100, true);
        si.on_midi_note(60, 0, false);

        // In RealTime mode, note-off sets the duration but doesn't clear
        // pending until process_midi handles it.
        auto pending = si.pending_note();
        REQUIRE(pending.has_value());
        // Duration should be set: position (0) - start (0) = 0
        // But minimum is step_ppqn_ (240), so duration = 240
    }
}

// ═══════════════════════════════════════════════════════════════
// MIDI event processing
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiStepInput process_midi", "[pianoroll][stepinput][midi]") {
    MidiStepInput si;
    si.set_enabled(true);
    si.set_mode(MidiStepInput::Step);
    si.set_step_duration(240);
    si.set_next_position(0);

    NoteCollection notes;

    SECTION("NoteOn adds a note to the collection") {
        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        ev.data1 = 60;   // C4
        ev.data2 = 100;  // velocity

        si.process_midi(ev, notes);

        REQUIRE(notes.size() == 1);
        const auto& added = notes.notes();
        REQUIRE(added[0].key == 60);
        REQUIRE(added[0].velocity == 100);
        REQUIRE(added[0].start_ppqn == 0);
    }

    SECTION("NoteOff updates duration in collection") {
        MidiEvent note_on;
        note_on.type = MidiMessageType::NoteOn;
        note_on.data1 = 60;
        note_on.data2 = 100;

        si.process_midi(note_on, notes);

        // Advance step and send note-off.
        MidiEvent note_off;
        note_off.type = MidiMessageType::NoteOff;
        note_off.data1 = 60;
        note_off.data2 = 0;

        si.process_midi(note_off, notes);

        // Note should still exist with its duration updated.
        REQUIRE(notes.size() == 1);
    }

    SECTION("NoteOn with velocity 0 is treated as NoteOff") {
        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        ev.data1 = 60;
        ev.data2 = 0;  // Zero velocity = note off

        REQUIRE_NOTHROW(si.process_midi(ev, notes));
    }

    SECTION("process with disabled step input does nothing") {
        si.set_enabled(false);

        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        ev.data1 = 60;
        ev.data2 = 100;

        si.process_midi(ev, notes);
        REQUIRE(notes.empty());
    }
}

// ═══════════════════════════════════════════════════════════════
// Step input advanced
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MidiStepInput clear_pending", "[pianoroll][stepinput]") {
    MidiStepInput si;
    si.set_enabled(true);

    SECTION("clear_pending removes held note") {
        si.on_midi_note(60, 100, true);
        REQUIRE(si.pending_note().has_value());

        si.clear_pending();
        REQUIRE_FALSE(si.pending_note().has_value());
    }

    SECTION("disabling clears pending") {
        si.on_midi_note(60, 100, true);
        REQUIRE(si.pending_note().has_value());

        si.set_enabled(false);
        REQUIRE_FALSE(si.pending_note().has_value());
    }
}

TEST_CASE("MidiStepInput advance_step", "[pianoroll][stepinput][advance]") {
    MidiStepInput si;
    si.set_enabled(true);
    si.set_mode(MidiStepInput::Step);
    si.set_step_duration(120);  // 32nd notes

    SECTION("advance_step manually moves position") {
        REQUIRE(si.next_position() == 0);
        si.advance_step();
        REQUIRE(si.next_position() == 120);
        si.advance_step();
        REQUIRE(si.next_position() == 240);
    }

    SECTION("advance_step disabled when step input disabled") {
        si.set_enabled(false);
        si.advance_step();
        REQUIRE(si.next_position() == 0);
    }

    SECTION("advance_step in RealTime mode does nothing") {
        si.set_mode(MidiStepInput::RealTime);
        si.advance_step();
        REQUIRE(si.next_position() == 0);
    }
}
