#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "audio/audio_graph.h"
#include "audio/audio_node.h"
#include "audio/audio_node_types.h"
#include "audio/buffer_pool.h"
#include "audio/simd_ops.h"

#include <memory>
#include <cmath>
#include <vector>

using namespace aria;

// ─── Test harness for graph processing ─────────────────────────

struct GraphTestFixture {
    LockFreeBufferPool pool;
    AudioGraph graph;
    static constexpr uint32_t kFrames = 64;
    static constexpr uint32_t kSampleRate = 48000;

    GraphTestFixture() {
        REQUIRE(pool.init(kFrames, 2, 256));
        graph.set_pool(&pool);
    }
};

// ─── AudioNode tests ──────────────────────────────────────────

TEST_CASE("GainNode parameter API", "[audio][node]") {
    GainNode node;

    SECTION("default gain is 1.0") {
        REQUIRE(node.gain() == Catch::Approx(1.0));
        REQUIRE(node.get_parameter(0) == Catch::Approx(1.0));
    }

    SECTION("set_parameter updates gain") {
        node.set_parameter(0, 0.5);
        REQUIRE(node.gain() == Catch::Approx(0.5));
    }

    SECTION("parameter count is 1") {
        REQUIRE(node.parameter_count() == 1);
    }
}

// ─── AudioGraph topology tests ─────────────────────────────────

TEST_CASE("AudioGraph add and remove nodes", "[audio][graph]") {
    GraphTestFixture fx;

    NodeID in_id = fx.graph.add_node(std::make_unique<AudioInputNode>(2));
    NodeID out_id = fx.graph.add_node(std::make_unique<AudioOutputNode>(2));

    SECTION("node IDs are valid") {
        REQUIRE(in_id != kInvalidNodeID);
        REQUIRE(out_id != kInvalidNodeID);
        REQUIRE(in_id != out_id);
    }

    SECTION("get_node returns correct type") {
        auto* in = fx.graph.get_node(in_id);
        auto* out = fx.graph.get_node(out_id);
        REQUIRE(in != nullptr);
        REQUIRE(out != nullptr);
        REQUIRE(in != out);
    }

    SECTION("remove_node") {
        REQUIRE(fx.graph.node_count() >= 2);
        fx.graph.remove_node(in_id);
        REQUIRE(fx.graph.get_node(in_id) == nullptr);
    }
}

TEST_CASE("AudioGraph cycle detection", "[audio][graph]") {
    GraphTestFixture fx;

    NodeID id_a = fx.graph.add_node(std::make_unique<GainNode>());
    NodeID id_b = fx.graph.add_node(std::make_unique<GainNode>());
    NodeID id_c = fx.graph.add_node(std::make_unique<GainNode>());

    SECTION("no cycle in simple chain") {
        fx.graph.connect(id_a, id_b);
        fx.graph.connect(id_b, id_c);
        REQUIRE_FALSE(fx.graph.would_create_cycle(id_a, id_b));
        REQUIRE_FALSE(fx.graph.would_create_cycle(id_b, id_c));
    }

    SECTION("direct cycle detected") {
        fx.graph.connect(id_a, id_b);
        REQUIRE(fx.graph.would_create_cycle(id_b, id_a));
    }

    SECTION("indirect cycle detected") {
        fx.graph.connect(id_a, id_b);
        fx.graph.connect(id_b, id_c);
        REQUIRE(fx.graph.would_create_cycle(id_c, id_a));
    }

    SECTION("self-connection is a cycle") {
        REQUIRE(fx.graph.would_create_cycle(id_a, id_a));
    }
}

TEST_CASE("AudioGraph connect and disconnect", "[audio][graph]") {
    GraphTestFixture fx;

    NodeID in_id = fx.graph.add_node(std::make_unique<AudioInputNode>(2));
    NodeID gain_id = fx.graph.add_node(std::make_unique<GainNode>());
    NodeID out_id = fx.graph.add_node(std::make_unique<AudioOutputNode>(2));

    SECTION("chain processes without error") {
        fx.graph.connect(in_id, gain_id, 0, 0);
        fx.graph.connect(gain_id, out_id, 0, 0);

        // Should not throw
        REQUIRE_NOTHROW(fx.graph.process(GraphTestFixture::kFrames, 0));
    }

    SECTION("disconnect removes edge") {
        fx.graph.connect(in_id, gain_id, 0, 0);
        fx.graph.connect(gain_id, out_id, 0, 0);
        fx.graph.disconnect(gain_id, out_id);

        REQUIRE_NOTHROW(fx.graph.process(GraphTestFixture::kFrames, 0));
    }

    SECTION("disconnect_all") {
        fx.graph.connect(in_id, gain_id, 0, 0);
        fx.graph.connect(gain_id, out_id, 0, 0);
        fx.graph.disconnect_all(gain_id);

        REQUIRE_NOTHROW(fx.graph.process(GraphTestFixture::kFrames, 0));
    }
}

