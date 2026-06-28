#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "audio/src_converter.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace aria;

// ═══════════════════════════════════════════════════════════════════
// SRCConverter Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("SRCConverter identity conversion (same rate)", "[audio][src]") {
    SRCConverter converter;
    converter.set_input_rate(48000);
    converter.set_output_rate(48000);

    constexpr uint32_t kFrames = 256;
    std::vector<float> input(kFrames);
    for (uint32_t i = 0; i < kFrames; ++i) {
        input[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);
    }

    SECTION("Fast quality preserves output at same rate") {
        converter.set_quality(SRCConverter::Quality::Fast);
        std::vector<float> output(converter.max_output_frames(kFrames));

        uint32_t out_frames = converter.process(
            input.data(), kFrames, output.data(),
            static_cast<uint32_t>(output.size()));

        // Should produce same number of frames
        REQUIRE(out_frames == kFrames);

        // Output should closely match input
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(output[i] == Catch::Approx(input[i]).margin(0.001f));
        }
    }

    SECTION("Medium quality preserves output at same rate") {
        converter.set_quality(SRCConverter::Quality::Medium);
        std::vector<float> output(converter.max_output_frames(kFrames));

        uint32_t out_frames = converter.process(
            input.data(), kFrames, output.data(),
            static_cast<uint32_t>(output.size()));

        REQUIRE(out_frames == kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(output[i] == Catch::Approx(input[i]).margin(0.001f));
        }
    }

    SECTION("High quality preserves output at same rate") {
        converter.set_quality(SRCConverter::Quality::High);
        std::vector<float> output(converter.max_output_frames(kFrames));

        uint32_t out_frames = converter.process(
            input.data(), kFrames, output.data(),
            static_cast<uint32_t>(output.size()));

        REQUIRE(out_frames == kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(output[i] == Catch::Approx(input[i]).margin(0.001f));
        }
    }

    SECTION("Best quality preserves output at same rate") {
        converter.set_quality(SRCConverter::Quality::Best);
        std::vector<float> output(converter.max_output_frames(kFrames));

        uint32_t out_frames = converter.process(
            input.data(), kFrames, output.data(),
            static_cast<uint32_t>(output.size()));

        REQUIRE(out_frames == kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(output[i] == Catch::Approx(input[i]).margin(0.001f));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// SRC: Upsampling (44.1k → 48k)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("SRCConverter upsampling 44.1k → 48k", "[audio][src]") {
    SRCConverter converter;
    converter.set_input_rate(44100);
    converter.set_output_rate(48000);

    constexpr uint32_t kFrames = 441; // 10ms
    std::vector<float> input(kFrames);
    for (uint32_t i = 0; i < kFrames; ++i) {
        input[i] = std::sin(2.0f * 3.14159f * 1000.0f * i / 44100.0f);
    }

    SECTION("Fast upsampling produces more output") {
        converter.set_quality(SRCConverter::Quality::Fast);
        std::vector<float> output(converter.max_output_frames(kFrames));

        uint32_t out_frames = converter.process(
            input.data(), kFrames, output.data(),
            static_cast<uint32_t>(output.size()));

        // 480/441 ratio: output should be ~480 frames
        REQUIRE(out_frames >= kFrames);
        REQUIRE(out_frames <= kFrames + 50);
    }

    SECTION("Medium upsampling produces more output") {
        converter.set_quality(SRCConverter::Quality::Medium);
        std::vector<float> output(converter.max_output_frames(kFrames));

        uint32_t out_frames = converter.process(
            input.data(), kFrames, output.data(),
            static_cast<uint32_t>(output.size()));

        REQUIRE(out_frames >= kFrames);
        REQUIRE(out_frames <= kFrames + 50);
    }

    SECTION("All quality levels produce similar output sizes") {
        uint32_t fast_out, med_out, high_out, best_out;

        converter.set_quality(SRCConverter::Quality::Fast);
        {
            std::vector<float> out(converter.max_output_frames(kFrames));
            fast_out = converter.process(input.data(), kFrames, out.data(),
                                          static_cast<uint32_t>(out.size()));
        }

        converter.reset();
        converter.set_quality(SRCConverter::Quality::Medium);
        {
            std::vector<float> out(converter.max_output_frames(kFrames));
            med_out = converter.process(input.data(), kFrames, out.data(),
                                         static_cast<uint32_t>(out.size()));
        }

        converter.reset();
        converter.set_quality(SRCConverter::Quality::High);
        {
            std::vector<float> out(converter.max_output_frames(kFrames));
            high_out = converter.process(input.data(), kFrames, out.data(),
                                          static_cast<uint32_t>(out.size()));
        }

        converter.reset();
        converter.set_quality(SRCConverter::Quality::Best);
        {
            std::vector<float> out(converter.max_output_frames(kFrames));
            best_out = converter.process(input.data(), kFrames, out.data(),
                                          static_cast<uint32_t>(out.size()));
        }

        // All should produce similar frame counts (±10%)
        REQUIRE(std::abs(static_cast<int>(fast_out - med_out)) < 10);
        REQUIRE(std::abs(static_cast<int>(fast_out - high_out)) < 10);
        REQUIRE(std::abs(static_cast<int>(fast_out - best_out)) < 10);
    }
}

// ═══════════════════════════════════════════════════════════════════
// SRC: Downsampling (48k → 44.1k)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("SRCConverter downsampling 48k → 44.1k", "[audio][src]") {
    SRCConverter converter;
    converter.set_input_rate(48000);
    converter.set_output_rate(44100);

    constexpr uint32_t kFrames = 480; // 10ms
    std::vector<float> input(kFrames);
    for (uint32_t i = 0; i < kFrames; ++i) {
        input[i] = std::sin(2.0f * 3.14159f * 500.0f * i / 48000.0f);
    }

    SECTION("Fast downsampling produces fewer output frames") {
        converter.set_quality(SRCConverter::Quality::Fast);
        std::vector<float> output(converter.max_output_frames(kFrames));

        uint32_t out_frames = converter.process(
            input.data(), kFrames, output.data(),
            static_cast<uint32_t>(output.size()));

        // 441/480 ratio: output should be ~441 frames
        REQUIRE(out_frames <= kFrames);
        REQUIRE(static_cast<int>(out_frames) >= static_cast<int>(kFrames) - 50);
    }
}

