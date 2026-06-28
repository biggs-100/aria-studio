#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "audio/pdc_manager.h"
#include "audio/audio_node.h"
#include "audio/audio_node_types.h"
#include "audio/track_processor.h"

#include <memory>
#include <cmath>
#include <vector>
#include <cstring>

using namespace aria;

// ─── Test node with configurable latency ──────────────────────

class LatencyTestNode : public AudioNode {
public:
    explicit LatencyTestNode(uint32_t latency_samples = 0)
        : latency_(latency_samples) {}

    void process(ProcessContext& ctx) override {
        // Pass-through: copy input to output
        uint32_t ch = std::min(ctx.input_count, ctx.output_count);
        for (uint32_t c = 0; c < ch; ++c) {
            if (ctx.inputs[c] && ctx.outputs[c]) {
                std::memcpy(ctx.outputs[c]->data[0],
                           ctx.inputs[c]->data[0],
                           ctx.frames * sizeof(float));
                ctx.outputs[c]->frames = ctx.frames;
            }
        }
    }

    uint32_t latency() const override { return latency_; }
    const char* label() const override { return "LatencyTest"; }

    void set_latency(uint32_t samples) { latency_ = samples; }

private:
    uint32_t latency_;
};

// ─── Test fixture ─────────────────────────────────────────────

struct PDCFixture {
    PDCManager pdc;

    // Helper to create a chain of test nodes
    std::vector<AudioNode*> create_chain(std::vector<uint32_t> latencies) {
        chain_nodes_.clear();
        chain_.clear();
        for (auto lat : latencies) {
            auto node = std::make_unique<LatencyTestNode>(lat);
            chain_.push_back(node.get());
            chain_nodes_.push_back(std::move(node));
        }
        return chain_;
    }

    // Clean up
    ~PDCFixture() {
        chain_nodes_.clear();
        chain_.clear();
    }

private:
    std::vector<std::unique_ptr<LatencyTestNode>> chain_nodes_;
    std::vector<AudioNode*> chain_;
};

// ─── PDCManager tests ─────────────────────────────────────────

TEST_CASE("PDCManager track latency calculation", "[audio][pdc]") {
    PDCFixture fx;

    SECTION("empty chain has zero delay") {
        auto chain = fx.create_chain({});
        REQUIRE(fx.pdc.chain_delay(chain) == 0);
    }

    SECTION("single node reports its latency") {
        auto chain = fx.create_chain({42});
        REQUIRE(fx.pdc.chain_delay(chain) == 42);
    }

    SECTION("chain delay is sum of node latencies") {
        auto chain = fx.create_chain({10, 20, 30});
        REQUIRE(fx.pdc.chain_delay(chain) == 60);
    }

    SECTION("chain delay is zero when all nodes have zero latency") {
        auto chain = fx.create_chain({0, 0, 0});
        REQUIRE(fx.pdc.chain_delay(chain) == 0);
    }

    SECTION("chain with 100 sample latency") {
        auto chain = fx.create_chain({100});
        REQUIRE(fx.pdc.chain_delay(chain) == 100);
    }
}

TEST_CASE("PDCManager recalculate and max delay", "[audio][pdc]") {
    PDCFixture fx;

    SECTION("recalculate sets max_delay") {
        auto chain = fx.create_chain({50});
        fx.pdc.recalculate(chain);
        REQUIRE(fx.pdc.max_delay() == 50);
    }

    SECTION("recalculate with zero latency") {
        auto chain = fx.create_chain({0});
        fx.pdc.recalculate(chain);
        REQUIRE_FALSE(fx.pdc.is_active());
        REQUIRE(fx.pdc.max_delay() == 0);
    }

    SECTION("recalculate with multiple nodes") {
        auto chain = fx.create_chain({10, 20, 30});
        fx.pdc.recalculate(chain);
        REQUIRE(fx.pdc.max_delay() == 60);
    }

    SECTION("recalculate updates max delay when chain grows") {
        auto chain_a = fx.create_chain({50});
        fx.pdc.recalculate(chain_a);
        REQUIRE(fx.pdc.max_delay() == 50);

        auto chain_b = fx.create_chain({100});
        fx.pdc.recalculate(chain_b);
        REQUIRE(fx.pdc.max_delay() >= 100);
    }

    SECTION("is_active returns correct value") {
        REQUIRE_FALSE(fx.pdc.is_active());

        auto chain = fx.create_chain({64});
        fx.pdc.recalculate(chain);
        REQUIRE(fx.pdc.is_active());
    }
}

