#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mixer/master_channel.h"
#include "audio/audio_buffer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace aria;

// ═══════════════════════════════════════════════════════════════════
//  Volume
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("MasterChannel volume", "[mixer][master][volume]") {
    MasterChannel mc;

    SECTION("default volume is 0 dB") {
        REQUIRE(mc.volume() == Catch::Approx(0.0));
    }

    SECTION("set_volume clamps to +24 dB max") {
        mc.set_volume(30.0);
        REQUIRE(mc.volume() == Catch::Approx(24.0));
    }

    SECTION("set_volume clamps to -120 dB min") {
        mc.set_volume(-200.0);
        REQUIRE(mc.volume() == Catch::Approx(-120.0));
    }

    SECTION("set_volume accepts normal range") {
        mc.set_volume(-6.0);
        REQUIRE(mc.volume() == Catch::Approx(-6.0));
    }

    SECTION("process applies volume gain") {
        constexpr uint32_t kFrames = 64;

        float in_data[2][kFrames], out_data[2][kFrames];
        for (uint32_t i = 0; i < kFrames; ++i) {
            in_data[0][i] = 1.0f;
            in_data[1][i] = 0.5f;
            out_data[0][i] = 0.0f;
            out_data[1][i] = 0.0f;
        }

        AudioBuffer input, output;
        input.frames   = kFrames;
        input.channels = 2;
        input.capacity = kFrames;
        input.data[0]  = in_data[0];
        input.data[1]  = in_data[1];

        output.frames   = kFrames;
        output.channels = 2;
        output.capacity = kFrames;
        output.data[0]  = out_data[0];
        output.data[1]  = out_data[1];

        mc.set_volume(-6.0); // ~0.5 linear
        mc.process(&input, &output, kFrames);

        float expected = static_cast<float>(Channel::db_to_linear(-6.0));
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out_data[0][i] == Catch::Approx(expected).margin(0.001f));
            REQUIRE(out_data[1][i] == Catch::Approx(expected * 0.5f).margin(0.001f));
        }
    }

    SECTION("process with 0 dB is passthrough") {
        constexpr uint32_t kFrames = 32;
        float in[kFrames], out[kFrames];
        for (uint32_t i = 0; i < kFrames; ++i) {
            in[i] = 0.75f;
            out[i] = 0.0f;
        }

        AudioBuffer input, output;
        input.frames = output.frames = kFrames;
        input.channels = output.channels = 1;
        input.capacity = output.capacity = kFrames;
        input.data[0] = in;
        output.data[0] = out;

        mc.set_volume(0.0);
        mc.process(&input, &output, kFrames);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out[i] == Catch::Approx(in[i]).margin(1e-6f));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Limiter
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("MasterChannel limiter configuration", "[mixer][master][limiter]") {
    MasterChannel mc;

    SECTION("default limiter is enabled with -1 dB threshold") {
        auto cfg = mc.limiter();
        REQUIRE(cfg.enabled == true);
        REQUIRE(cfg.threshold_db == Catch::Approx(-1.0));
    }

    SECTION("set_limiter clamps threshold to valid range") {
        MasterChannel::LimiterConfig cfg;
        cfg.threshold_db = -20.0; // Too low
        mc.set_limiter(cfg);
        REQUIRE(mc.limiter().threshold_db == Catch::Approx(-6.0));
    }

    SECTION("set_limiter clamps threshold above -0.1") {
        MasterChannel::LimiterConfig cfg;
        cfg.threshold_db = 0.0; // Too high
        mc.set_limiter(cfg);
        REQUIRE(mc.limiter().threshold_db == Catch::Approx(-0.1));
    }

    SECTION("set_limiter clamps release minimum") {
        MasterChannel::LimiterConfig cfg;
        cfg.release_ms = 0.0;
        mc.set_limiter(cfg);
        REQUIRE(mc.limiter().release_ms >= 1.0);
    }

    SECTION("set_limiter accepts valid config") {
        MasterChannel::LimiterConfig cfg;
        cfg.enabled = true;
        cfg.threshold_db = -3.0;
        cfg.ceiling_db = -0.5;
        cfg.release_ms = 50.0;
        mc.set_limiter(cfg);

        auto read = mc.limiter();
        REQUIRE(read.enabled == true);
        REQUIRE(read.threshold_db == Catch::Approx(-3.0));
        REQUIRE(read.ceiling_db == Catch::Approx(-0.5));
        REQUIRE(read.release_ms == Catch::Approx(50.0));
    }

    SECTION("limiter can be disabled") {
        MasterChannel::LimiterConfig cfg;
        cfg.enabled = false;
        mc.set_limiter(cfg);
        REQUIRE(mc.limiter().enabled == false);
    }
}

TEST_CASE("MasterChannel brickwall limiter attenuates peaks", "[mixer][master][limiter]") {
    MasterChannel mc;
    mc.set_volume(0.0);

    constexpr uint32_t kFrames = 128;
    float in_data[2][kFrames], out_data[2][kFrames];

    // Signal that exceeds -1 dB threshold (threshold = -1 dB → ~0.891 linear)
    for (uint32_t i = 0; i < kFrames; ++i) {
        in_data[0][i] = 1.0f;  // 0 dBFS — exceeds -1 dB threshold
        in_data[1][i] = 1.0f;
        out_data[0][i] = 0.0f;
        out_data[1][i] = 0.0f;
    }

    AudioBuffer input, output;
    input.frames   = kFrames;
    input.channels = 2;
    input.capacity = kFrames;
    input.data[0]  = in_data[0];
    input.data[1]  = in_data[1];

    output.frames   = kFrames;
    output.channels = 2;
    output.capacity = kFrames;
    output.data[0]  = out_data[0];
    output.data[1]  = out_data[1];

    mc.process(&input, &output, kFrames);

    // Output should be reduced below (or at) threshold level
    float threshold_linear = std::pow(10.0f, -1.0f / 20.0f);
    for (uint32_t i = 0; i < kFrames; ++i) {
        REQUIRE(out_data[0][i] <= threshold_linear + 0.01f);
        REQUIRE(out_data[1][i] <= threshold_linear + 0.01f);
    }
}

