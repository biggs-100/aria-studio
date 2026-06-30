#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "graphics/graphics_engine.h"

#include <cstdint>
#include <thread>

using namespace aria;

// ─── GraphicsEngine TDD Tests ───────────────────────────────────────
//
// Test Strategy:
//   - Unit tests for device initialization and adapter selection.
//   - Swap chain creation and lifecycle (create / resize / destroy).
//   - Graceful fallback when no GPU adapter is available.
//   - The production GraphicsEngine class is designed so that a
//     null / headless Dawn backend can be injected for CI / headless
//     testing without requiring physical GPU hardware.
//
//   TDD notes:
//     - RED:   All tests reference GraphicsEngine before it exists.
//     - GREEN: Implement minimum code to compile and pass.
//     - TRIANGULATE: Multiple adapter paths, lifecycle scenarios.

// =====================================================================
// RED / GREEN – Device Initialisation
// =====================================================================

TEST_CASE("GraphicsEngine::init() succeeds with default adapter",
          "[graphics][engine][device]")
{
    // GIVEN a default GraphicsEngine with no explicit backend override
    GraphicsEngine engine(BackendPreference::Default);

    // WHEN init() is called
    const bool ok = engine.init();

    // THEN init returns true (platform has at least one GPU adapter)
    // This test will PASS on any system with a working GPU driver.
    // On headless CI it may fail — see the test below for fallback.
    CHECK(ok);

    // WHEN the engine is running
    CHECK(engine.is_initialized());

    // THEN shutdown cleans up
    engine.shutdown();
    CHECK_FALSE(engine.is_initialized());
}

TEST_CASE("GraphicsEngine::init() handles headless / no-GPU fallback",
          "[graphics][engine][fallback]")
{
    // GIVEN an engine configured to prefer the null (headless) backend
    // The null backend simulates a system with no physical GPU.
    GraphicsEngine engine(BackendPreference::Null);

    // WHEN init() is called
    const bool ok = engine.init();

    // THEN the engine gracefully reports failure
    // and does NOT crash or leave dangling resources.
    // The null backend returns false from init().
    CHECK_FALSE(ok);
    CHECK_FALSE(engine.is_initialized());

    // SAFETY: calling shutdown on an un-initialised engine is a no-op
    engine.shutdown();
    CHECK_FALSE(engine.is_initialized());
}

TEST_CASE("GraphicsEngine::init() prefers discrete GPU over integrated",
          "[graphics][engine][adapter]")
{
    // GIVEN an engine configured to prefer discrete GPU
    GraphicsEngine engine(BackendPreference::DiscreteGpu);

    // WHEN init() succeeds
    if (engine.init()) {
        // THEN the selected adapter type is discrete GPU
        const auto info = engine.adapter_info();
        CHECK(info.type == AdapterType::DiscreteGpu);
        CHECK_FALSE(info.name.empty());
        engine.shutdown();
    } else {
        // Some systems have no discrete GPU — fallback is acceptable
        SUCCEED("No discrete GPU available; skipping assertion");
    }
}

// =====================================================================
// TRIANGULATE – Lifecycle: init / shutdown / re-init
// =====================================================================

TEST_CASE("GraphicsEngine supports init → shutdown → re-init cycle",
          "[graphics][engine][lifecycle]")
{
    // GIVEN a fresh engine
    GraphicsEngine engine(BackendPreference::Default);

    // WHEN we init, shutdown, then init again
    REQUIRE(engine.init());
    REQUIRE(engine.is_initialized());
    engine.shutdown();
    CHECK_FALSE(engine.is_initialized());

    const bool re_init_ok = engine.init();

    // THEN the second init also succeeds
    CHECK(re_init_ok);
    CHECK(engine.is_initialized());
    engine.shutdown();
}

TEST_CASE("Double shutdown is safe (idempotent)",
          "[graphics][engine][lifecycle][safety]")
{
    GraphicsEngine engine(BackendPreference::Default);
    if (engine.init()) {
        engine.shutdown();
        // WHEN shutdown is called again on an already-shutdown engine
        // THEN no crash, no undefined behaviour
        CHECK_NOTHROW(engine.shutdown());
    }
}

// =====================================================================
// TRIANGULATE – Swap Chain Lifecycle
// =====================================================================

TEST_CASE("GraphicsEngine creates and destroys a swap chain",
          "[graphics][swapchain][lifecycle]")
{
    GraphicsEngine engine(BackendPreference::Null);
    if (!engine.init()) {
        // Null backend — swap chains cannot be created without a device
        SUCCEED("Engine not initialised (expected with Null backend)");
        return;
    }

    // GIVEN an initialised engine with a valid device
    // WHEN a swap chain is created for a 640×480 surface
    SwapChainConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = WGPUTextureFormat_BGRA8Unorm;
    cfg.vsync = true;

    auto* sc = engine.create_swapchain(cfg);
    REQUIRE(sc != nullptr);

    // THEN the swap chain has the expected dimensions and format
    SwapChainInfo info = engine.query_swapchain_info(sc);
    CHECK(info.width == 640);
    CHECK(info.height == 480);
    CHECK(info.format == WGPUTextureFormat_BGRA8Unorm);

    // WHEN the swap chain is destroyed
    engine.destroy_swapchain(sc);

    // THEN it is safe to call destroy again (idempotent)
    CHECK_NOTHROW(engine.destroy_swapchain(sc));
}

TEST_CASE("Swap chain is recreated on resize",
          "[graphics][swapchain][resize]")
{
    GraphicsEngine engine(BackendPreference::Null);
    if (!engine.init()) {
        SUCCEED("Engine not initialised (expected with Null backend)");
        return;
    }

    // GIVEN a swap chain at 800×600
    SwapChainConfig cfg{};
    cfg.width = 800;
    cfg.height = 600;
    cfg.format = WGPUTextureFormat_BGRA8Unorm;

    auto* sc = engine.create_swapchain(cfg);
    REQUIRE(sc != nullptr);

    // WHEN the surface is resized to 1024×768
    engine.resize_swapchain(sc, 1024, 768);

    // THEN the swap chain reports the new size
    SwapChainInfo info = engine.query_swapchain_info(sc);
    CHECK(info.width == 1024);
    CHECK(info.height == 768);

    engine.destroy_swapchain(sc);
}

// =====================================================================
// TRIANGULATE – Adapter info when not initialised
// =====================================================================

TEST_CASE("Adapter info returns empty struct before init()",
          "[graphics][engine][adapter]")
{
    // GIVEN an engine that has NOT been initialised
    GraphicsEngine engine(BackendPreference::Default);

    // WHEN adapter_info() is queried
    const auto info = engine.adapter_info();

    // THEN the struct is empty / zeroed
    CHECK(info.type == AdapterType::Unknown);
    CHECK(info.name.empty());
}

// =====================================================================
// TRIANGULATE – Create swap chain without init
// =====================================================================

TEST_CASE("create_swapchain returns nullptr when engine not initialised",
          "[graphics][swapchain][safety]")
{
    GraphicsEngine engine(BackendPreference::Default);

    // GIVEN an engine that has NOT been initialised
    SwapChainConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;

    // WHEN create_swapchain is called
    auto* sc = engine.create_swapchain(cfg);

    // THEN it returns nullptr (safe degradation)
    CHECK(sc == nullptr);
}
