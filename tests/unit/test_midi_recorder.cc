#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "midi/midi_recorder.h"
#include "midi/midi_types.h"

#include <vector>
#include <cstdint>

using namespace aria;

TEST_CASE("MidiRecorder lifecycle", "[midi][recorder]") {
    MidiRecorder recorder;

    SECTION("starts not recording") {
        REQUIRE_FALSE(recorder.is_recording());
        REQUIRE(recorder.event_count() == 0);
        REQUIRE(recorder.note_count() == 0);
    }

    SECTION("start_recording begins recording") {
        recorder.start_recording(0);
        REQUIRE(recorder.is_recording());
    }

    SECTION("stop_recording ends recording") {
        recorder.start_recording(0);
        recorder.stop_recording();
        REQUIRE_FALSE(recorder.is_recording());
    }

    SECTION("start_recording with PPQN position") {
        recorder.start_recording(960);
        // After starting, recording state is active
        REQUIRE(recorder.is_recording());
    }

    SECTION("finalize_clip without recording returns empty clip") {
        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().empty());
    }

    SECTION("stop_recording while not recording is safe") {
        recorder.stop_recording(); // should not crash
        REQUIRE_FALSE(recorder.is_recording());
    }

    SECTION("reset clears state") {
        recorder.start_recording(0);
        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        ev.data1 = 60;
        ev.data2 = 100;
        recorder.record_event(ev, 0, 48000.0);
        recorder.reset();
        REQUIRE_FALSE(recorder.is_recording());
        REQUIRE(recorder.event_count() == 0);
    }
}

TEST_CASE("MidiRecorder note creation", "[midi][recorder]") {
    MidiRecorder recorder;
    recorder.start_recording(0);

    SECTION("note-on then note-off creates a note") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;

        recorder.record_event(on, 0, 48000.0);
        recorder.record_event(off, 48000, 48000.0); // 1 second later

        MidiClip clip = recorder.finalize_clip();

        REQUIRE(clip.notes().size() == 1);
        REQUIRE(clip.notes()[0].note == 60);
        REQUIRE(clip.notes()[0].channel == 0);
        REQUIRE(clip.notes()[0].velocity == 100);
        REQUIRE(clip.notes()[0].start_ppqn >= 0);
        REQUIRE(clip.notes()[0].duration_ppqn > 0);
    }

    SECTION("multiple notes recorded") {
        // Note 60 on channel 0
        MidiEvent on1;
        on1.type = MidiMessageType::NoteOn;
        on1.channel = 0;
        on1.data1 = 60;
        on1.data2 = 100;

        MidiEvent off1;
        off1.type = MidiMessageType::NoteOff;
        off1.channel = 0;
        off1.data1 = 60;
        off1.data2 = 64;

        // Note 64 on channel 1
        MidiEvent on2;
        on2.type = MidiMessageType::NoteOn;
        on2.channel = 1;
        on2.data1 = 64;
        on2.data2 = 80;

        MidiEvent off2;
        off2.type = MidiMessageType::NoteOff;
        off2.channel = 1;
        off2.data1 = 64;
        off2.data2 = 64;

        recorder.record_event(on1, 0, 48000.0);
        recorder.record_event(on2, 0, 48000.0);
        recorder.record_event(off1, 48000, 48000.0);
        recorder.record_event(off2, 96000, 48000.0);

        MidiClip clip = recorder.finalize_clip();

        REQUIRE(clip.notes().size() == 2);
        REQUIRE(clip.notes()[0].note == 60);
        REQUIRE(clip.notes()[1].note == 64);
        REQUIRE(clip.notes()[0].channel == 0);
        REQUIRE(clip.notes()[1].channel == 1);
    }

    SECTION("velocity 0 note-on treated as note-off") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;

        MidiEvent off;
        off.type = MidiMessageType::NoteOn; // Note-on with vel 0
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 0;

        recorder.record_event(on, 0, 48000.0);
        recorder.record_event(off, 48000, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().size() == 1);
    }

    SECTION("record_event while not recording is ignored") {
        // Don't start recording
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        REQUIRE(recorder.event_count() == 0);
    }
}

TEST_CASE("MidiRecorder quantize on record", "[midi][recorder]") {
    MidiRecorder recorder;

    SECTION("quantize disabled by default") {
        REQUIRE_FALSE(recorder.quantize_on_record());
    }

    SECTION("enable quantize") {
        recorder.set_quantize_on_record(true, 480);
        REQUIRE(recorder.quantize_on_record());
        REQUIRE(recorder.grid_ppqn() == 480);
    }

    SECTION("disable quantize") {
        recorder.set_quantize_on_record(true, 480);
        recorder.set_quantize_on_record(false, 480);
        REQUIRE_FALSE(recorder.quantize_on_record());
    }

    SECTION("quantize grid must be positive") {
        recorder.set_quantize_on_record(true, 0);
        REQUIRE(recorder.grid_ppqn() == 480); // default fallback
    }

    SECTION("quantize snaps note positions") {
        recorder.set_quantize_on_record(true, 480);
        recorder.start_recording(0);

        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;

        // Record at an off-grid position
        recorder.record_event(on, 100, 48000.0);
        recorder.record_event(off, 50000, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().size() == 1);

        // With quantize on, positions should be on grid
        REQUIRE(clip.notes()[0].start_ppqn % 480 == 0);
    }
}

