#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "audio/audio_clock.h"

using namespace aria;

TEST_CASE("AudioClock — basic position tracking", "[audio][clock]") {
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    SECTION("initial state") {
        REQUIRE(clock.sample_position() == 0);
        REQUIRE(clock.current_time() == Catch::Approx(0.0));
        REQUIRE(clock.current_bpm() == Catch::Approx(120.0));
    }

    SECTION("process advances position") {
        clock.process(128);
        REQUIRE(clock.sample_position() == 128);

        clock.process(256);
        REQUIRE(clock.sample_position() == 384);
    }

    SECTION("time conversion") {
        clock.process(48000);  // 1 second at 48kHz
        REQUIRE(clock.current_time() == Catch::Approx(1.0));
    }

    SECTION("multiple callbacks") {
        for (int i = 0; i < 100; ++i) {
            clock.process(128);
        }
        REQUIRE(clock.sample_position() == 12800);
        REQUIRE(clock.current_time() == Catch::Approx(12800.0 / 48000.0));
    }
}

TEST_CASE("AudioClock — BPM tracking with tempo map", "[audio][clock]") {
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    TempoMap map;
    map.set_events({
        {0,      120.0},
        {48000,  140.0},  // change at 1 second
        {96000,  100.0},  // change at 2 seconds
    });
    clock.set_tempo_map(map);

    SECTION("BPM starts at first tempo event") {
        clock.process(1);
        REQUIRE(clock.current_bpm() == Catch::Approx(120.0));
    }

    SECTION("BPM updates after tempo change boundary") {
        // Process up to the tempo change at 48000
        // After process(48000), position = 48000
        clock.process(48000);
        REQUIRE(clock.current_bpm() == Catch::Approx(140.0));
    }

    SECTION("BPM follows through multiple tempo changes") {
        clock.process(48000);  // pos=48000, bpm=140
        REQUIRE(clock.current_bpm() == Catch::Approx(140.0));

        clock.process(48000);  // pos=96000, bpm=100
        REQUIRE(clock.current_bpm() == Catch::Approx(100.0));
    }

    SECTION("reset restores default BPM") {
        clock.process(48000);  // triggers 140 BPM
        REQUIRE(clock.current_bpm() == Catch::Approx(140.0));

        clock.reset();
        REQUIRE(clock.sample_position() == 0);
        REQUIRE(clock.current_bpm() == Catch::Approx(120.0));
    }
}

TEST_CASE("AudioClock — beat and measure", "[audio][clock]") {
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    SECTION("beat position at start") {
        REQUIRE(clock.beat_position() == Catch::Approx(0.0));
        REQUIRE(clock.current_measure() == 1);
        REQUIRE(clock.current_beat() == 0);
    }

    SECTION("beat position after one beat at 120 BPM") {
        // At 120 BPM, one beat = 0.5 seconds = 24000 samples
        clock.process(24000);
        REQUIRE(clock.beat_position() == Catch::Approx(0.5));   // half a beat
        REQUIRE(clock.current_measure() == 1);                     // still bar 1
        REQUIRE(clock.current_beat() == 0);                        // still beat 0

        clock.process(24000);  // total = 48000 samples = 1.0 sec
        REQUIRE(clock.beat_position() == Catch::Approx(2.0));   // 2 beats (120 BPM * 1 sec / 60)
        REQUIRE(clock.current_measure() == 1);
        REQUIRE(clock.current_beat() == 2);  // 3rd beat in 4/4
    }

    SECTION("measure advances after 4 beats") {
        // 4 beats at 120 BPM = 2 seconds = 96000 samples
        clock.process(96000);
        REQUIRE(clock.current_measure() == 2);  // bar 2
        REQUIRE(clock.current_beat() == 0);     // beat 0 of bar 2
    }
}

TEST_CASE("AudioClock — set_sample_rate affects time", "[audio][clock]") {
    AudioClock clock;
    clock.set_sample_rate(44100.0);
    clock.process(44100);

    REQUIRE(clock.sample_position() == 44100);
    REQUIRE(clock.current_time() == Catch::Approx(1.0));
    REQUIRE(clock.sample_rate() == Catch::Approx(44100.0));
}

TEST_CASE("AudioClock — reset", "[audio][clock]") {
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    clock.process(1000);
    REQUIRE(clock.sample_position() == 1000);

    clock.reset();
    REQUIRE(clock.sample_position() == 0);
    REQUIRE(clock.current_time() == Catch::Approx(0.0));
    REQUIRE(clock.current_bpm() == Catch::Approx(120.0));
}
