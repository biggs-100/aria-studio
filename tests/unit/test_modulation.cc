// Tests for modulation: matrix connect/disconnect, stack ordering,
// nesting depth, cycle rejection, and bypass.
#include <catch2/catch_all.hpp>
#include <catch2/catch_approx.hpp>

#include "automation/automation_engine.h"
#include "automation/modulation_matrix.h"
#include "automation/modulation_source.h"
#include "automation/modulation_types.h"
#include "automation/nested_modulation.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace aria::automation;

static constexpr double kApprox = 0.0001;
static constexpr ParameterID kTarget1 = 1001;
static constexpr ParameterID kTarget2 = 1002;
static constexpr ParameterID kTarget3 = 1003;
static constexpr SourceID kSource1 = 1;
static constexpr SourceID kSource2 = 2;
static constexpr SourceID kSource3 = 3;

// A trivial test source that returns a fixed value.
class TestSource : public ModulationSource {
public:
    explicit TestSource(SourceID id, double value = 0.5)
        : value_(value) { set_id(id); }

    void set_fixed_value(double v) { value_ = v; }

    double evaluate(uint64_t /*ppqn*/,
                    const ModulationContext& /*ctx*/) const override {
        return value_;
    }

private:
    double value_ = 0.5;
};

// ============================================================
// ModulationMatrix: connect / disconnect
// ============================================================

TEST_CASE("ModulationMatrix: basic connect",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    bool ok = matrix.connect(kSource1, kTarget1, 0.8f,
                              ModulationPolarity::Unipolar);
    REQUIRE(ok);

    REQUIRE(matrix.total_connections() == 1);
    REQUIRE(matrix.connection_count(kTarget1) == 1);
    REQUIRE(matrix.has_connection(kSource1, kTarget1));
}

TEST_CASE("ModulationMatrix: disconnect removes connection",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    matrix.connect(kSource1, kTarget1);
    REQUIRE(matrix.total_connections() == 1);

    bool removed = matrix.disconnect(kSource1, kTarget1);
    REQUIRE(removed);
    REQUIRE(matrix.total_connections() == 0);
    REQUIRE_FALSE(matrix.has_connection(kSource1, kTarget1));
}

TEST_CASE("ModulationMatrix: disconnect non-existent returns false",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;
    REQUIRE_FALSE(matrix.disconnect(kSource1, kTarget1));
}

TEST_CASE("ModulationMatrix: connect with invalid IDs returns false",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    REQUIRE_FALSE(matrix.connect(0, kTarget1));
    REQUIRE_FALSE(matrix.connect(kSource1, 0));
    REQUIRE_FALSE(matrix.connect(0, 0));
    REQUIRE(matrix.total_connections() == 0);
}

TEST_CASE("ModulationMatrix: clear_target removes all connections for target",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    matrix.connect(kSource1, kTarget1);
    matrix.connect(kSource2, kTarget1);
    matrix.connect(kSource1, kTarget2);
    REQUIRE(matrix.total_connections() == 3);

    matrix.clear_target(kTarget1);
    REQUIRE(matrix.total_connections() == 1);
    REQUIRE_FALSE(matrix.has_connection(kSource1, kTarget1));
    REQUIRE_FALSE(matrix.has_connection(kSource2, kTarget1));
    REQUIRE(matrix.has_connection(kSource1, kTarget2));
}

TEST_CASE("ModulationMatrix: clear_source removes all connections from source",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    matrix.connect(kSource1, kTarget1);
    matrix.connect(kSource1, kTarget2);
    matrix.connect(kSource2, kTarget1);
    REQUIRE(matrix.total_connections() == 3);

    matrix.clear_source(kSource1);
    REQUIRE(matrix.total_connections() == 1);
    REQUIRE_FALSE(matrix.has_connection(kSource1, kTarget1));
    REQUIRE_FALSE(matrix.has_connection(kSource1, kTarget2));
    REQUIRE(matrix.has_connection(kSource2, kTarget1));
}

TEST_CASE("ModulationMatrix: clear_all removes everything",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    matrix.connect(kSource1, kTarget1);
    matrix.connect(kSource2, kTarget2);
    matrix.connect(kSource3, kTarget3);
    REQUIRE(matrix.total_connections() == 3);

    matrix.clear_all();
    REQUIRE(matrix.total_connections() == 0);
    REQUIRE_FALSE(matrix.has_connection(kSource1, kTarget1));
}

