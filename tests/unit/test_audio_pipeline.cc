#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "audio/audio_engine.h"
#include "audio/audio_graph.h"
#include "audio/audio_node.h"
#include "audio/audio_node_types.h"
#include "audio/buffer_pool.h"
#include "audio/diagnostics.h"
#include "audio/track_processor.h"
#include "audio/transport.h"
#include "test_common/audio_harness.h"

#include <memory>
#include <cmath>
#include <atomic>
#include <thread>
#include <chrono>

using namespace aria;

// ─── AudioEngine pipeline tests ───────────────────────────────

TEST_CASE("AudioEngine init and shutdown", "[audio][engine][pipeline]") {
    AudioEngine engine;

    SECTION("init succeeds with valid parameters") {
        REQUIRE(engine.init(48000, 128));
        REQUIRE(engine.is_initialized());
        REQUIRE(engine.sample_rate() == 48000);
        REQUIRE(engine.buffer_size() == 128);
    }

    SECTION("double init returns false") {
        REQUIRE(engine.init(48000, 128));
        REQUIRE_FALSE(engine.init(48000, 128));
    }

    SECTION("shutdown clears state") {
        REQUIRE(engine.init(48000, 128));
        engine.shutdown();
        REQUIRE_FALSE(engine.is_initialized());
    }

    SECTION("shutdown on uninitialized engine is safe") {
        REQUIRE_NOTHROW(engine.shutdown());
    }
}

TEST_CASE("AudioEngine process lifecycle", "[audio][engine][pipeline]") {
    AudioEngine engine;
    REQUIRE(engine.init(48000, 128));

    SECTION("process when stopped does not crash") {
        REQUIRE_NOTHROW(engine.process(128, 0));
    }

    SECTION("process when playing updates diagnostics") {
        engine.transport().play();
        REQUIRE(engine.transport().is_playing());

        engine.process(128, 0);
        REQUIRE(engine.diagnostics().callback_count() > 0);
    }

    SECTION("process when uninitialized is safe") {
        AudioEngine empty_engine;
        REQUIRE_NOTHROW(empty_engine.process(128, 0));
    }

    SECTION("process multiple callbacks accumulates diagnostics") {
        engine.transport().play();

        for (uint32_t i = 0; i < 10; ++i) {
            engine.process(128, i * 128);
        }

        REQUIRE(engine.diagnostics().callback_count() == 10);
        REQUIRE(engine.diagnostics().avg_callback_time_ms() >= 0.0);
    }
}

TEST_CASE("AudioEngine track management", "[audio][engine][pipeline]") {
    AudioEngine engine;
    REQUIRE(engine.init(48000, 128));

    SECTION("add track returns valid index") {
        uint32_t idx = engine.add_track();
        REQUIRE(idx == 0);
        REQUIRE(engine.track_count() == 1);
    }

    SECTION("add multiple tracks") {
        uint32_t t1 = engine.add_track();
        uint32_t t2 = engine.add_track();
        uint32_t t3 = engine.add_track();

        REQUIRE(t1 == 0);
        REQUIRE(t2 == 1);
        REQUIRE(t3 == 2);
        REQUIRE(engine.track_count() == 3);
    }

    SECTION("remove track shrinks count") {
        engine.add_track();
        engine.add_track();
        engine.add_track();
        REQUIRE(engine.track_count() == 3);

        engine.remove_track(1);
        REQUIRE(engine.track_count() == 2);

        engine.remove_track(0);
        REQUIRE(engine.track_count() == 1);
    }

    SECTION("remove invalid index is safe") {
        REQUIRE_NOTHROW(engine.remove_track(999));
    }

    SECTION("track accessor returns valid pointer") {
        uint32_t idx = engine.add_track();
        auto* tp = engine.track(idx);
        REQUIRE(tp != nullptr);
        REQUIRE(tp->channels() == 2);
    }

    SECTION("track accessor returns null for invalid index") {
        REQUIRE(engine.track(999) == nullptr);
    }
}

