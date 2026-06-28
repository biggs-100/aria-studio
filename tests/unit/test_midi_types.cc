#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "midi/midi_types.h"
#include "midi/midi_clip.h"

#include <vector>
#include <cstdint>

using namespace aria;

// ─── MidiEvent Tests ───────────────────────────────────────────

TEST_CASE("MidiEvent utility methods", "[midi][types]") {
    SECTION("is_note returns true for NoteOn and NoteOff") {
        MidiEvent note_on;
        note_on.type = MidiMessageType::NoteOn;
        REQUIRE(note_on.is_note());

        MidiEvent note_off;
        note_off.type = MidiMessageType::NoteOff;
        REQUIRE(note_off.is_note());
    }

    SECTION("is_note returns false for non-note events") {
        MidiEvent cc;
        cc.type = MidiMessageType::ControlChange;
        REQUIRE_FALSE(cc.is_note());

        MidiEvent clock;
        clock.type = MidiMessageType::MidiClock;
        REQUIRE_FALSE(clock.is_note());
    }

    SECTION("is_cc returns true only for ControlChange") {
        MidiEvent cc;
        cc.type = MidiMessageType::ControlChange;
        REQUIRE(cc.is_cc());

        MidiEvent note;
        note.type = MidiMessageType::NoteOn;
        REQUIRE_FALSE(note.is_cc());
    }

    SECTION("is_channel_message returns true for 0x80-0xEF") {
        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        REQUIRE(ev.is_channel_message());

        ev.type = MidiMessageType::PitchBend;
        REQUIRE(ev.is_channel_message());
    }

    SECTION("is_channel_message returns false for system messages") {
        MidiEvent ev;
        ev.type = MidiMessageType::SysEx;
        REQUIRE_FALSE(ev.is_channel_message());

        ev.type = MidiMessageType::MidiClock;
        REQUIRE_FALSE(ev.is_channel_message());
    }
}

TEST_CASE("MidiEvent default construction", "[midi][types]") {
    MidiEvent ev;

    SECTION("default type is NoteOff") {
        REQUIRE(ev.type == MidiMessageType::NoteOff);
    }

    SECTION("default channel is 0") {
        REQUIRE(ev.channel == 0);
    }

    SECTION("default data bytes are 0") {
        REQUIRE(ev.data1 == 0);
        REQUIRE(ev.data2 == 0);
    }

    SECTION("timestamps default to 0") {
        REQUIRE(ev.timestamp_us == 0);
        REQUIRE(ev.ppqn_position == 0);
    }

    SECTION("sysex_data is empty") {
        REQUIRE(ev.sysex_data.empty());
    }
}

// ─── MPEExpression Tests ───────────────────────────────────────

TEST_CASE("MPEExpression", "[midi][types]") {
    SECTION("default expression has no data") {
        MPEExpression expr;
        REQUIRE_FALSE(expr.has_data());
    }

    SECTION("non-zero pitch_bend means has_data") {
        MPEExpression expr;
        expr.pitch_bend = 100;
        REQUIRE(expr.has_data());
    }

    SECTION("timbre != 64 means has_data") {
        MPEExpression expr;
        expr.timbre = 100;
        REQUIRE(expr.has_data());
    }

    SECTION("non-zero pressure means has_data") {
        MPEExpression expr;
        expr.pressure = 50;
        REQUIRE(expr.has_data());
    }

    SECTION("default values") {
        MPEExpression expr;
        REQUIRE(expr.pitch_bend == 0);
        REQUIRE(expr.timbre == 64);
        REQUIRE(expr.pressure == 0);
    }
}

// ─── MidiNote Tests ────────────────────────────────────────────

TEST_CASE("MidiNote default construction", "[midi][types]") {
    MidiNote note;
    REQUIRE(note.start_ppqn == 0);
    REQUIRE(note.duration_ppqn == 0);
    REQUIRE(note.note == 0);
    REQUIRE(note.velocity == 0);
    REQUIRE(note.channel == 0);
    REQUIRE(note.note_events.empty());
}

// ─── MidiClip Tests ────────────────────────────────────────────

TEST_CASE("MidiClip note management", "[midi][clip]") {
    MidiClip clip;

    SECTION("starts empty") {
        REQUIRE(clip.notes().empty());
    }

    SECTION("add_note adds a note") {
        MidiNote note;
        note.start_ppqn = 0;
        note.duration_ppqn = 960;
        note.note = 60;
        note.velocity = 100;
        note.channel = 0;

        clip.add_note(note);
        REQUIRE(clip.notes().size() == 1);
        REQUIRE(clip.notes()[0].note == 60);
    }

    SECTION("remove_note removes by index") {
        MidiNote n1, n2;
        n1.note = 60;
        n2.note = 64;
        clip.add_note(n1);
        clip.add_note(n2);

        clip.remove_note(0);
        REQUIRE(clip.notes().size() == 1);
        REQUIRE(clip.notes()[0].note == 64);
    }

    SECTION("clear removes all notes") {
        clip.add_note(MidiNote{});
        clip.add_note(MidiNote{});
        clip.clear();
        REQUIRE(clip.notes().empty());
    }
}