// ═══════════════════════════════════════════════════════════════════
// SRC: Impulse response
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("SRCConverter impulse response", "[audio][src]") {
    SRCConverter converter;
    converter.set_input_rate(48000);
    converter.set_output_rate(48000); // identity for clean impulse

    SECTION("impulse produces expected output") {
        converter.set_quality(SRCConverter::Quality::High);

        std::vector<float> impulse(64, 0.0f);
        impulse[0] = 1.0f; // unit impulse at position 0

        std::vector<float> output(converter.max_output_frames(64));
        uint32_t out_frames = converter.process(
            impulse.data(), 64, output.data(),
            static_cast<uint32_t>(output.size()));

        // With identity conversion, impulse should be preserved
        REQUIRE(out_frames == 64);
        REQUIRE(output[0] == Catch::Approx(1.0f).margin(0.1f));
    }
}

// ═══════════════════════════════════════════════════════════════════
// SRC: Max output frames calculation
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("SRCConverter max_output_frames estimation", "[audio][src]") {
    SRCConverter converter;
    converter.set_input_rate(48000);
    converter.set_output_rate(96000); // 2x upsampling

    SECTION("upsampling 2x") {
        uint32_t max_frames = converter.max_output_frames(100);
        // 2x ratio should give ~200+
        REQUIRE(max_frames >= 200);
        REQUIRE(max_frames <= 300);
    }

    SECTION("downsampling 2x") {
        converter.set_input_rate(96000);
        converter.set_output_rate(48000);
        uint32_t max_frames = converter.max_output_frames(100);
        REQUIRE(max_frames >= 50);
    }
}

// ═══════════════════════════════════════════════════════════════════
// SRC: Sine wave preservation (qualitative)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("SRCConverter sine wave preservation across quality levels", "[audio][src]") {
    SRCConverter converter;
    converter.set_input_rate(48000);
    converter.set_output_rate(48000);

    constexpr uint32_t kFrames = 1024;
    std::vector<float> input(kFrames);
    for (uint32_t i = 0; i < kFrames; ++i) {
        input[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);
    }

    SECTION("reset clears fractional position") {
        converter.set_quality(SRCConverter::Quality::Fast);
        std::vector<float> out1(converter.max_output_frames(kFrames));
        std::vector<float> out2(converter.max_output_frames(kFrames));

        converter.process(input.data(), kFrames, out1.data(),
                          static_cast<uint32_t>(out1.size()));
        converter.reset();
        converter.process(input.data(), kFrames, out2.data(),
                          static_cast<uint32_t>(out2.size()));

        // Both should produce same result
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out1[i] == Catch::Approx(out2[i]).margin(0.001f));
        }
    }
}