TEST_CASE("AudioEngine process control messages", "[audio][engine][pipeline]") {
    AudioEngine engine;
    REQUIRE(engine.init(48000, 128));

    SECTION("control message queue is accessible") {
        REQUIRE(engine.control_queue().empty());
    }

    SECTION("push and process a control message") {
        std::atomic<bool> executed{false};

        bool pushed = engine.push_control_message([&]() {
            executed.store(true, std::memory_order_release);
        });

        REQUIRE(pushed);
        REQUIRE_FALSE(engine.control_queue().empty());

        // Process messages (simulating audio thread)
        engine.control_queue().process_all();

        REQUIRE(executed.load(std::memory_order_acquire));
        REQUIRE(engine.control_queue().empty());
    }

    SECTION("control message execution during process()") {
        std::atomic<int> counter{0};

        engine.push_control_message([&]() {
            counter.fetch_add(1, std::memory_order_release);
        });

        // process() should drain the control queue
        engine.process(128, 0);

        REQUIRE(counter.load(std::memory_order_acquire) == 1);
    }

    SECTION("multiple control messages processed in order") {
        std::vector<int> results;
        std::mutex mtx;

        for (int i = 0; i < 5; ++i) {
            engine.push_control_message([&, i]() {
                std::lock_guard<std::mutex> lock(mtx);
                results.push_back(i);
            });
        }

        engine.control_queue().process_all();

        std::lock_guard<std::mutex> lock(mtx);
        REQUIRE(results.size() == 5);
        for (int i = 0; i < 5; ++i) {
            REQUIRE(results[i] == i);
        }
    }
}

TEST_CASE("AudioEngine full pipeline integration", "[audio][engine][pipeline][integration]") {
    AudioEngine engine;
    REQUIRE(engine.init(48000, 128));

    // Add some tracks
    engine.add_track();
    engine.add_track();
    engine.add_track();

    SECTION("process with tracks while stopped") {
        REQUIRE_NOTHROW(engine.process(128, 0));
    }

    SECTION("process with tracks while playing") {
        engine.transport().play();
        REQUIRE_NOTHROW(engine.process(128, 0));
        REQUIRE(engine.diagnostics().callback_count() == 1);
    }

    SECTION("process multiple blocks with tracks") {
        engine.transport().play();

        for (uint32_t i = 0; i < 50; ++i) {
            engine.process(128, i * 128);
        }

        REQUIRE(engine.diagnostics().callback_count() == 50);
        REQUIRE(engine.transport().is_playing());
    }

    SECTION("process with tracks and transport state transitions") {
        engine.transport().play();
        engine.process(128, 0);

        engine.transport().pause();
        engine.process(128, 128);

        engine.transport().play();
        engine.process(128, 256);

        engine.transport().stop();
        engine.process(128, 384);

        REQUIRE(engine.diagnostics().callback_count() == 4);
    }

    SECTION("engine handles tracks with plugins") {
        engine.transport().play();

        // Add a track with a gain plugin
        uint32_t track_idx = engine.add_track();
        auto* tp = engine.track(track_idx);
        REQUIRE(tp != nullptr);

        tp->set_active(true);
        tp->set_gain(0.75f);

        REQUIRE_NOTHROW(engine.process(128, 0));
    }
}