TEST_CASE("ModulationMatrix: connections_for returns the entries",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    matrix.connect(kSource1, kTarget1, 0.5f, ModulationPolarity::Bipolar);

    auto entries = matrix.connections_for(kTarget1);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].source_id == kSource1);
    REQUIRE(entries[0].amount == Catch::Approx(0.5f).margin(0.001f));
    REQUIRE(entries[0].polarity == ModulationPolarity::Bipolar);
}

TEST_CASE("ModulationMatrix: connections_for missing target returns empty",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;
    REQUIRE(matrix.connections_for(99999).empty());
}

// ============================================================
// ModulationMatrix: bypass
// ============================================================

TEST_CASE("ModulationMatrix: bypass toggles correctly",
          "[automation][modulation][matrix]") {
    ModulationMatrix matrix;

    matrix.connect(kSource1, kTarget1, 1.0f, ModulationPolarity::Unipolar);
    REQUIRE_FALSE(matrix.is_bypassed(kSource1, kTarget1));

    matrix.set_bypass(kSource1, kTarget1, true);
    REQUIRE(matrix.is_bypassed(kSource1, kTarget1));

    matrix.set_bypass(kSource1, kTarget1, false);
    REQUIRE_FALSE(matrix.is_bypassed(kSource1, kTarget1));
}

// ============================================================
// ModulationMatrix: multiple connections per target
// ============================================================

TEST_CASE("ModulationMatrix: multiple sources on same target",
          "[automation][modulation][stack]") {
    ModulationMatrix matrix;

    matrix.connect(kSource1, kTarget1, 0.5f, ModulationPolarity::Unipolar);
    matrix.connect(kSource2, kTarget1, 0.3f, ModulationPolarity::Bipolar);
    matrix.connect(kSource3, kTarget1, 0.2f, ModulationPolarity::Unipolar);

    REQUIRE(matrix.connection_count(kTarget1) == 3);
    REQUIRE(matrix.total_connections() == 3);
}

// ============================================================
// ModulationMatrix: cycle detection
// ============================================================

TEST_CASE("ModulationMatrix: no false positive cycle detection",
          "[automation][modulation][cycle]") {
    ModulationMatrix matrix;

    // Source3 → Target3 (clean)
    bool ok = matrix.connect(kSource3, kTarget3);
    REQUIRE(ok);

    // Source1 → Target2 (should work, no cycle)
    ok = matrix.connect(kSource1, kTarget2);
    REQUIRE(ok);
}

TEST_CASE("ModulationMatrix: rejects actual indirect cycles",
          "[automation][modulation][cycle]") {
    ModulationMatrix matrix;

    // Source1 → Target1
    matrix.connect(kSource1, kTarget1);
    // Source2 → Target2
    matrix.connect(kSource2, kTarget2);
    // Source1 → Target2 creates S1→T1, S1→T2, S2→T2
    // Trying S2→T1: S2→T2→(S1 modulates T2)→T1→(S1 modulates T1)... loop!
    matrix.connect(kSource1, kTarget2);

    // S2 → T1 would create: S2 → T1 → S1 → T2 → S2 (cycle!)
    bool ok = matrix.connect(kSource2, kTarget1);
    REQUIRE_FALSE(ok);
}

TEST_CASE("ModulationMatrix: no cycle when sources are independent",
          "[automation][modulation][cycle]") {
    ModulationMatrix matrix;

    // S1 → T1
    matrix.connect(kSource1, kTarget1);
    // S2 → T2 (independent path)
    matrix.connect(kSource2, kTarget2);

    // S2 → T1: S2 doesn't modulate any target that S1 also modulates
    // S1→T1 only. No path from S1 back to S2.
    bool ok = matrix.connect(kSource2, kTarget1);
    REQUIRE(ok);
}

// ============================================================
// ModulationStack: ordering and evaluation
// ============================================================

TEST_CASE("ModulationStack: add entries and evaluate",
          "[automation][modulation][stack]") {
    ModulationStack stack;
    REQUIRE(stack.empty());
    REQUIRE(stack.size() == 0);

    TestSource src1(10, 0.3);
    TestSource src2(11, 0.7);

    stack.add_entry(&src1, 0.5f, ModulationPolarity::Unipolar);
    REQUIRE(stack.size() == 1);

    stack.add_entry(&src2, 0.3f, ModulationPolarity::Bipolar);
    REQUIRE(stack.size() == 2);
}

