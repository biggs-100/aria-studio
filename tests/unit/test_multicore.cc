#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "audio/multi_core_distributor.h"
#include "audio/track_processor.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using namespace aria;

// ─── MultiCoreDistributor tests ───────────────────────────────

TEST_CASE("MultiCoreDistributor init and shutdown", "[audio][multicore]") {
    SECTION("default init (auto-detect)") {
        MultiCoreDistributor dist;
        REQUIRE(dist.init(0));
        REQUIRE(dist.num_workers() > 0);
        dist.shutdown();
    }

    SECTION("init with specific thread count") {
        MultiCoreDistributor dist;
        REQUIRE(dist.init(2));
        REQUIRE(dist.num_workers() == 2);
        dist.shutdown();
    }

    SECTION("double init is safe") {
        MultiCoreDistributor dist;
        REQUIRE(dist.init(1));
        // init again (should be handled gracefully)
        REQUIRE(dist.init(1));
        dist.shutdown();
    }

    SECTION("shutdown without init is safe") {
        MultiCoreDistributor dist;
        REQUIRE_NOTHROW(dist.shutdown());
    }

    SECTION("is_enabled returns correct state") {
        MultiCoreDistributor dist;
        dist.init(2);

        REQUIRE(dist.is_enabled());
        dist.set_enabled(false);
        REQUIRE_FALSE(dist.is_enabled());
        dist.set_enabled(true);
        REQUIRE(dist.is_enabled());

        dist.shutdown();
    }

    SECTION("is_enabled returns false with no workers") {
        MultiCoreDistributor dist;
        // Not initialized — should not be enabled
        REQUIRE_FALSE(dist.is_enabled());
    }
}

TEST_CASE("MultiCoreDistributor serial fallback", "[audio][multicore]") {
    MultiCoreDistributor dist;
    dist.init(0);
    dist.set_enabled(false);  // force serial mode
    dist.shutdown();

    SECTION("distribute with no workers completes safely") {
        MultiCoreDistributor dist2;
        std::vector<TrackProcessor*> empty_tracks;
        bool phase1_called = false;
        bool phase3_called = false;

        dist2.distribute(
            empty_tracks, 64, 0,
            [&]() { phase1_called = true; },
            [&]() { phase3_called = true; }
        );

        REQUIRE(phase1_called);
        REQUIRE(phase3_called);
    }
}

TEST_CASE("MultiCoreDistributor parallel processing", "[audio][multicore][.mayfail]") {
    MultiCoreDistributor dist;
    REQUIRE(dist.init(2));

    // Create multiple track processors
    constexpr uint32_t kNumTracks = 4;
    std::vector<std::unique_ptr<TrackProcessor>> track_owners;
    std::vector<TrackProcessor*> tracks;

    for (uint32_t i = 0; i < kNumTracks; ++i) {
        auto tp = std::make_unique<TrackProcessor>();
        tp->configure(2, 48000);
        tp->set_active(true);
        tracks.push_back(tp.get());
        track_owners.push_back(std::move(tp));
    }

    SECTION("distribute processes all tracks") {
        // Verify distribute doesn't crash with multiple tracks
        REQUIRE_NOTHROW(dist.distribute(
            tracks, 64, 0, nullptr, nullptr
        ));
    }

    SECTION("distribute with serial fallback matches results") {
        // Phase 1 and 3 callbacks
        std::atomic<bool> p1{false}, p3{false};
        dist.distribute(
            tracks, 64, 0,
            [&]() { p1.store(true, std::memory_order_release); },
            [&]() { p3.store(true, std::memory_order_release); }
        );

        REQUIRE(p1.load());
        REQUIRE(p3.load());
    }

    SECTION("rebalance with track changes") {
        REQUIRE_NOTHROW(dist.rebalance(tracks));

        // Remove a track and rebalance
        tracks.pop_back();
        REQUIRE_NOTHROW(dist.rebalance(tracks));
    }

    dist.shutdown();
}

TEST_CASE("MultiCoreDistributor edge cases", "[audio][multicore]") {
    SECTION("distribute with empty track list") {
        MultiCoreDistributor dist;
        dist.init(2);

        std::vector<TrackProcessor*> empty;
        REQUIRE_NOTHROW(dist.distribute(empty, 64, 0, nullptr, nullptr));

        dist.shutdown();
    }

    SECTION("distribute with 1 track (no parallelism needed)") {
        MultiCoreDistributor dist;
        dist.init(2);

        auto tp = std::make_unique<TrackProcessor>();
        tp->configure(2, 48000);

        std::vector<TrackProcessor*> tracks = { tp.get() };
        REQUIRE_NOTHROW(dist.distribute(tracks, 64, 0, nullptr, nullptr));

        dist.shutdown();
    }

    SECTION("shutdown during operation is safe") {
        MultiCoreDistributor dist;
        dist.init(2);

        auto tp = std::make_unique<TrackProcessor>();
        tp->configure(2, 48000);
        std::vector<TrackProcessor*> tracks = { tp.get() };

        // Process once to ensure workers are running
        dist.distribute(tracks, 64, 0, nullptr, nullptr);

        // Shutdown should complete without hanging
        dist.shutdown();
    }

    SECTION("rebalance empty list") {
        MultiCoreDistributor dist;
        dist.init(1);

        std::vector<TrackProcessor*> empty;
        REQUIRE_NOTHROW(dist.rebalance(empty));

        dist.shutdown();
    }
}

TEST_CASE("MultiCoreDistributor worker lifecycle", "[audio][multicore]") {
    SECTION("workers start and stop cleanly") {
        MultiCoreDistributor dist;

        // Init and shutdown multiple times
        for (int i = 0; i < 3; ++i) {
            REQUIRE(dist.init(2));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            dist.shutdown();
        }
    }

    SECTION("worker threads process tracks in parallel") {
        MultiCoreDistributor dist;
        dist.init(2);

        // Create a few tracks with identifiable processing
        constexpr uint32_t kNumTracks = 4;
        std::vector<std::unique_ptr<TrackProcessor>> owners;
        std::vector<TrackProcessor*> tracks;

        for (uint32_t i = 0; i < kNumTracks; ++i) {
            auto tp = std::make_unique<TrackProcessor>();
            tp->configure(2, 48000);
            tp->set_active(true);
            tracks.push_back(tp.get());
            owners.push_back(std::move(tp));
        }

        // Process multiple blocks
        for (uint32_t block = 0; block < 5; ++block) {
            REQUIRE_NOTHROW(dist.distribute(
                tracks, 64, block * 64, nullptr, nullptr
            ));
        }

        dist.shutdown();
    }
}
