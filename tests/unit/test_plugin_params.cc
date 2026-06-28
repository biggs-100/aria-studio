#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "plugin/audio_plugin.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace aria::plugin;

// ══════════════════════════════════════════════════════════════════════
//  Registration
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("ParameterManager starts empty", "[plugin][params]") {
    ParameterManager mgr;
    CHECK(mgr.count() == 0);
}

TEST_CASE("ParameterManager register_parameter returns sequential IDs",
          "[plugin][params]")
{
    ParameterManager mgr;

    ParameterInfo a;
    a.name = "Gain";
    a.min_value = 0.0;
    a.max_value = 1.0;
    a.default_value = 0.5;

    ParameterInfo b;
    b.name = "Pan";
    b.min_value = -1.0;
    b.max_value = 1.0;
    b.default_value = 0.0;

    ParamID id_a = mgr.register_parameter(a);
    ParamID id_b = mgr.register_parameter(b);

    CHECK(id_a == 0);
    CHECK(id_b == 1);
    CHECK(mgr.count() == 2);
}

TEST_CASE("ParameterManager stores and retrieves parameter info",
          "[plugin][params]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Frequency";
    info.min_value = 20.0;
    info.max_value = 20000.0;
    info.default_value = 1000.0;

    ParamID id = mgr.register_parameter(info);
    auto retrieved = mgr.parameter_info(id);

    CHECK(retrieved.name == "Frequency");
    CHECK(retrieved.min_value == 20.0);
    CHECK(retrieved.max_value == 20000.0);
}

// ══════════════════════════════════════════════════════════════════════
//  Get / Set
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("ParameterManager default value matches registered default",
          "[plugin][params]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    ParamID id = mgr.register_parameter(info);
    CHECK(mgr.get_value(id) == 0.5);
}

TEST_CASE("ParameterManager set_value clamps to valid range",
          "[plugin][params]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    ParamID id = mgr.register_parameter(info);

    // Set above max
    mgr.set_value(id, 2.0);
    CHECK(mgr.get_value(id) == 1.0);

    // Set below min
    mgr.set_value(id, -1.0);
    CHECK(mgr.get_value(id) == 0.0);
}

TEST_CASE("ParameterManager set_value within range works",
          "[plugin][params]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Pan";
    info.min_value = -1.0;
    info.max_value = 1.0;
    info.default_value = 0.0;

    ParamID id = mgr.register_parameter(info);
    mgr.set_value(id, 0.33);
    CHECK(mgr.get_value(id) == 0.33);
}

TEST_CASE("Unknown ParamID returns default values", "[plugin][params]") {
    ParameterManager mgr;
    CHECK(mgr.get_value(999) == 0.0);
    CHECK(mgr.parameter_info(999).name.empty());
}

// ══════════════════════════════════════════════════════════════════════
//  Begin/Edit/End (undo boundary)
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("ParameterManager begin_edit followed by perform_edit and end_edit",
          "[plugin][params][automation]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    ParamID id = mgr.register_parameter(info);

    // Start edit
    mgr.begin_edit(id);

    // Apply new value during edit
    mgr.perform_edit(id, 0.8);
    CHECK(mgr.get_value(id) == 0.8);

    // End edit — value persists
    mgr.end_edit(id);
    CHECK(mgr.get_value(id) == 0.8);
}

TEST_CASE("perform_edit without begin_edit does nothing",
          "[plugin][params][automation]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    ParamID id = mgr.register_parameter(info);

    // perform_edit without begin_edit — should not change value
    mgr.perform_edit(id, 0.9);
    CHECK(mgr.get_value(id) == 0.5);  // still default
}

TEST_CASE("perform_edit clamps during automation",
          "[plugin][params][automation]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    ParamID id = mgr.register_parameter(info);

    mgr.begin_edit(id);
    mgr.perform_edit(id, 5.0);  // above max
    CHECK(mgr.get_value(id) == 1.0);
    mgr.end_edit(id);
}

// ══════════════════════════════════════════════════════════════════════
//  Thread-safety — concurrent reads do not block
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Concurrent reads do not block during write",
          "[plugin][params][thread]")
{
    ParameterManager mgr;

    ParameterInfo info;
    info.name = "Gain";
    info.min_value = 0.0;
    info.max_value = 1.0;
    info.default_value = 0.5;

    ParamID id = mgr.register_parameter(info);

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> reads{0};

    // Reader thread — continuously reads
    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            mgr.get_value(id);
            mgr.parameter_info(id);
            reads.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Writer thread — continuously writes
    std::thread writer([&]() {
        for (int i = 0; i < 1000; ++i) {
            mgr.set_value(id, 0.1);
            mgr.begin_edit(id);
            mgr.perform_edit(id, 0.9);
            mgr.end_edit(id);
            std::this_thread::yield();
        }
    });

    writer.join();
    stop.store(true, std::memory_order_relaxed);
    reader.join();

    // At least some reads happened (verifying the reader didn't deadlock)
    CHECK(reads.load(std::memory_order_relaxed) > 0);
}

TEST_CASE("Multiple readers can access simultaneously",
          "[plugin][params][thread]")
{
    ParameterManager mgr;

    // Register several parameters
    for (int i = 0; i < 16; ++i) {
        ParameterInfo info;
        info.name = "Param" + std::to_string(i);
        info.min_value = 0.0;
        info.max_value = 1.0;
        info.default_value = static_cast<double>(i) / 16.0;
        mgr.register_parameter(info);
    }

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> reads_a{0};
    std::atomic<uint64_t> reads_b{0};
    std::atomic<uint64_t> reads_c{0};

    // Three concurrent readers
    auto reader_fn = [&](std::atomic<uint64_t>& counter) {
        while (!stop.load(std::memory_order_relaxed)) {
            for (ParamID i = 0; i < 16; ++i) {
                mgr.get_value(i);
                mgr.parameter_info(i);
            }
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::thread r1(reader_fn, std::ref(reads_a));
    std::thread r2(reader_fn, std::ref(reads_b));
    std::thread r3(reader_fn, std::ref(reads_c));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop.store(true, std::memory_order_relaxed);

    r1.join();
    r2.join();
    r3.join();

    // All readers made progress concurrently
    CHECK(reads_a.load() > 0);
    CHECK(reads_b.load() > 0);
    CHECK(reads_c.load() > 0);
}

// ══════════════════════════════════════════════════════════════════════
//  Multiple parameters — isolation
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Multiple parameters do not interfere", "[plugin][params]") {
    ParameterManager mgr;

    ParameterInfo a, b;
    a.name = "Volume";
    a.default_value = 0.8;
    b.name = "Pan";
    b.min_value = -1.0;
    b.max_value = 1.0;
    b.default_value = 0.0;

    ParamID vol_id = mgr.register_parameter(a);
    ParamID pan_id = mgr.register_parameter(b);

    mgr.set_value(vol_id, 0.5);
    CHECK(mgr.get_value(vol_id) == 0.5);
    CHECK(mgr.get_value(pan_id) == 0.0);  // unaffected

    mgr.set_value(pan_id, -0.5);
    CHECK(mgr.get_value(pan_id) == -0.5);
    CHECK(mgr.get_value(vol_id) == 0.5);  // unaffected
}