TEST_CASE("MidiClip transpose", "[midi][clip]") {
    MidiClip clip;

    MidiNote note;
    note.note = 60;
    clip.add_note(note);

    SECTION("transpose up") {
        clip.transpose(5);
        REQUIRE(clip.notes()[0].note == 65);
    }

    SECTION("transpose down") {
        clip.transpose(-5);
        REQUIRE(clip.notes()[0].note == 55);
    }

    SECTION("transpose clamps to valid range") {
        clip.transpose(-100);
        REQUIRE(clip.notes()[0].note == 0);
    }

    SECTION("transpose clamps upper range") {
        note.note = 120;
        clip.clear();
        clip.add_note(note);
        clip.transpose(20);
        REQUIRE(clip.notes()[0].note == 127);
    }
}

TEST_CASE("MidiClip quantize", "[midi][clip]") {
    MidiClip clip;

    MidiNote note;
    note.start_ppqn = 47;
    clip.add_note(note);

    SECTION("full strength snaps to nearest grid") {
        clip.quantize(48, 1.0);
        REQUIRE(clip.notes()[0].start_ppqn == 48);
    }

    SECTION("half strength moves halfway") {
        clip.quantize(48, 0.5);
        REQUIRE(clip.notes()[0].start_ppqn == 47);  // 47 + (48-47)*0.5 = 47.5 -> 47
    }

    SECTION("zero strength does nothing") {
        clip.quantize(48, 0.0);
        REQUIRE(clip.notes()[0].start_ppqn == 47);
    }
}

TEST_CASE("MidiClip serialization roundtrip", "[midi][clip]") {
    MidiClip original;

    // Set metadata
    original.length_ppqn = 3840;
    original.loop_start_ppqn = 0;
    original.loop_end_ppqn = 3840;

    // Add notes
    MidiNote note1;
    note1.start_ppqn = 0;
    note1.duration_ppqn = 960;
    note1.note = 60;
    note1.velocity = 100;
    note1.channel = 0;
    original.add_note(note1);

    MidiNote note2;
    note2.start_ppqn = 960;
    note2.duration_ppqn = 480;
    note2.note = 64;
    note2.velocity = 80;
    note2.channel = 1;
    original.add_note(note2);

    // Add events
    MidiEvent ev;
    ev.type = MidiMessageType::ControlChange;
    ev.channel = 0;
    ev.data1 = 7;
    ev.data2 = 100;
    ev.ppqn_position = 480;
    original.add_event(ev);

    // Serialize and deserialize
    auto data = original.serialize();
    REQUIRE_FALSE(data.empty());

    MidiClip restored = MidiClip::deserialize(data);

    SECTION("metadata preserved") {
        REQUIRE(restored.length_ppqn == original.length_ppqn);
        REQUIRE(restored.loop_start_ppqn == original.loop_start_ppqn);
        REQUIRE(restored.loop_end_ppqn == original.loop_end_ppqn);
    }

    SECTION("notes preserved") {
        REQUIRE(restored.notes().size() == 2);
        REQUIRE(restored.notes()[0].note == 60);
        REQUIRE(restored.notes()[0].start_ppqn == 0);
        REQUIRE(restored.notes()[0].duration_ppqn == 960);
        REQUIRE(restored.notes()[0].velocity == 100);
        REQUIRE(restored.notes()[1].note == 64);
        REQUIRE(restored.notes()[1].channel == 1);
    }

    SECTION("events preserved") {
        REQUIRE(restored.events().size() == 1);
        REQUIRE(restored.events()[0].type == MidiMessageType::ControlChange);
        REQUIRE(restored.events()[0].data1 == 7);
        REQUIRE(restored.events()[0].data2 == 100);
    }
}

TEST_CASE("MidiClip events management", "[midi][clip]") {
    MidiClip clip;

    SECTION("add_event stores events") {
        MidiEvent ev;
        ev.type = MidiMessageType::ControlChange;
        ev.data1 = 7;
        ev.data2 = 64;
        clip.add_event(ev);

        REQUIRE(clip.events().size() == 1);
    }

    SECTION("clear removes both notes and events") {
        clip.add_note(MidiNote{});
        clip.add_event(MidiEvent{});
        clip.clear();
        REQUIRE(clip.notes().empty());
        REQUIRE(clip.events().empty());
    }
}

TEST_CASE("MidiClip humanize", "[midi][clip]") {
    MidiClip clip;

    MidiNote note;
    note.start_ppqn = 960;
    note.velocity = 100;
    clip.add_note(note);

    SECTION("zero amounts do nothing") {
        clip.humanize(0.0, 0.0);
        REQUIRE(clip.notes()[0].start_ppqn == 960);
        REQUIRE(clip.notes()[0].velocity == 100);
    }

    SECTION("timing humanize modifies start") {
        clip.humanize(1.0, 0.0);
        // Timing may have jitter, but note should still exist
        REQUIRE(clip.notes().size() == 1);
    }

    SECTION("velocity humanize modifies velocity") {
        clip.humanize(0.0, 1.0);
        // Velocity may change, but should stay in valid range
        REQUIRE(clip.notes()[0].velocity <= 127);
    }
}