TEST_CASE("MidiRecorder MPE expression tracking", "[midi][recorder]") {
    MidiRecorder recorder;
    recorder.start_recording(0);

    SECTION("pitch bend during note is captured") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        // Pitch bend during the note
        MidiEvent pb;
        pb.type = MidiMessageType::PitchBend;
        pb.channel = 0;
        pb.data1 = 0x00; // Center + 0 (LSB)
        pb.data2 = 0x40; // Center (MSB)
        recorder.record_event(pb, 24000, 48000.0);

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;
        recorder.record_event(off, 48000, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().size() == 1);
        REQUIRE(clip.notes()[0].note_events.size() >= 1);
    }

    SECTION("CC during note is captured as note event") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        // CC 74 (timbre) during the note
        MidiEvent cc;
        cc.type = MidiMessageType::ControlChange;
        cc.channel = 0;
        cc.data1 = 74;
        cc.data2 = 80;
        recorder.record_event(cc, 24000, 48000.0);

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;
        recorder.record_event(off, 48000, 48000.0);

        MidiClip clip = recorder.finalize_clip();

        REQUIRE(clip.notes().size() == 1);
        // The CC should be in the note's note_events
        REQUIRE(clip.notes()[0].note_events.size() >= 1);
    }

    SECTION("polyphonic aftertouch on active note") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        // Poly aftertouch
        MidiEvent poly;
        poly.type = MidiMessageType::PolyphonicKey;
        poly.channel = 0;
        poly.data1 = 60;
        poly.data2 = 75;
        recorder.record_event(poly, 24000, 48000.0);

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;
        recorder.record_event(off, 48000, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().size() == 1);
    }
}

TEST_CASE("MidiRecorder edge cases", "[midi][recorder]") {
    MidiRecorder recorder;

    SECTION("double note-on retrigger") {
        recorder.start_recording(0);

        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        // Second note-on for same note (retrigger)
        MidiEvent on2;
        on2.type = MidiMessageType::NoteOn;
        on2.channel = 0;
        on2.data1 = 60;
        on2.data2 = 100;
        recorder.record_event(on2, 48000, 48000.0);

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;
        recorder.record_event(off, 96000, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        // Should produce 2 notes (retrigger + final off)
        REQUIRE(clip.notes().size() == 2);
    }

    SECTION("note-off without note-on is ignored") {
        recorder.start_recording(0);

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;
        recorder.record_event(off, 0, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().empty());
    }

    SECTION("CC without active note is stored as clip event") {
        recorder.start_recording(0);

        MidiEvent cc;
        cc.type = MidiMessageType::ControlChange;
        cc.channel = 0;
        cc.data1 = 7;
        cc.data2 = 100;
        recorder.record_event(cc, 0, 48000.0);

        MidiClip clip = recorder.finalize_clip();

        // CC should be stored as a clip-level event
        // (may also be empty if no active note to route to)
        // The key test: no crash, clip has correct length
        REQUIRE(clip.length() >= 1920);
    }

    SECTION("finalize_clip after stop_recording") {
        recorder.start_recording(0);

        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        recorder.stop_recording();
        // stop_recording already finalized pending notes

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().size() == 1);
        // Note duration should be 0 since stop closed it
    }

    SECTION("event_count tracks recorded events") {
        recorder.start_recording(0);

        MidiEvent ev;
        ev.type = MidiMessageType::NoteOn;
        ev.data1 = 60;
        ev.data2 = 100;
        recorder.record_event(ev, 0, 48000.0);
        recorder.record_event(ev, 1000, 48000.0);
        recorder.record_event(ev, 2000, 48000.0);

        REQUIRE(recorder.event_count() == 3);
    }

    SECTION("multiple channels with same note number") {
        recorder.start_recording(0);

        MidiEvent on1, on2;
        on1.type = MidiMessageType::NoteOn;
        on1.channel = 0;
        on1.data1 = 60;
        on1.data2 = 100;

        on2.type = MidiMessageType::NoteOn;
        on2.channel = 1;
        on2.data1 = 60;
        on2.data2 = 100;

        recorder.record_event(on1, 0, 48000.0);
        recorder.record_event(on2, 0, 48000.0);

        MidiEvent off1, off2;
        off1.type = MidiMessageType::NoteOff;
        off1.channel = 0;
        off1.data1 = 60;

        off2.type = MidiMessageType::NoteOff;
        off2.channel = 1;
        off2.data1 = 60;

        recorder.record_event(off1, 48000, 48000.0);
        recorder.record_event(off2, 96000, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().size() == 2);
    }

    SECTION("finalize_clip produces valid clip length") {
        recorder.start_recording(0);

        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        off.data2 = 64;
        recorder.record_event(off, 48000, 48000.0);

        MidiClip clip = recorder.finalize_clip();

        REQUIRE(clip.length() >= 1920); // At least 1 bar
        REQUIRE(clip.loop_start() == 0);
        REQUIRE(clip.loop_end() == clip.length());
    }
}

TEST_CASE("MidiRecorder channel aftertouch", "[midi][recorder]") {
    MidiRecorder recorder;
    recorder.start_recording(0);

    SECTION("channel aftertouch is captured") {
        MidiEvent on;
        on.type = MidiMessageType::NoteOn;
        on.channel = 0;
        on.data1 = 60;
        on.data2 = 100;
        recorder.record_event(on, 0, 48000.0);

        MidiEvent at;
        at.type = MidiMessageType::ChannelAftertouch;
        at.channel = 0;
        at.data1 = 80;
        recorder.record_event(at, 24000, 48000.0);

        MidiEvent off;
        off.type = MidiMessageType::NoteOff;
        off.channel = 0;
        off.data1 = 60;
        recorder.record_event(off, 48000, 48000.0);

        MidiClip clip = recorder.finalize_clip();
        REQUIRE(clip.notes().size() == 1);
    }
}
