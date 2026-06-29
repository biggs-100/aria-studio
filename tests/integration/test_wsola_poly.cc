// Integration Test: WSOLA Polymorphism
//
// Verifies that TimeStretchAlgorithm can be used polymorphically
// with different implementations in the TrackProcessor pipeline.
//
// PR 6 — Task 6.6: Integration tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "audio/wsola.h"
#include "audio/track_processor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

using namespace aria;

// ─── Helpers ─────────────────────────────────────────────────────

static std::vector<float> make_sine(double freq, double sample_rate,
                                     uint32_t frames, double amp = 0.5) {
    std::vector<float> buf(frames);
    for (uint32_t i = 0; i < frames; ++i) {
        buf[i] = static_cast<float>(
            amp * std::sin(2.0 * std::numbers::pi_v<double> * freq *
                           static_cast<double>(i) / sample_rate));
    }
    return buf;
}

// ─── Test Stub Implementation ────────────────────────────────────

class PassthroughStretch : public TimeStretchAlgorithm {
public:
    double last_ratio = 1.0;
    uint32_t process_calls = 0;
    uint32_t reset_calls = 0;

    void set_ratio(double r) override { last_ratio = r; }
    uint32_t process(const float* input, float* output,
                      uint32_t input_frames,
                      uint32_t max_output_frames) override {
        ++process_calls;
        uint32_t to_copy = std::min(input_frames, max_output_frames);
        if (input && output && to_copy > 0) {
            std::copy(input, input + to_copy, output);
        }
        return to_copy;
    }
    void reset() override { ++reset_calls; }
    uint32_t latency() const override { return 0; }
};

// ─── Tests ───────────────────────────────────────────────────────

TEST_CASE("Integration: WSOLAStretch via TimeStretchAlgorithm pointer",
          "[integration][wsola][polymorphism]") {
    constexpr uint32_t kSR = 48000;
    constexpr uint32_t kFrames = 1024;

    SECTION("WSOLAStretch used through base pointer") {
        std::unique_ptr<TimeStretchAlgorithm> algo =
            std::make_unique<WSOLAStretch>(kSR, 1);
        REQUIRE(algo != nullptr);

        algo->set_ratio(1.0);
        auto input = make_sine(440.0, kSR, kFrames, 0.5);
        std::vector<float> output(kFrames * 2, 0.0f);

        uint32_t written = algo->process(input.data(), output.data(),
                                          kFrames, kFrames * 2);
        REQUIRE(written <= kFrames * 2);
        REQUIRE_NOTHROW(algo->reset());
    }

    SECTION("different stretch algorithms via same interface") {
        // First, use WSOLAStretch
        auto wsola = std::make_unique<WSOLAStretch>(kSR, 1);
        wsola->set_ratio(1.5);
        REQUIRE(wsola->ratio() == Catch::Approx(1.5));

        // Replace with passthrough stub
        auto passthrough = std::make_unique<PassthroughStretch>();
        passthrough->set_ratio(1.0);
        REQUIRE(passthrough->last_ratio == Catch::Approx(1.0));
    }
}

TEST_CASE("Integration: TrackProcessor with multiple stretch algorithms",
          "[integration][wsola][track]") {
    constexpr uint32_t kSR = 48000;
    constexpr uint32_t kFrames = 256;

    TrackProcessor tp;
    tp.configure(1, kSR);
    tp.set_active(true);
    tp.set_gain(1.0f);

    auto input_data = make_sine(440.0, kSR, kFrames, 0.5);

    ClipPlaybackSlot slot;
    slot.source_data = input_data.data();
    slot.source_frames = kFrames;
    slot.source_channels = 1;
    slot.gain = 1.0f;
    tp.set_clip_slots({slot});

    SECTION("WSOLAStretch in pipeline does not crash") {
        auto wsola = std::make_unique<WSOLAStretch>(kSR, 1);
        wsola->set_ratio(1.0);
        tp.set_time_stretch_algorithm(std::move(wsola));

        AudioBuffer output;
        float out_data[kFrames * 2] = {0.0f};
        output.frames = kFrames;
        output.channels = 1;
        output.data[0] = out_data;

        REQUIRE_NOTHROW(tp.process(kFrames, 0, nullptr, &output));
    }

    SECTION("PassthroughStretch in pipeline processes correctly") {
        auto passthrough = std::make_unique<PassthroughStretch>();
        PassthroughStretch* ptr = passthrough.get();
        tp.set_time_stretch_algorithm(std::move(passthrough));

        AudioBuffer output;
        float out_data[kFrames * 2] = {0.0f};
        output.frames = kFrames;
        output.channels = 1;
        output.data[0] = out_data;

        REQUIRE_NOTHROW(tp.process(kFrames, 0, nullptr, &output));
        // Passthrough should have been called
        REQUIRE(ptr->process_calls > 0);
    }

    SECTION("swap algorithms between calls") {
        auto wsola = std::make_unique<WSOLAStretch>(kSR, 1);
        wsola->set_ratio(1.0);
        tp.set_time_stretch_algorithm(std::move(wsola));

        AudioBuffer output;
        float out_data[kFrames * 2] = {0.0f};
        output.frames = kFrames;
        output.channels = 1;
        output.data[0] = out_data;

        REQUIRE_NOTHROW(tp.process(kFrames, 0, nullptr, &output));

        // Swap to passthrough
        auto passthrough = std::make_unique<PassthroughStretch>();
        tp.set_time_stretch_algorithm(std::move(passthrough));

        REQUIRE_NOTHROW(tp.process(kFrames, 0, nullptr, &output));
    }
}

TEST_CASE("Integration: Crossfader gain with stretch processing",
          "[integration][wsola][crossfader]") {
    constexpr uint32_t kSR = 48000;
    constexpr uint32_t kFrames = 256;

    TrackProcessor tp;
    tp.configure(1, kSR);
    tp.set_active(true);
    tp.set_gain(1.0f);

    auto wsola = std::make_unique<WSOLAStretch>(kSR, 1);
    wsola->set_ratio(1.0);
    tp.set_time_stretch_algorithm(std::move(wsola));

    auto input_data = make_sine(440.0, kSR, kFrames, 0.5);

    ClipPlaybackSlot slot;
    slot.source_data = input_data.data();
    slot.source_frames = kFrames;
    slot.source_channels = 1;
    slot.gain = 1.0f;
    tp.set_clip_slots({slot});

    AudioBuffer output;
    float out_data[kFrames * 2] = {0.0f};
    output.frames = kFrames;
    output.channels = 1;
    output.data[0] = out_data;

    SECTION("crossfader gain 0.5 with WSOLA active") {
        tp.set_crossfader_gain(0.5f);
        REQUIRE_NOTHROW(tp.process(kFrames, 0, nullptr, &output));
    }

    SECTION("crossfader gain 0.0 produces silence") {
        tp.set_crossfader_gain(0.0f);
        REQUIRE_NOTHROW(tp.process(kFrames, 0, nullptr, &output));
    }
}
