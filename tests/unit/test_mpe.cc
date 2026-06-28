#include <catch2/catch_test_macros.hpp>
#include "midi/mpe.h"

using namespace aria;

// ═══════════════════════════════════════════════════════════════
// MPEDecoder Tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MPEDecoder initial state", "[midi][mpe]") {
    MPEDecoder decoder;

    SECTION("starts with no MPE configuration") {
        REQUIRE_FALSE(decoder.has_mpe_configuration());
    }

    SECTION("initial member channel count is 0") {
        REQUIRE(decoder.member_channel_count() == 0);
    }

    SECTION("get_note_expression returns default for unset note") {
        MPEExpression expr = decoder.get_note_expression(60);
        REQUIRE_FALSE(expr.has_data());
        REQUIRE(expr.pitch_bend == 0);
        REQUIRE(expr.timbre == 64);
        REQUIRE(expr.pressure == 0);
    }

    SECTION("get_channel_expression returns default for unset channel") {
        MPEExpression expr = decoder.get_channel_expression(2);
        REQUIRE_FALSE(expr.has_data());
    }

    SECTION("reset does not crash") {
        decoder.reset();
        REQUIRE_FALSE(decoder.has_mpe_configuration());
    }
}

TEST_CASE("MPEDecoder event classification", "[midi][mpe]") {
    MPEDecoder decoder;

    SECTION("master channel pitch bend") {
        MidiEvent ev;
        ev.type = MidiMessageType::PitchBend;
        ev.channel = MPEConstants::kMasterChannel;
        REQUIRE(decoder.classify(ev) == MPEMessageType::MasterPitchBend);
    }

    SECTION("master channel CC") {
        MidiEvent ev;
        ev.type = MidiMessageType::ControlChange;
        ev.channel = MPEConstants::kMasterChannel;
        REQUIRE(decoder.classify(ev) == MPEMessageType::MasterControlChange);
    }

    SECTION("member channel note-on") {
        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        ev.channel = 2;
        REQUIRE(decoder.classify(ev) == MPEMessageType::ZoneNoteOn);
    }

    SECTION("member channel note-off") {
        MidiEvent ev;
        ev.type = MidiMessageType::NoteOff;
        ev.channel = 2;
        REQUIRE(decoder.classify(ev) == MPEMessageType::ZoneNoteOff);
    }

    SECTION("member channel pitch bend") {
        MidiEvent ev;
        ev.type = MidiMessageType::PitchBend;
        ev.channel = 3;
        REQUIRE(decoder.classify(ev) == MPEMessageType::ZonePitchBend);
    }

    SECTION("member channel CC 74 (timbre)") {
        MidiEvent ev;
        ev.type = MidiMessageType::ControlChange;
        ev.channel = 3;
        ev.data1 = MPEConstants::kCC_Timbre;
        REQUIRE(decoder.classify(ev) == MPEMessageType::ZoneTimbre);
    }

    SECTION("member channel channel aftertouch") {
        MidiEvent ev;
        ev.type = MidiMessageType::ChannelAftertouch;
        ev.channel = 3;
        REQUIRE(decoder.classify(ev) == MPEMessageType::ZonePressure);
    }

    SECTION("non-MPE channel 15") {
        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        ev.channel = 15; // Outside MPE zone
        // Still classified as zone note-on since it's within channel range
        // but not in the typical MPE zone
        REQUIRE(decoder.classify(ev) == MPEMessageType::ZoneNoteOn);
    }

    SECTION("non-CC74 on member channel") {
        MidiEvent ev;
        ev.type = MidiMessageType::ControlChange;
        ev.channel = 3;
        ev.data1 = 7; // Volume, not timbre
        REQUIRE(decoder.classify(ev) == MPEMessageType::NonMPE);
    }
}

