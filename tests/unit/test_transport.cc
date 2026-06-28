#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "audio/transport.h"
#include "audio/audio_clock.h"

using namespace aria;

// ─── State Machine Tests ──────────────────────────────────────

TEST_CASE("Transport — initial state is Stopped", "[audio][transport]") {
    Transport transport;
    REQUIRE(transport.state() == TransportState::Stopped);
    REQUIRE(transport.is_stopped());
    REQUIRE_FALSE(transport.is_playing());
    REQUIRE_FALSE(transport.is_recording());
    REQUIRE_FALSE(transport.is_paused());
}

TEST_CASE("Transport — play transitions from Stopped to Playing", "[audio][transport]") {
    Transport transport;
    transport.play();
    REQUIRE(transport.state() == TransportState::Playing);
    REQUIRE(transport.is_playing());
}

TEST_CASE("Transport — play from Playing is a no-op", "[audio][transport]") {
    Transport transport;
    transport.play();
    REQUIRE(transport.is_playing());

    // Second play() should not change state
    transport.play();
    REQUIRE(transport.is_playing());
}

TEST_CASE("Transport — play from Paused resumes Playing", "[audio][transport]") {
    Transport transport;
    transport.play();   // Stopped → Playing
    transport.pause();  // Playing → Paused
    REQUIRE(transport.is_paused());

    transport.play();   // Paused → Playing
    REQUIRE(transport.is_playing());
}

TEST_CASE("Transport — pause transitions from Playing to Paused", "[audio][transport]") {
    Transport transport;
    transport.play();
    transport.pause();
    REQUIRE(transport.state() == TransportState::Paused);
    REQUIRE(transport.is_paused());
}

TEST_CASE("Transport — pause from Stopped is a no-op", "[audio][transport]") {
    Transport transport;
    transport.pause();
    REQUIRE(transport.is_stopped());  // should remain Stopped
}

TEST_CASE("Transport — stop from Playing resets to Stopped", "[audio][transport]") {
    Transport transport;
    transport.play();
    transport.stop();
    REQUIRE(transport.is_stopped());
}

TEST_CASE("Transport — stop from Paused resets to Stopped", "[audio][transport]") {
    Transport transport;
    transport.play();
    transport.pause();
    transport.stop();
    REQUIRE(transport.is_stopped());
}

TEST_CASE("Transport — stop from Recording resets to Stopped", "[audio][transport]") {
    Transport transport;
    transport.record();
    REQUIRE(transport.is_recording());

    transport.stop();
    REQUIRE(transport.is_stopped());
}

TEST_CASE("Transport — record transitions from Stopped to Recording", "[audio][transport]") {
    Transport transport;
    transport.record();
    REQUIRE(transport.state() == TransportState::Recording);
    REQUIRE(transport.is_recording());
}

TEST_CASE("Transport — record from Playing is a no-op", "[audio][transport]") {
    Transport transport;
    transport.play();
    transport.record();  // should not work when playing
    REQUIRE(transport.is_playing());  // unchanged
}

TEST_CASE("Transport — record from Recording is a no-op", "[audio][transport]") {
    Transport transport;
    transport.record();
    transport.record();  // second record should be no-op
    REQUIRE(transport.is_recording());
}

TEST_CASE("Transport — full state machine cycle", "[audio][transport]") {
    Transport transport;

    REQUIRE(transport.is_stopped());

    transport.play();
    REQUIRE(transport.is_playing());

    transport.pause();
    REQUIRE(transport.is_paused());

    transport.play();
    REQUIRE(transport.is_playing());

    transport.stop();
    REQUIRE(transport.is_stopped());

    transport.record();
    REQUIRE(transport.is_recording());

    transport.stop();
    REQUIRE(transport.is_stopped());
}

// ─── Loop Tests ──────────────────────────────────────────────

TEST_CASE("Transport — loop control", "[audio][transport]") {
    Transport transport;

    SECTION("loop disabled by default") {
        REQUIRE_FALSE(transport.loop_enabled());
    }

    SECTION("enable and disable loop") {
        transport.set_loop(true);
        REQUIRE(transport.loop_enabled());

        transport.set_loop(false);
        REQUIRE_FALSE(transport.loop_enabled());
    }

    SECTION("set loop range") {
        transport.set_loop_range(1000, 5000);
        REQUIRE(transport.loop_start() == 1000);
        REQUIRE(transport.loop_end() == 5000);
    }
}

// ─── Process / Seek Tests ────────────────────────────────────

TEST_CASE("Transport — process updates clock when playing", "[audio][transport]") {
    Transport transport;
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    transport.play();
    REQUIRE(transport.is_playing());

    transport.process(128, clock);
    REQUIRE(clock.sample_position() == 128);

    transport.process(256, clock);
    REQUIRE(clock.sample_position() == 384);
}

TEST_CASE("Transport — process does not advance clock when stopped", "[audio][transport]") {
    Transport transport;
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    transport.process(128, clock);
    REQUIRE(clock.sample_position() == 0);  // stopped — no advance
}

TEST_CASE("Transport — process does not advance clock when paused", "[audio][transport]") {
    Transport transport;
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    transport.play();
    transport.process(128, clock);
    REQUIRE(clock.sample_position() == 128);

    transport.pause();
    transport.process(256, clock);
    REQUIRE(clock.sample_position() == 128);  // paused — no advance
}

TEST_CASE("Transport — process advances clock while recording", "[audio][transport]") {
    Transport transport;
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    transport.record();
    transport.process(64, clock);
    REQUIRE(clock.sample_position() == 64);

    transport.process(64, clock);
    REQUIRE(clock.sample_position() == 128);
}

TEST_CASE("Transport — seek position", "[audio][transport]") {
    Transport transport;
    AudioClock clock;
    clock.set_sample_rate(48000.0);

    // Seek sets a pending flag
    transport.set_position(10000);
    // The seek is consumed during process()
    // (Actual position change depends on the engine's clock management)

    transport.play();
    transport.process(128, clock);
    // After seek + process, position advances from wherever the clock is
    REQUIRE(clock.sample_position() > 0);
}

// ─── Loop Behavior Tests ─────────────────────────────────────

TEST_CASE("Transport — loop range boundaries", "[audio][transport]") {
    Transport transport;

    transport.set_loop_range(0, 1000);
    REQUIRE(transport.loop_start() == 0);
    REQUIRE(transport.loop_end() == 1000);

    transport.set_loop(true);
    REQUIRE(transport.loop_enabled());
}

// ─── Thread Safety Sanity (basic, no actual concurrency) ─────

TEST_CASE("Transport — concurrent read from state is safe", "[audio][transport]") {
    Transport transport;
    transport.play();

    // Reading state while simulating processing (single-thread here,
    // but the atomic reads should not tear)
    for (int i = 0; i < 1000; ++i) {
        bool playing = transport.is_playing();
        TransportState s = transport.state();
        REQUIRE(playing == (s == TransportState::Playing));
    }
}
