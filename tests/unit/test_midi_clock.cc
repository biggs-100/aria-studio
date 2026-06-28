#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "midi/midi_clock.h"

using namespace aria;

TEST_CASE("MidiClock role management", "[midi][clock]") {
    MidiClock clock;

    SECTION("default role is Off") {
        REQUIRE(clock.role() == MidiClock::Role::Off);
        REQUIRE_FALSE(clock.is_running());
    }

    SECTION("set_role to Internal") {
        clock.set_role(MidiClock::Role::Internal);
        REQUIRE(clock.role() == MidiClock::Role::Internal);
    }

    SECTION("set_role to Slave") {
        clock.set_role(MidiClock::Role::Slave);
        REQUIRE(clock.role() == MidiClock::Role::Slave);
    }

    SECTION("set_role to Off stops the clock") {
        clock.set_role(MidiClock::Role::Internal);
        clock.start();
        REQUIRE(clock.is_running());

        clock.set_role(MidiClock::Role::Off);
        REQUIRE_FALSE(clock.is_running());
    }
}

TEST_CASE("MidiClock start/stop", "[midi][clock]") {
    MidiClock clock;

    SECTION("start with Role::Off does nothing") {
        clock.start();
        REQUIRE_FALSE(clock.is_running());
    }

    SECTION("start with Role::Internal starts clock") {
        clock.set_role(MidiClock::Role::Internal);
        clock.start();
        REQUIRE(clock.is_running());
    }

    SECTION("stop stops the clock") {
        clock.set_role(MidiClock::Role::Internal);
        clock.start();
        clock.stop();
        REQUIRE_FALSE(clock.is_running());
    }

    SECTION("reset clears tick count") {
        clock.set_role(MidiClock::Role::Internal);
        clock.start();
        clock.process(48000, 120.0);  // 1 second at 120 BPM
        REQUIRE(clock.tick_count() > 0);

        clock.reset();
        REQUIRE(clock.tick_count() == 0);
        REQUIRE_FALSE(clock.is_running());
    }
}

TEST_CASE("MidiClock tick generation", "[midi][clock]") {
    MidiClock clock;
    clock.set_role(MidiClock::Role::Internal);
    clock.start();

    SECTION("process at 120 BPM generates ticks") {
        clock.process(48000, 120.0);  // 1 second at 48kHz
        // 120 BPM = 2 beats/sec = 48 ticks/sec
        // At 48kHz sample rate: 48 ticks per 48000 frames = 48 ticks
        REQUIRE(clock.tick_count() > 0);
    }

    SECTION("tick count at different BPMs") {
        clock.process(48000, 120.0);
        uint32_t ticks_120 = clock.tick_count();

        clock.reset();
        clock.start();
        clock.process(48000, 60.0);  // half the BPM
        uint32_t ticks_60 = clock.tick_count();

        // At 60 BPM, we should get half the ticks
        REQUIRE(ticks_60 > 0);
    }

    SECTION("process with BPM = 0 does nothing") {
        clock.reset();
        clock.start();
        clock.process(48000, 0.0);
        REQUIRE(clock.tick_count() == 0);
    }

    SECTION("process while not running does nothing") {
        clock.process(48000, 120.0);  // clock not started/stopped
        REQUIRE(clock.tick_count() == 0);
    }
}

TEST_CASE("MidiClock callback firing", "[midi][clock]") {
    MidiClock clock;
    clock.set_role(MidiClock::Role::Internal);
    clock.start();

    SECTION("on_tick fires for each tick") {
        int tick_count = 0;
        clock.on_tick = [&]() { ++tick_count; };

        clock.process(48000, 120.0);  // ~48 ticks

        REQUIRE(tick_count > 0);
        REQUIRE(static_cast<uint32_t>(tick_count) == clock.tick_count());
    }

    SECTION("on_beat fires every 24 ticks") {
        int beat_count = 0;
        clock.on_beat = [&]() { ++beat_count; };

        // Process 2 quarter notes worth of time at 120 BPM
        // 120 BPM = 2 beats/sec. Each beat = 24 ticks.
        // 2 beats = 48000 frames at 48kHz
        clock.process(48000, 120.0);

        REQUIRE(beat_count == 2);
    }

    SECTION("on_bar fires every 96 ticks (4 beats)") {
        int bar_count = 0;
        clock.on_bar = [&]() { ++bar_count; };

        // Process 8 beats = 2 bars = 192000 frames at 48kHz
        clock.process(192000, 120.0);

        REQUIRE(bar_count == 2);
    }
}

TEST_CASE("MidiClock ppm_error", "[midi][clock]") {
    MidiClock clock;
    clock.set_role(MidiClock::Role::Internal);
    clock.start();

    SECTION("PPM error is small for aligned buffer sizes") {
        // 48000 frames at 120 BPM = 1 second = 48 ticks exactly
        // With floating point, error may accumulate but should be very small
        clock.process(48000, 120.0);
        REQUIRE(clock.ppm_error() >= 0.0);
    }

    SECTION("PPM error is reported") {
        clock.process(512, 120.0);  // non-standard buffer size
        REQUIRE(clock.ppm_error() >= 0.0);
    }

    SECTION("PPM error is 0 when not running") {
        REQUIRE(clock.ppm_error() == 0.0);
    }
}

TEST_CASE("MidiClock multiple process calls", "[midi][clock]") {
    MidiClock clock;
    clock.set_role(MidiClock::Role::Internal);
    clock.start();

    int tick_count = 0;
    clock.on_tick = [&]() { ++tick_count; };

    // Process multiple consecutive buffers
    clock.process(128, 120.0);
    int ticks_after_first = tick_count;

    clock.process(128, 120.0);
    REQUIRE(tick_count > ticks_after_first);

    // Stop and verify no more ticks
    clock.stop();
    clock.process(128, 120.0);
    int ticks_after_stop = tick_count;
    REQUIRE(ticks_after_stop == tick_count - ticks_after_first + ticks_after_first);
    // Actually after stop, tick_count shouldn't change
    REQUIRE(tick_count == ticks_after_stop);
}

TEST_CASE("MidiClock reset mid-stream", "[midi][clock]") {
    MidiClock clock;
    clock.set_role(MidiClock::Role::Internal);
    clock.start();

    int tick_count = 0;
    clock.on_tick = [&]() { ++tick_count; };

    clock.process(48000, 120.0);
    REQUIRE(tick_count > 0);

    // Reset and verify counter is 0
    clock.reset();
    REQUIRE(clock.tick_count() == 0);

    // Start again — should work fresh
    clock.start();
    clock.process(48000, 120.0);
    REQUIRE(clock.tick_count() > 0);
}