TEST_CASE("MasterChannel limiter gain reduction is reported", "[mixer][master][limiter]") {
    MasterChannel mc;
    mc.set_volume(0.0);

    constexpr uint32_t kFrames = 64;
    float in[kFrames], out[kFrames];
    for (uint32_t i = 0; i < kFrames; ++i) {
        in[i] = 1.0f;
        out[i] = 0.0f;
    }

    AudioBuffer input, output;
    input.frames = output.frames = kFrames;
    input.channels = output.channels = 1;
    input.capacity = output.capacity = kFrames;
    input.data[0] = in;
    output.data[0] = out;

    mc.process(&input, &output, kFrames);

    // Signal at 0 dBFS with -1 dB threshold should show gain reduction
    REQUIRE(mc.gain_reduction_db() < 0.0f);
}

TEST_CASE("MasterChannel disabled limiter passes signal unchanged", "[mixer][master][limiter]") {
    MasterChannel mc;
    mc.set_volume(0.0);

    MasterChannel::LimiterConfig cfg;
    cfg.enabled = false;
    mc.set_limiter(cfg);

    constexpr uint32_t kFrames = 64;
    float in[kFrames], out[kFrames];

    // Burst of high amplitude
    for (uint32_t i = 0; i < 10; ++i) in[i] = 1.0f;
    for (uint32_t i = 10; i < kFrames; ++i) in[i] = 0.0f;
    std::memset(out, 0, sizeof(out));

    AudioBuffer input, output;
    input.frames = output.frames = kFrames;
    input.channels = output.channels = 1;
    input.capacity = output.capacity = kFrames;
    input.data[0] = in;
    output.data[0] = out;

    mc.process(&input, &output, kFrames);

    // With limiter disabled, signal should pass through at full amplitude
    for (uint32_t i = 0; i < 10; ++i) {
        REQUIRE(out[i] == Catch::Approx(1.0f).margin(0.001f));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Dither
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("MasterChannel dither configuration", "[mixer][master][dither]") {
    MasterChannel mc;

    SECTION("default dither is None") {
        REQUIRE(mc.dither() == MasterChannel::None);
    }

    SECTION("default bit depth is 24") {
        REQUIRE(mc.bit_depth() == 24);
    }

    SECTION("set_dither changes dither type") {
        mc.set_dither(MasterChannel::Triangular, 16);
        REQUIRE(mc.dither() == MasterChannel::Triangular);
        REQUIRE(mc.bit_depth() == 16);
    }

    SECTION("set_dither clamps bit depth") {
        mc.set_dither(MasterChannel::Shaped, 4);
        REQUIRE(mc.bit_depth() == 8);
    }

    SECTION("set_dither limits bit depth to 32") {
        mc.set_dither(MasterChannel::Triangular, 64);
        REQUIRE(mc.bit_depth() == 32);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  FX Chain
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("MasterChannel FX chain", "[mixer][master][fx]") {
    MasterChannel mc;

    SECTION("default FX count is 0") {
        REQUIRE(mc.fx_count() == 0);
    }

    SECTION("add_fx adds a plugin") {
        mc.add_fx(1001, 0);
        REQUIRE(mc.fx_count() == 1);
    }

    SECTION("add_fx at position appends") {
        mc.add_fx(1001, 0);
        mc.add_fx(1002, 10);
        REQUIRE(mc.fx_count() == 2);
    }

    SECTION("add_fx inserts at specified position") {
        mc.add_fx(1001, 0);
        mc.add_fx(1002, 0);
        REQUIRE(mc.fx_count() == 2);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Processing Edge Cases
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("MasterChannel process edge cases", "[mixer][master][process]") {
    MasterChannel mc;

    SECTION("process with null input does nothing") {
        float out[32] = {42.0f};
        AudioBuffer output;
        output.data[0] = out;
        mc.process(nullptr, &output, 32);
        REQUIRE(out[0] == Catch::Approx(42.0f));
    }

    SECTION("process with null output does nothing") {
        float in[32] = {};
        AudioBuffer input;
        input.data[0] = in;
        mc.process(&input, nullptr, 32);
    }

    SECTION("process with 0 frames does nothing") {
        float in[4] = {1.0f}, out[4] = {0.0f};
        AudioBuffer input, output;
        input.data[0] = in;   input.frames = 4;
        output.data[0] = out; output.frames = 4;
        mc.process(&input, &output, 0);
        REQUIRE(out[0] == Catch::Approx(0.0f));
    }

    SECTION("process with single channel is valid") {
        constexpr uint32_t kFrames = 32;
        float in[kFrames], out[kFrames];
        for (uint32_t i = 0; i < kFrames; ++i) in[i] = 0.5f;

        AudioBuffer input, output;
        input.frames = output.frames = kFrames;
        input.channels = output.channels = 1;
        input.capacity = output.capacity = kFrames;
        input.data[0] = in;
        output.data[0] = out;

        mc.set_volume(0.0);
        REQUIRE_NOTHROW(mc.process(&input, &output, kFrames));

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out[i] == Catch::Approx(0.5f).margin(0.001f));
        }
    }
}