TEST_CASE("ModulationStack: remove entry by index",
          "[automation][modulation][stack]") {
    ModulationStack stack;
    TestSource src(10, 0.5);

    stack.add_entry(&src, 0.5f);
    stack.add_entry(&src, 1.0f);
    REQUIRE(stack.size() == 2);

    stack.remove_entry(0);
    REQUIRE(stack.size() == 1);
}

TEST_CASE("ModulationStack: clear removes all entries",
          "[automation][modulation][stack]") {
    ModulationStack stack;
    TestSource src(10, 0.5);

    stack.add_entry(&src, 0.5f);
    stack.add_entry(&src, 0.8f);
    stack.add_entry(&src, 0.3f);
    REQUIRE(stack.size() == 3);

    stack.clear();
    REQUIRE(stack.empty());
}

TEST_CASE("ModulationStack: set_bypass and set_amount",
          "[automation][modulation][stack]") {
    ModulationStack stack;
    TestSource src(10, 0.5);

    stack.add_entry(&src, 0.5f, ModulationPolarity::Unipolar);
    REQUIRE_FALSE(stack.entry(0).bypassed);
    REQUIRE(stack.entry(0).amount == Catch::Approx(0.5f).margin(0.001f));

    stack.set_bypass(0, true);
    REQUIRE(stack.entry(0).bypassed);

    stack.set_amount(0, 1.0f);
    REQUIRE(stack.entry(0).amount == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("ModulationStack: evaluate with multiple sources",
          "[automation][modulation][stack]") {
    ModulationStack stack;
    TestSource src1(10, 0.3);
    TestSource src2(11, 0.7);

    stack.add_entry(&src1, 0.5f, ModulationPolarity::Unipolar);
    stack.add_entry(&src2, 0.3f, ModulationPolarity::Unipolar);

    ModulationContext ctx{};
    double result = stack.evaluate(0.5, 0, ctx);
    // result = base + src1*amt1 + src2*amt2 = 0.5 + 0.3*0.5 + 0.7*0.3 = 0.86
    REQUIRE(result == Catch::Approx(0.86).margin(0.01));
}

TEST_CASE("ModulationStack: evaluate clamped to [0, 1]",
          "[automation][modulation][stack]") {
    ModulationStack stack;
    TestSource src(10, 0.5);

    stack.add_entry(&src, 1.5f, ModulationPolarity::Unipolar);

    ModulationContext ctx{};
    double result = stack.evaluate(0.8, 0, ctx);
    REQUIRE(result >= 0.0);
    REQUIRE(result <= 1.0);
}

// ============================================================
// NestedModulation: depth enforcement and chain setup
// ============================================================

TEST_CASE("NestedModulation: empty chain evaluates to 0",
          "[automation][modulation][nested]") {
    NestedModulation nm;
    REQUIRE(nm.depth() == 0);
    REQUIRE(nm.target() == 0);

    ModulationContext ctx{};
    double result = nm.evaluate(0, ctx);
    REQUIRE(result == Catch::Approx(0.0).margin(kApprox));
}

TEST_CASE("NestedModulation: set_chain with valid depth succeeds",
          "[automation][modulation][nested]") {
    NestedModulation nm;
    TestSource src1(10, 0.5);
    TestSource src2(11, 0.7);

    std::vector<NestedModulation::Link> chain;
    chain.push_back({&src1, 0.8f, ModulationPolarity::Unipolar, 0});
    chain.push_back({&src2, 0.5f, ModulationPolarity::Bipolar, 0});

    bool ok = nm.set_chain(kTarget1, chain);
    REQUIRE(ok);
    REQUIRE(nm.depth() == 2);
    REQUIRE(nm.target() == kTarget1);
}

TEST_CASE("NestedModulation: rejects chain exceeding max depth",
          "[automation][modulation][nested]") {
    NestedModulation nm;
    TestSource src(10, 0.5);

    std::vector<NestedModulation::Link> chain;
    for (size_t i = 0; i < NestedModulation::kMaxDepth + 1; ++i) {
        chain.push_back({&src, 1.0f, ModulationPolarity::Unipolar, 0});
    }

    bool ok = nm.set_chain(kTarget1, chain);
    REQUIRE_FALSE(ok);
    REQUIRE(nm.depth() == 0);
}

TEST_CASE("NestedModulation: clear_chain resets state",
          "[automation][modulation][nested]") {
    NestedModulation nm;
    TestSource src(10, 0.5);

    std::vector<NestedModulation::Link> chain;
    chain.push_back({&src, 0.5f, ModulationPolarity::Unipolar, 0});

    nm.set_chain(kTarget1, chain);
    REQUIRE(nm.depth() == 1);

    nm.clear_chain();
    REQUIRE(nm.depth() == 0);
    REQUIRE(nm.target() == 0);
}

// ============================================================
// AutomationEngine: integration with matrix and sources
// ============================================================

TEST_CASE("AutomationEngine: create and retrieve clip",
          "[automation][modulation][engine]") {
    aria::AutomationEngine engine;
    engine.init();

    auto* clip = engine.create_clip(42);
    REQUIRE(clip != nullptr);
    REQUIRE(clip->id() == 42);

    auto* found = engine.get_clip(42);
    REQUIRE(found == clip);

    engine.destroy_clip(42);
    REQUIRE(engine.get_clip(42) == nullptr);

    engine.shutdown();
}

TEST_CASE("AutomationEngine: register and retrieve modulation source",
          "[automation][modulation][engine]") {
    aria::AutomationEngine engine;
    engine.init();

    auto source = std::make_unique<TestSource>(42, 0.75);
    source->set_name("Test LFO");
    engine.register_source(std::move(source));

    auto* found = engine.get_source(42);
    REQUIRE(found != nullptr);
    REQUIRE(found->name() == "Test LFO");

    engine.shutdown();
}

TEST_CASE("AutomationEngine: create_lane and get_lane round-trip",
          "[automation][modulation][engine]") {
    aria::AutomationEngine engine;
    engine.init();

    auto* lane = engine.create_lane(kTarget1);
    REQUIRE(lane != nullptr);
    REQUIRE(lane->target() == kTarget1);

    auto* found = engine.get_lane(kTarget1);
    REQUIRE(found == lane);

    auto* same = engine.create_lane(kTarget1);
    REQUIRE(same == lane);

    engine.shutdown();
}

TEST_CASE("AutomationEngine: matrix accessor returns valid reference",
          "[automation][modulation][engine]") {
    aria::AutomationEngine engine;
    engine.init();

    auto& matrix = engine.matrix();
    REQUIRE(matrix.total_connections() == 0);

    matrix.connect(kSource1, kTarget1);
    REQUIRE(matrix.total_connections() == 1);

    const auto& const_matrix = static_cast<const aria::AutomationEngine&>(engine).matrix();
    REQUIRE(const_matrix.total_connections() == 1);

    engine.shutdown();
}

TEST_CASE("AutomationEngine: cache accessor returns valid reference",
          "[automation][modulation][engine]") {
    aria::AutomationEngine engine;
    engine.init();

    auto& cache = engine.cache();
    cache.update_value(kTarget1, 0.75);
    cache.swap_buffers();
    REQUIRE(cache.read_value(kTarget1) == Catch::Approx(0.75).margin(kApprox));

    const auto& const_cache = static_cast<const aria::AutomationEngine&>(engine).cache();
    REQUIRE(const_cache.read_value(kTarget1) == Catch::Approx(0.75).margin(kApprox));

    engine.shutdown();
}

TEST_CASE("AutomationEngine: process_audio_thread updates cache from lanes",
          "[automation][modulation][engine]") {
    aria::AutomationEngine engine;
    engine.init();

    auto* lane = engine.create_lane(kTarget1);
    REQUIRE(lane != nullptr);
    lane->set_bypassed(false);

    auto clip = std::make_shared<AutomationClip>();
    clip->add_point({0, 0.0, InterpolationType::Linear, {}});
    clip->add_point({960, 1.0, InterpolationType::Linear, {}});
    lane->bind_clip(clip);

    engine.process_audio_thread(0, 960);

    double cached = engine.cache().read_value(kTarget1);
    REQUIRE(cached == Catch::Approx(0.5).margin(kApprox));

    engine.shutdown();
}

TEST_CASE("AutomationEngine: bypassed lanes don't update cache",
          "[automation][modulation][engine]") {
    aria::AutomationEngine engine;
    engine.init();

    auto* lane = engine.create_lane(kTarget1);
    lane->set_bypassed(true);

    auto clip = std::make_shared<AutomationClip>();
    clip->add_point({0, 0.5, InterpolationType::Linear, {}});
    lane->bind_clip(clip);

    engine.process_audio_thread(0, 960);

    double cached = engine.cache().read_value(kTarget1);
    REQUIRE(cached == Catch::Approx(0.0).margin(kApprox));

    engine.shutdown();
}