TEST_CASE("AudioGraph process with silence", "[audio][graph]") {
    GraphTestFixture fx;

    NodeID sil_id = fx.graph.add_node(std::make_unique<SilenceNode>(2));
    NodeID meter_id = fx.graph.add_node(std::make_unique<MeterNode>());

    fx.graph.connect(sil_id, meter_id, 0, 0);

    SECTION("silence node produces zeros") {
        REQUIRE_NOTHROW(fx.graph.process(GraphTestFixture::kFrames, 0));

        auto* meter = static_cast<MeterNode*>(fx.graph.get_node(meter_id));
        REQUIRE(meter != nullptr);
        REQUIRE(meter->peak(0) == Catch::Approx(0.0f));
        REQUIRE(meter->rms(0) == Catch::Approx(0.0f));
    }
}

TEST_CASE("AudioGraph process with gain", "[audio][graph]") {
    GraphTestFixture fx;

    NodeID in_id = fx.graph.add_node(std::make_unique<AudioInputNode>(1));
    NodeID gain_id = fx.graph.add_node(std::make_unique<GainNode>());
    NodeID meter_id = fx.graph.add_node(std::make_unique<MeterNode>());

    fx.graph.connect(in_id, gain_id, 0, 0);
    fx.graph.connect(gain_id, meter_id, 0, 0);

    SECTION("gain=1 passes through silence (no input data)") {
        REQUIRE_NOTHROW(fx.graph.process(GraphTestFixture::kFrames, 0));

        auto* meter = static_cast<MeterNode*>(fx.graph.get_node(meter_id));
        REQUIRE(meter != nullptr);
        REQUIRE(meter->peak(0) == Catch::Approx(0.0f));
    }

    SECTION("gain=0.5 applied") {
        auto* gain = static_cast<GainNode*>(fx.graph.get_node(gain_id));
        REQUIRE(gain != nullptr);
        gain->set_gain(0.5);

        REQUIRE_NOTHROW(fx.graph.process(GraphTestFixture::kFrames, 0));

        auto* meter = static_cast<MeterNode*>(fx.graph.get_node(meter_id));
        REQUIRE(meter != nullptr);
        REQUIRE(meter->peak(0) == Catch::Approx(0.0f));
    }
}

// ─── AudioGraph process with complex topology ──────────────────

TEST_CASE("AudioGraph multiple parallel chains", "[audio][graph]") {
    GraphTestFixture fx;

    // Two parallel chains: Gain → Meter each, fed by shared Silence
    NodeID sil_id = fx.graph.add_node(std::make_unique<SilenceNode>(2));
    NodeID g1_id = fx.graph.add_node(std::make_unique<GainNode>());
    NodeID g2_id = fx.graph.add_node(std::make_unique<GainNode>());
    NodeID m1_id = fx.graph.add_node(std::make_unique<MeterNode>());
    NodeID m2_id = fx.graph.add_node(std::make_unique<MeterNode>());

    fx.graph.connect(sil_id, g1_id, 0, 0);
    fx.graph.connect(g1_id, m1_id, 0, 0);
    fx.graph.connect(sil_id, g2_id, 0, 0);
    fx.graph.connect(g2_id, m2_id, 0, 0);

    SECTION("parallel chains process correctly") {
        REQUIRE_NOTHROW(fx.graph.process(GraphTestFixture::kFrames, 0));

        auto* meter1 = static_cast<MeterNode*>(fx.graph.get_node(m1_id));
        auto* meter2 = static_cast<MeterNode*>(fx.graph.get_node(m2_id));
        REQUIRE(meter1 != nullptr);
        REQUIRE(meter2 != nullptr);
        REQUIRE(meter1->peak(0) == Catch::Approx(0.0f));
        REQUIRE(meter2->peak(0) == Catch::Approx(0.0f));
    }
}

// ─── AtomicParameter tests ────────────────────────────────────

TEST_CASE("AtomicParameter thread-safe operations", "[audio][param]") {
    AtomicParameter param(1.0);

    SECTION("initial value") {
        REQUIRE(param.load() == Catch::Approx(1.0));
    }

    SECTION("store and load") {
        param.store(0.5);
        REQUIRE(param.load() == Catch::Approx(0.5));
    }

    SECTION("implicit conversion") {
        double val = param;
        REQUIRE(val == Catch::Approx(1.0));
    }

    SECTION("assignment") {
        param = 0.75;
        REQUIRE(static_cast<double>(param) == Catch::Approx(0.75));
    }

    SECTION("compare_exchange") {
        REQUIRE(param.compare_exchange(1.0, 2.0));
        REQUIRE(param.load() == Catch::Approx(2.0));
        REQUIRE_FALSE(param.compare_exchange(1.0, 3.0));  // expected doesn't match
        REQUIRE(param.load() == Catch::Approx(2.0));
    }
}