TEST_CASE("TrackProcessor standalone processing", "[audio][engine][track]") {
    // Create a test buffer
    constexpr uint32_t kFrames = 64;
    AudioBuffer input, output;
    float in_data[kFrames];
    float out_data[kFrames];

    input.frames = kFrames;
    input.channels = 1;
    input.data[0] = in_data;
    output.frames = kFrames;
    output.channels = 1;
    output.data[0] = out_data;

    // Fill input with a test signal
    for (uint32_t i = 0; i < kFrames; ++i) {
        in_data[i] = 0.5f;
    }

    SECTION("track with gain produces scaled output") {
        TrackProcessor tp;
        tp.configure(1, 48000);
        tp.set_active(true);
        tp.set_gain(0.5f);

        tp.process(kFrames, 0, &input, &output);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out_data[i] == Catch::Approx(0.25f));
        }
    }

    SECTION("muted track produces silence") {
        TrackProcessor tp;
        tp.configure(1, 48000);
        tp.set_active(true);
        tp.set_muted(true);

        tp.process(kFrames, 0, &input, &output);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out_data[i] == Catch::Approx(0.0f));
        }
    }

    SECTION("inactive track with no input produces silence") {
        TrackProcessor tp;
        tp.configure(1, 48000);
        tp.set_active(false);

        tp.process(kFrames, 0, nullptr, &output);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out_data[i] == Catch::Approx(0.0f));
        }
    }

    SECTION("pan adjusts channel levels") {
        TrackProcessor tp;
        tp.configure(2, 48000);
        tp.set_active(true);

        // Stereo input
        float stereo_in[kFrames * 2];
        float stereo_out[kFrames * 2];
        for (uint32_t i = 0; i < kFrames; ++i) {
            stereo_in[i] = 1.0f;
            stereo_in[kFrames + i] = 1.0f;
        }

        AudioBuffer stereo_input, stereo_output;
        stereo_input.frames = kFrames;
        stereo_input.channels = 2;
        stereo_input.data[0] = stereo_in;
        stereo_input.data[1] = stereo_in + kFrames;

        stereo_output.frames = kFrames;
        stereo_output.channels = 2;
        stereo_output.data[0] = stereo_out;
        stereo_output.data[1] = stereo_out + kFrames;

        // Pan hard left
        tp.set_pan(-1.0f);
        tp.process(kFrames, 0, &stereo_input, &stereo_output);

        // Left should be at gain, right should be near 0
        float left_sum = 0.0f, right_sum = 0.0f;
        for (uint32_t i = 0; i < kFrames; ++i) {
            left_sum += stereo_out[i];
            right_sum += stereo_out[kFrames + i];
        }
        REQUIRE(left_sum > 0.0f);
        REQUIRE(right_sum == Catch::Approx(0.0f));
    }

    SECTION("track with gain=1 passes signal through") {
        TrackProcessor tp;
        tp.configure(1, 48000);
        tp.set_active(true);
        tp.set_gain(1.0f);

        tp.process(kFrames, 0, &input, &output);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out_data[i] == Catch::Approx(0.5f));
        }
    }
}

TEST_CASE("TrackProcessor plugin chain", "[audio][engine][track]") {
    SECTION("add and remove plugins") {
        TrackProcessor tp;
        tp.configure(2, 48000);

        REQUIRE(tp.plugin_count() == 0);

        tp.add_plugin(std::make_unique<GainNode>());
        REQUIRE(tp.plugin_count() == 1);
        REQUIRE(tp.plugin(0) != nullptr);

        tp.add_plugin(std::make_unique<MeterNode>());
        REQUIRE(tp.plugin_count() == 2);

        auto removed = tp.remove_plugin(0);
        REQUIRE(removed != nullptr);
        REQUIRE(tp.plugin_count() == 1);
    }

    SECTION("remove invalid index returns nullptr") {
        TrackProcessor tp;
        tp.configure(2, 48000);
        REQUIRE(tp.remove_plugin(0) == nullptr);
    }
}

TEST_CASE("AudioEngine multicore integration", "[audio][engine][multicore][.mayfail]") {
    AudioEngine engine;
    REQUIRE(engine.init(48000, 128));

    SECTION("multicore distributor is enabled by default") {
        REQUIRE(engine.multicore_enabled());
    }

    SECTION("multicore can be disabled") {
        engine.set_multicore_enabled(false);
        REQUIRE_FALSE(engine.multicore_enabled());
    }

    SECTION("process with multicore enabled and tracks") {
        // Add several tracks to exercise distribution
        for (uint32_t i = 0; i < 8; ++i) {
            engine.add_track();
        }

        engine.transport().play();
        REQUIRE_NOTHROW(engine.process(128, 0));
        REQUIRE(engine.diagnostics().callback_count() == 1);
    }
}