TEST_CASE("PDCManager delay compensation", "[audio][pdc]") {
    constexpr uint32_t kFrames = 32;
    constexpr uint32_t kDelay = 16;

    SECTION("compensation delays audio by requested amount") {
        PDCManager pdc;

        // Create a buffer with an impulse at position 0
        AudioBuffer buf;
        float buf_data[kFrames];
        buf.frames = kFrames;
        buf.channels = 1;
        buf.data[0] = buf_data;

        std::memset(buf_data, 0, sizeof(buf_data));
        buf_data[0] = 1.0f;  // impulse at start

        // Apply compensation delay
        pdc.apply_compensation(buf, kDelay, 1);

        // The impulse should now be at position kDelay
        for (uint32_t i = 0; i < kFrames; ++i) {
            if (i == kDelay) {
                REQUIRE(buf_data[i] == Catch::Approx(1.0f));
            } else {
                REQUIRE(buf_data[i] == Catch::Approx(0.0f));
            }
        }
    }

    SECTION("zero delay is a no-op") {
        PDCManager pdc;

        AudioBuffer buf;
        float buf_data[kFrames];
        buf.frames = kFrames;
        buf.channels = 1;
        buf.data[0] = buf_data;

        for (uint32_t i = 0; i < kFrames; ++i) {
            buf_data[i] = static_cast<float>(i);
        }

        pdc.apply_compensation(buf, 0, 1);

        // Buffer unchanged
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(buf_data[i] == Catch::Approx(static_cast<float>(i)));
        }
    }

    SECTION("compensation preserves signal shape") {
        PDCManager pdc;

        AudioBuffer buf;
        float buf_data[kFrames];
        buf.frames = kFrames;
        buf.channels = 1;
        buf.data[0] = buf_data;

        // Create a sine wave
        for (uint32_t i = 0; i < kFrames; ++i) {
            buf_data[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);
        }

        // Apply delay
        pdc.apply_compensation(buf, 8, 2);

        // First 8 samples should be from the previous buffer (zeros initially)
        for (uint32_t i = 0; i < 8 && i < kFrames; ++i) {
            REQUIRE(buf_data[i] == Catch::Approx(0.0f));
        }

        // Remaining samples should match the original (shifted)
        for (uint32_t i = 8; i < kFrames; ++i) {
            float original = std::sin(2.0f * 3.14159f * 440.0f * (i - 8) / 48000.0f);
            REQUIRE(buf_data[i] == Catch::Approx(original).margin(0.001f));
        }
    }

    SECTION("multi-channel compensation") {
        PDCManager pdc;

        AudioBuffer buf;
        float buf_data_ch0[kFrames];
        float buf_data_ch1[kFrames];
        buf.frames = kFrames;
        buf.channels = 2;
        buf.data[0] = buf_data_ch0;
        buf.data[1] = buf_data_ch1;

        // Fill with impulses on both channels
        std::memset(buf_data_ch0, 0, sizeof(buf_data_ch0));
        std::memset(buf_data_ch1, 0, sizeof(buf_data_ch1));
        buf_data_ch0[0] = 1.0f;
        buf_data_ch1[0] = 0.5f;

        pdc.apply_compensation(buf, 8, 3);

        // Both channels should be delayed by 8 samples
        REQUIRE(buf_data_ch0[8] == Catch::Approx(1.0f));
        REQUIRE(buf_data_ch1[8] == Catch::Approx(0.5f));

        // Everything before position 8 should be 0
        for (uint32_t i = 0; i < 8; ++i) {
            REQUIRE(buf_data_ch0[i] == Catch::Approx(0.0f));
            REQUIRE(buf_data_ch1[i] == Catch::Approx(0.0f));
        }
    }
}

TEST_CASE("PDCManager reset", "[audio][pdc]") {
    PDCManager pdc;

    SECTION("reset clears delay buffers and state") {
        // First, use recalculate to set PDC state
        std::vector<std::unique_ptr<LatencyTestNode>> nodes;
        nodes.push_back(std::make_unique<LatencyTestNode>(8));
        std::vector<AudioNode*> chain = { nodes[0].get() };
        pdc.recalculate(chain);
        REQUIRE(pdc.is_active());
        REQUIRE(pdc.max_delay() == 8);

        // Apply compensation
        AudioBuffer buf;
        float buf_data[32];
        buf.frames = 32;
        buf.channels = 1;
        buf.data[0] = buf_data;
        buf_data[0] = 1.0f;

        pdc.apply_compensation(buf, 8, 1);

        pdc.reset();
        REQUIRE_FALSE(pdc.is_active());
        REQUIRE(pdc.max_delay() == 0);
    }

    SECTION("reset while no state is safe") {
        REQUIRE_NOTHROW(pdc.reset());
    }
}

TEST_CASE("PDC integration with TrackProcessor", "[audio][pdc][track]") {
    SECTION("track reports chain latency") {
        TrackProcessor tp;
        tp.configure(2, 48000);

        tp.add_plugin(std::make_unique<LatencyTestNode>(32));
        REQUIRE(tp.chain_latency() == 32);

        tp.add_plugin(std::make_unique<LatencyTestNode>(16));
        REQUIRE(tp.chain_latency() == 48);
    }

    SECTION("track with zero-latency plugins") {
        TrackProcessor tp;
        tp.configure(2, 48000);

        tp.add_plugin(std::make_unique<GainNode>());
        tp.add_plugin(std::make_unique<MeterNode>());

        REQUIRE(tp.chain_latency() == 0);
    }

    SECTION("track PDC manager accessible") {
        TrackProcessor tp;
        tp.configure(2, 48000);

        auto& pdc = tp.pdc();
        REQUIRE_FALSE(pdc.is_active());
    }
}