TEST_CASE("MPEDecoder per-note expression", "[midi][mpe]") {
    MPEDecoder decoder;

    SECTION("pitch bend updates note expression") {
        // Simulate a note-on on channel 2, then pitch bend
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 2;
        on.data1 = 60;
        on.data2 = 100;
        decoder.process(on);

        // Pitch bend = center + 1000 (roughly)
        MidiEvent pb;
        pb.type = MidiMessageType::PitchBend;
        pb.channel = 2;
        pb.data1 = 0x00; // LSB
        pb.data2 = 0x45; // MSB — gives ~+384
        decoder.process(pb);

        MPEExpression expr = decoder.get_note_expression(60);
        // Should have non-zero pitch bend from the member channel
        REQUIRE(expr.pitch_bend != 0);
    }

    SECTION("timbre (CC 74) updates note expression") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 2;
        on.data1 = 60;
        on.data2 = 100;
        decoder.process(on);

        MidiEvent cc;
        cc.type = MidiMessageType::ControlChange;
        cc.channel = 2;
        cc.data1 = MPEConstants::kCC_Timbre;
        cc.data2 = 100;
        decoder.process(cc);

        MPEExpression expr = decoder.get_note_expression(60);
        REQUIRE(expr.timbre == 100);
    }

    SECTION("channel pressure updates note expression") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 2;
        on.data1 = 60;
        on.data2 = 100;
        decoder.process(on);

        MidiEvent at;
        at.type = MidiMessageType::ChannelAftertouch;
        at.channel = 2;
        at.data1 = 80;
        decoder.process(at);

        MPEExpression expr = decoder.get_note_expression(60);
        REQUIRE(expr.pressure == 80);
    }

    SECTION("master channel pitch bend does not affect note expression") {
        MidiEvent pb;
        pb.type = MidiMessageType::PitchBend;
        pb.channel = MPEConstants::kMasterChannel;
        pb.data1 = 0x00;
        pb.data2 = 0x40; // Center
        decoder.process(pb);

        // No note mapped yet — should not crash
        REQUIRE(decoder.get_note_expression(60).pitch_bend == 0);
    }
}

TEST_CASE("MPEDecoder note-off releases channel", "[midi][mpe]") {
    MPEDecoder decoder;

    MidiEvent on;
    on.type = MidiMessageType::NoteOn;
    on.channel = 2;
    on.data1 = 60;
    on.data2 = 100;
    decoder.process(on);

    // Get expression while note is active
    REQUIRE(decoder.get_note_expression(60).pitch_bend == 0);

    // Note-off
    MidiEvent off;
    off.type = MidiMessageType::NoteOff;
    off.channel = 2;
    off.data1 = 60;
    off.data2 = 64;
    decoder.process(off);

    // Note expression should still be retrievable (not cleared on release)
    // The expression data is only cleared on reassign
    MPEExpression expr = decoder.get_note_expression(60);
    REQUIRE(expr.pitch_bend == 0);
    REQUIRE(expr.timbre == 64);
    REQUIRE(expr.pressure == 0);

    // Channel expression should still be default
    MPEExpression ch_expr = decoder.get_channel_expression(2);
    REQUIRE_FALSE(ch_expr.has_data());
}

TEST_CASE("MPEDecoder multiple notes on different channels", "[midi][mpe]") {
    MPEDecoder decoder;

    // Note 60 on channel 2
    MidiEvent on1;
    on1.type = MidiMessageType::NoteOn;
    on1.channel = 2;
    on1.data1 = 60;
    on1.data2 = 100;
    decoder.process(on1);

    // Note 64 on channel 3
    MidiEvent on2;
    on2.type = MidiMessageType::NoteOn;
    on2.channel = 3;
    on2.data1 = 64;
    on2.data2 = 100;
    decoder.process(on2);

    // Pitch bend on channel 2 only
    MidiEvent pb;
    pb.type = MidiMessageType::PitchBend;
    pb.channel = 2;
    pb.data1 = 0x00;
    pb.data2 = 0x50; // +~1024
    decoder.process(pb);

    MPEExpression expr60 = decoder.get_note_expression(60);
    MPEExpression expr64 = decoder.get_note_expression(64);

    // Note 60 should have bend, note 64 should not
    REQUIRE(expr60.pitch_bend != 0);
    // The expression for note 64 might also be non-zero if stored differently
    // In our implementation, pitch bend is stored per-channel then mapped to notes
    REQUIRE(expr64.pitch_bend == 0);
}

