#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "audio/export/dither.h"
#include "audio/export/file_writer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

using namespace aria;

// ═══════════════════════════════════════════════════════════════════
// Dither Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("Dither::apply_none leaves data unchanged", "[audio][export][dither]") {
    constexpr uint32_t kFrames = 64;
    float data[kFrames];
    float ref[kFrames];

    for (uint32_t i = 0; i < kFrames; ++i) {
        data[i] = 0.5f * std::sin(2.0f * 3.14159f * 10.0f * i / kFrames);
        ref[i] = data[i];
    }

    Dither::apply_none(data, kFrames, 1);
    for (uint32_t i = 0; i < kFrames; ++i) {
        REQUIRE(data[i] == ref[i]);
    }
}

TEST_CASE("Dither::apply_triangular adds noise within expected range", "[audio][export][dither]") {
    constexpr uint32_t kFrames = 4096;
    std::vector<float> data(kFrames, 0.0f);

    Dither::apply_triangular(data.data(), kFrames, 1);

    // TPDF: triangular distribution between ±2 LSB (two uniforms each ±1)
    // The sum of two uniform [-1,1] yields triangular [-2,2]
    // Values should be in range [-2, 2]
    float min_val = *std::min_element(data.begin(), data.end());
    float max_val = *std::max_element(data.begin(), data.end());

    REQUIRE(min_val >= -2.1f);
    REQUIRE(max_val <= 2.1f);

    // Mean should be near zero
    double sum = 0.0;
    for (auto v : data) sum += v;
    double mean = sum / data.size();
    REQUIRE(std::abs(mean) < 0.1);
}

TEST_CASE("Dither::apply_shaped produces noise-shaped output", "[audio][export][dither]") {
    constexpr uint32_t kFrames = 4096;
    std::vector<float> data(kFrames, 0.0f);

    Dither::reset();
    Dither::apply_shaped(data.data(), kFrames, 1);

    // Shaped dither should also be within reasonable range
    float min_val = *std::min_element(data.begin(), data.end());
    float max_val = *std::max_element(data.begin(), data.end());
    REQUIRE(min_val >= -3.0f);
    REQUIRE(max_val <= 3.0f);
}

// ═══════════════════════════════════════════════════════════════════
// WAV Writer Tests — Roundtrip
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("FileWriter::write_wav writes and reads back correctly", "[audio][export][wav]") {
    constexpr uint32_t kFrames = 256;
    constexpr uint32_t kChannels = 2;
    constexpr uint32_t kSampleRate = 48000;

    // Generate a simple test signal
    std::vector<float> data(kFrames * kChannels);
    for (uint32_t i = 0; i < kFrames; ++i) {
        data[i * 2]     = 0.5f * std::sin(2.0f * 3.14159f * 440.0f * i / kSampleRate);
        data[i * 2 + 1] = 0.3f * std::cos(2.0f * 3.14159f * 880.0f * i / kSampleRate);
    }

    const std::string test_path = "test_output_16bit.wav";

    SECTION("16-bit WAV roundtrip") {
        REQUIRE(FileWriter::write_wav(test_path, data.data(), kChannels, kFrames,
                                       kSampleRate, 16, DitherType::None));

        // Verify file exists and has correct size
        std::ifstream in(test_path, std::ios::binary | std::ios::ate);
        REQUIRE(in.good());
        auto size = in.tellg();
        REQUIRE(size > 44); // WAV header is at least 44 bytes

        // Read back header
        in.seekg(0);
        char header[44];
        in.read(header, 44);
        REQUIRE(in.good());

        // Verify RIFF marker
        REQUIRE(std::memcmp(header, "RIFF", 4) == 0);
        REQUIRE(std::memcmp(header + 8, "WAVE", 4) == 0);
        REQUIRE(std::memcmp(header + 12, "fmt ", 4) == 0);

        // Verify format: PCM = 1
        uint16_t audio_format;
        std::memcpy(&audio_format, header + 20, 2);
        REQUIRE(audio_format == 1); // PCM

        // Verify channels
        uint16_t channels;
        std::memcpy(&channels, header + 22, 2);
        REQUIRE(channels == kChannels);

        // Verify sample rate
        uint32_t sample_rate;
        std::memcpy(&sample_rate, header + 24, 4);
        REQUIRE(sample_rate == kSampleRate);

        // Verify bit depth
        uint16_t bit_depth;
        std::memcpy(&bit_depth, header + 34, 2);
        REQUIRE(bit_depth == 16);

        // Verify data chunk
        REQUIRE(std::memcmp(header + 36, "data", 4) == 0);

        in.close();
        std::remove(test_path.c_str());
    }

    SECTION("24-bit WAV roundtrip") {
        REQUIRE(FileWriter::write_wav(test_path, data.data(), kChannels, kFrames,
                                       kSampleRate, 24, DitherType::Triangular));

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char header[44];
        in.read(header, 44);

        uint16_t bit_depth;
        std::memcpy(&bit_depth, header + 34, 2);
        REQUIRE(bit_depth == 24);

        in.close();
        std::remove(test_path.c_str());
    }

    SECTION("32-bit float WAV") {
        REQUIRE(FileWriter::write_wav(test_path, data.data(), kChannels, kFrames,
                                       kSampleRate, 0, DitherType::None)); // 0 = float

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char header[44];
        in.read(header, 44);

        uint16_t audio_format;
        std::memcpy(&audio_format, header + 20, 2);
        REQUIRE(audio_format == 3); // IEEE float

        uint16_t bit_depth;
        std::memcpy(&bit_depth, header + 34, 2);
        REQUIRE(bit_depth == 32);

        in.close();
        std::remove(test_path.c_str());
    }

    SECTION("WAV with dither produces valid file") {
        REQUIRE(FileWriter::write_wav(test_path, data.data(), kChannels, kFrames,
                                       kSampleRate, 16, DitherType::Triangular));

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char header[44];
        in.read(header, 44);
        REQUIRE(std::memcmp(header, "RIFF", 4) == 0);

        in.close();
        std::remove(test_path.c_str());
    }

    SECTION("WAV with shaped dither") {
        REQUIRE(FileWriter::write_wav(test_path, data.data(), kChannels, kFrames,
                                       kSampleRate, 24, DitherType::Shaped));

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char header[44];
        in.read(header, 44);
        REQUIRE(std::memcmp(header, "RIFF", 4) == 0);

        in.close();
        std::remove(test_path.c_str());
    }

    SECTION("WAV error handling — null data") {
        REQUIRE_FALSE(FileWriter::write_wav(test_path, nullptr, 2, kFrames,
                                             kSampleRate, 16, DitherType::None));
    }

    SECTION("WAV error handling — zero channels") {
        REQUIRE_FALSE(FileWriter::write_wav(test_path, data.data(), 0, kFrames,
                                             kSampleRate, 16, DitherType::None));
    }
}