// ═══════════════════════════════════════════════════════════════
// MPEEncoder Tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MPEEncoder initial state", "[midi][mpe]") {
    MPEEncoder encoder;

    SECTION("get_note_expression returns default for unset note") {
        MPEExpression expr = encoder.get_note_expression(60);
        REQUIRE(expr.pitch_bend == 0);
        REQUIRE(expr.timbre == 64);
        REQUIRE(expr.pressure == 0);
    }

    SECTION("note_channel returns unmapped for unset note") {
        REQUIRE(encoder.note_channel(60) == 0xFF);
    }

    SECTION("reset does not crash") {
        encoder.reset();
        REQUIRE(encoder.note_channel(60) == 0xFF);
    }

    SECTION("generate_events with no changes returns empty") {
        auto events = encoder.generate_events();
        REQUIRE(events.empty());
    }
}

TEST_CASE("MPEEncoder set and generate expression", "[midi][mpe]") {
    MPEEncoder encoder;

    SECTION("set note expression generates events") {
        MPEExpression expr;
        expr.pitch_bend = 1000;
        expr.timbre = 80;
        expr.pressure = 70;

        encoder.set_note_expression(60, expr);
        auto events = encoder.generate_events();

        // Should generate pitch bend + timbre (pressure may also generate)
        REQUIRE_FALSE(events.empty());

        // At least one event should be a pitch bend
        bool has_pitch_bend = false;
        bool has_timbre = false;
        for (const auto& ev : events) {
            if (ev.type == MidiMessageType::PitchBend) has_pitch_bend = true;
            if (ev.type == MidiMessageType::ControlChange &&
                ev.data1 == MPEConstants::kCC_Timbre) has_timbre = true;
        }
        REQUIRE(has_pitch_bend);
        REQUIRE(has_timbre);
    }

    SECTION("get_note_expression returns last set value") {
        MPEExpression expr;
        expr.timbre = 90;
        encoder.set_note_expression(60, expr);

        MPEExpression retrieved = encoder.get_note_expression(60);
        REQUIRE(retrieved.timbre == 90);
    }
}

TEST_CASE("MPEEncoder channel assignment", "[midi][mpe]") {
    MPEEncoder encoder;

    SECTION("notes get consecutive channel assignments") {
        MPEExpression expr;
        expr.pitch_bend = 500;

        encoder.set_note_expression(60, expr);
        encoder.generate_events();
        REQUIRE(encoder.note_channel(60) == MPEConstants::kLowerZoneStart);

        encoder.set_note_expression(64, expr);
        encoder.generate_events();
        REQUIRE(encoder.note_channel(64) == MPEConstants::kLowerZoneStart + 1);
    }

    SECTION("note_channel returns correct channel after assignment") {
        MPEExpression expr;
        encoder.set_note_expression(60, expr);
        encoder.generate_events();

        uint8_t ch = encoder.note_channel(60);
        REQUIRE(ch != 0xFF);
        REQUIRE(ch >= MPEConstants::kLowerZoneStart);
    }
}

TEST_CASE("MPEEncoder multiple notes", "[midi][mpe]") {
    MPEEncoder encoder;

    SECTION("two notes generate separate events") {
        MPEExpression expr1, expr2;
        expr1.pitch_bend = 1000;
        expr2.pitch_bend = -500;

        encoder.set_note_expression(60, expr1);
        encoder.set_note_expression(64, expr2);

        auto events = encoder.generate_events();

        // Should generate events for both notes
        REQUIRE(events.size() >= 2);

        // Check channels are different
        uint8_t ch60 = encoder.note_channel(60);
        uint8_t ch64 = encoder.note_channel(64);
        REQUIRE(ch60 != ch64);
    }

    SECTION("same note updated twice generates events once") {
        MPEExpression expr1, expr2;
        expr1.pitch_bend = 500;
        expr2.pitch_bend = 1000;

        encoder.set_note_expression(60, expr1);
        encoder.set_note_expression(60, expr2);

        auto events = encoder.generate_events();
        // Only the latest expression for note 60 should be emitted
        REQUIRE(events.size() >= 1);
    }

    SECTION("reset clears all mappings") {
        MPEExpression expr;
        expr.pitch_bend = 500;
        encoder.set_note_expression(60, expr);
        encoder.generate_events();

        encoder.reset();
        REQUIRE(encoder.note_channel(60) == 0xFF);
        REQUIRE(encoder.generate_events().empty());
    }
}

TEST_CASE("MPEEncoder reset and re-use", "[midi][mpe]") {
    MPEEncoder encoder;

    SECTION("reset allows re-assignment") {
        MPEExpression expr;
        expr.pitch_bend = 500;

        // First use
        encoder.set_note_expression(60, expr);
        encoder.generate_events();
        uint8_t first_ch = encoder.note_channel(60);

        // Reset
        encoder.reset();

        // Second use — should assign same or different channel
        encoder.set_note_expression(60, expr);
        encoder.generate_events();
        uint8_t second_ch = encoder.note_channel(60);

        // Both should be valid member channels
        REQUIRE(first_ch >= MPEConstants::kLowerZoneStart);
        REQUIRE(second_ch >= MPEConstants::kLowerZoneStart);
    }
}

TEST_CASE("MPEEncoder pitch bend packing", "[midi][mpe]") {
    MPEEncoder encoder;

    SECTION("generate pitch bend events for positive bend") {
        MPEExpression expr;
        expr.pitch_bend = 8191; // Max positive
        encoder.set_note_expression(60, expr);

        auto events = encoder.generate_events();
        bool found = false;
        for (const auto& ev : events) {
            if (ev.type == MidiMessageType::PitchBend) {
                found = true;
                // data1 and data2 should be valid 7-bit values
                REQUIRE(ev.data1 < 128);
                REQUIRE(ev.data2 < 128);
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("generate pitch bend events for negative bend") {
        MPEExpression expr;
        expr.pitch_bend = -8192; // Max negative
        encoder.set_note_expression(60, expr);

        auto events = encoder.generate_events();
        bool found = false;
        for (const auto& ev : events) {
            if (ev.type == MidiMessageType::PitchBend) {
                found = true;
                REQUIRE(ev.data1 < 128);
                REQUIRE(ev.data2 < 128);
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("no pitch bend event for zero bend") {
        MPEExpression expr;
        expr.timbre = 100; // Change timbre but not pitch bend
        encoder.set_note_expression(60, expr);

        auto events = encoder.generate_events();
        for (const auto& ev : events) {
            REQUIRE_FALSE(ev.type == MidiMessageType::PitchBend);
        }
    }
}

TEST_CASE("MPEDecoder + MPEEncoder round-trip", "[midi][mpe]") {
    // Test: decode pitch bend from a MIDI event and encode it back
    MPEDecoder decoder;
    MPEEncoder encoder;

    // Simulate note on + pitch bend
    MidiEvent on;
    on.type = MidiMessageType::NoteOn;
    on.channel = 2;
    on.data1 = 60;
    on.data2 = 100;
    decoder.process(on);

    MidiEvent pb;
    pb.type = MidiMessageType::PitchBend;
    pb.channel = 2;
    pb.data1 = 0x00;
    pb.data2 = 0x50;
    decoder.process(pb);

    // Get decoded expression
    MPEExpression decoded = decoder.get_note_expression(60);

    // Encode it back
    encoder.set_note_expression(60, decoded);
    auto events = encoder.generate_events();

    // The encoder should generate a pitch bend event for this note
    bool has_pb = false;
    for (const auto& ev : events) {
        if (ev.type == MidiMessageType::PitchBend) {
            has_pb = true;
            break;
        }
    }
    REQUIRE(has_pb);
}