// ═══════════════════════════════════════════════════════════════════
// AIFF Writer Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("FileWriter::write_aiff produces valid AIFF-C file", "[audio][export][aiff]") {
    constexpr uint32_t kFrames = 128;
    constexpr uint32_t kChannels = 2;
    constexpr uint32_t kSampleRate = 48000;

    std::vector<float> data(kFrames * kChannels, 0.5f);
    const std::string test_path = "test_output.aiff";

    SECTION("16-bit AIFF") {
        REQUIRE(FileWriter::write_aiff(test_path, data.data(), kChannels, kFrames,
                                        kSampleRate, 16, DitherType::None));

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char header[12];
        in.read(header, 12);
        REQUIRE(std::memcmp(header, "FORM", 4) == 0);
        REQUIRE(std::memcmp(header + 8, "AIFC", 4) == 0);

        in.close();
        std::remove(test_path.c_str());
    }

    SECTION("24-bit AIFF") {
        REQUIRE(FileWriter::write_aiff(test_path, data.data(), kChannels, kFrames,
                                        kSampleRate, 24, DitherType::Triangular));

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char header[12];
        in.read(header, 12);
        REQUIRE(std::memcmp(header, "FORM", 4) == 0);

        in.close();
        std::remove(test_path.c_str());
    }

    // Cleanup
    std::remove(test_path.c_str());
}

// ═══════════════════════════════════════════════════════════════════
// FLAC Writer Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("FileWriter::write_flac produces valid FLAC file", "[audio][export][flac]") {
    constexpr uint32_t kFrames = 256;
    constexpr uint32_t kChannels = 2;
    constexpr uint32_t kSampleRate = 44100;

    std::vector<float> data(kFrames * kChannels, 0.25f);
    const std::string test_path = "test_output.flac";

    SECTION("16-bit FLAC") {
        REQUIRE(FileWriter::write_flac(test_path, data.data(), kChannels, kFrames,
                                        kSampleRate, 16, DitherType::None));

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char marker[4];
        in.read(marker, 4);
        REQUIRE(std::memcmp(marker, "fLaC", 4) == 0);

        // STREAMINFO block
        char streaminfo[42];
        in.read(streaminfo, 42);
        REQUIRE((streaminfo[0] & 0x80) != 0); // last metadata block
        REQUIRE((streaminfo[0] & 0x7F) == 0); // type = STREAMINFO

        in.close();
        std::remove(test_path.c_str());
    }

    SECTION("24-bit FLAC") {
        REQUIRE(FileWriter::write_flac(test_path, data.data(), kChannels, kFrames,
                                        kSampleRate, 24, DitherType::None));

        std::ifstream in(test_path, std::ios::binary);
        REQUIRE(in.good());

        char marker[4];
        in.read(marker, 4);
        REQUIRE(std::memcmp(marker, "fLaC", 4) == 0);

        in.close();
        std::remove(test_path.c_str());
    }

    std::remove(test_path.c_str());
}

// ═══════════════════════════════════════════════════════════════════
// MP3 / OGG Placeholder Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("MP3 and OGG placeholders return false", "[audio][export][placeholder]") {
    std::vector<float> data(64, 0.5f);

    REQUIRE_FALSE(FileWriter::write_mp3("test.mp3", data.data(), 2, 32,
                                        48000, 16, DitherType::None));

    REQUIRE_FALSE(FileWriter::write_ogg("test.ogg", data.data(), 2, 32,
                                        48000, 16, DitherType::None));
}
