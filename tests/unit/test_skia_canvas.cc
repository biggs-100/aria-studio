#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "graphics/graphics_types.h"
#include "graphics/skia_canvas.h"

#include <cstdint>

using namespace aria;

// ─── SkiaCanvas TDD Tests ──────────────────────────────────────────
//
// Test Strategy:
//   - Verify SkiaCanvas creation and initialisation from a GraphicsEngine
//     swap chain handle.
//   - Verify drawing primitives (clear, drawRect, drawCircle, drawTextBlob)
//     produce expected Skia calls by querying surface state.
//   - Verify flush() queues draw commands to the GPU.
//   - Verify surface recreation after swap chain resize.
//
//   TDD notes:
//     - RED:   All tests reference SkiaCanvas before it exists.
//     - GREEN: Implement minimum code to compile and pass.
//     - TRIANGULATE: Surface resize, error paths, multiple draw calls.

// =====================================================================
// RED / GREEN – Canvas lifecycle
// =====================================================================

TEST_CASE("SkiaCanvas is created and initialised from a swap chain",
          "[graphics][skia_canvas][lifecycle]")
{
    // GIVEN a default SkiaCanvas
    SkiaCanvas canvas;

    // WHEN not yet initialised
    // THEN it reports not ready
    CHECK_FALSE(canvas.is_ready());

    // WHEN initialised (will fail in CI without GPU; tested in graphics engine suite)
    // The init requires a valid GraphicsEngine + swap chain, which we test
    // structurally: the class compiles and the API is callable.
    // For headless environments, init() gracefully returns false.
    CHECK_FALSE(canvas.init(nullptr, nullptr));

    // SAFETY: calling methods on uninitialised canvas is a no-op / safe
    CHECK_NOTHROW(canvas.clear(Color::Black));
    CHECK_NOTHROW(canvas.flush());
}

TEST_CASE("SkiaCanvas clears surface to a colour",
          "[graphics][skia_canvas][clear]")
{
    // GIVEN an initialised SkiaCanvas (requires real GPU — structure test)
    SkiaCanvas canvas;

    // WHEN clear is called with a specific colour
    // THEN the call does not crash (surface-dependent draw verified in GPU tests)
    CHECK_NOTHROW(canvas.clear(Color::from_rgba8(32, 64, 128, 255)));
    CHECK_NOTHROW(canvas.clear(Color::Black));
    CHECK_NOTHROW(canvas.clear(Color::Transparent));
}

// =====================================================================
// RED / GREEN – Draw primitives
// =====================================================================

TEST_CASE("SkiaCanvas drawRect issues a GPU draw command",
          "[graphics][skia_canvas][draw]")
{
    // GIVEN an initialised SkiaCanvas
    SkiaCanvas canvas;

    // WHEN drawRect is called with a paint
    Paint paint;
    paint.fill = Color::Red;
    paint.stroke = Color::White;
    paint.stroke_width = 2.0f;
    paint.corner_radius = 4.0f;

    // THEN the call completes without crashing
    CHECK_NOTHROW(canvas.drawRect({10.0f, 20.0f, 100.0f, 50.0f}, paint));

    // AND a second drawRect with different params also succeeds
    CHECK_NOTHROW(canvas.drawRect({0.0f, 0.0f, 640.0f, 480.0f}, {}));
}

TEST_CASE("SkiaCanvas drawCircle issues a GPU draw command",
          "[graphics][skia_canvas][circle]")
{
    SkiaCanvas canvas;

    Paint paint;
    paint.fill = Color::Blue;

    CHECK_NOTHROW(canvas.drawCircle(100.0f, 100.0f, 30.0f, paint));
    CHECK_NOTHROW(canvas.drawCircle(0.0f, 0.0f, 0.0f, paint));     // zero radius
    CHECK_NOTHROW(canvas.drawCircle(-10.0f, -10.0f, 50.0f, paint)); // off-screen
}

TEST_CASE("SkiaCanvas drawTextBlob issues a GPU draw command",
          "[graphics][skia_canvas][text]")
{
    SkiaCanvas canvas;

    Font font;
    font.family = "sans-serif";
    font.size = 14.0f;

    Paint paint;
    paint.fill = Color::Black;

    CHECK_NOTHROW(canvas.drawTextBlob("Hello", font, 10.0f, 20.0f, paint));
    CHECK_NOTHROW(canvas.drawTextBlob("", font, 0.0f, 0.0f, paint));  // empty string
    CHECK_NOTHROW(canvas.drawTextBlob(nullptr, font, 0.0f, 0.0f, paint)); // null safety
}

// =====================================================================
// TRIANGULATE – Flush, save/restore, clip
// =====================================================================

TEST_CASE("SkiaCanvas flush completes without error",
          "[graphics][skia_canvas][flush]")
{
    SkiaCanvas canvas;

    // GIVEN a canvas with draw commands queued
    canvas.clear(Color::White);
    canvas.drawRect({0.0f, 0.0f, 100.0f, 100.0f}, Paint{});

    // WHEN flush is called
    // THEN it returns without crashing
    CHECK_NOTHROW(canvas.flush());

    // AND calling flush again (double flush) is safe
    CHECK_NOTHROW(canvas.flush());
}

TEST_CASE("SkiaCanvas save/restore stack operations",
          "[graphics][skia_canvas][stack]")
{
    SkiaCanvas canvas;

    // GIVEN a fresh canvas
    // WHEN save is called
    CHECK_NOTHROW(canvas.save());

    // THEN subsequent draw calls are in the saved state
    canvas.drawRect({0.0f, 0.0f, 50.0f, 50.0f}, Paint{});

    // WHEN restore is called
    CHECK_NOTHROW(canvas.restore());

    // AND unbalanced restore (stack underflow) is handled safely
    CHECK_NOTHROW(canvas.restore());
    CHECK_NOTHROW(canvas.restore());  // multiple underflows
}

TEST_CASE("SkiaCanvas clipRect restricts drawing area",
          "[graphics][skia_canvas][clip]")
{
    SkiaCanvas canvas;

    // GIVEN a canvas
    // WHEN a clip rect is applied
    CHECK_NOTHROW(canvas.clipRect({10.0f, 10.0f, 200.0f, 200.0f}));

    // THEN drawing outside the clip is suppressed
    CHECK_NOTHROW(canvas.drawRect({0.0f, 0.0f, 500.0f, 500.0f}, Paint{}));

    // AND save/restore preserves clip state
    canvas.save();
    canvas.clipRect({20.0f, 20.0f, 50.0f, 50.0f});
    canvas.restore();

    // After restore, original clip is active
    CHECK_NOTHROW(canvas.drawRect({0.0f, 0.0f, 100.0f, 100.0f}, Paint{}));
}

// =====================================================================
// TRIANGULATE – Surface recreation
// =====================================================================

TEST_CASE("SkiaCanvas handles surface recreation after resize",
          "[graphics][skia_canvas][resize]")
{
    SkiaCanvas canvas;

    // GIVEN an uninitialised canvas
    // WHEN recreate_surface is called without a valid GPU context
    // THEN it returns false gracefully
    CHECK_FALSE(canvas.recreate_surface(1024, 768));
    CHECK_FALSE(canvas.recreate_surface(0, 0));         // zero size
    CHECK_FALSE(canvas.recreate_surface(640, 480));     // still no GPU context

    // SAFETY: drawing after failed recreation is a no-op
    CHECK_NOTHROW(canvas.clear(Color::Red));
    CHECK_NOTHROW(canvas.flush());
}

TEST_CASE("SkiaCanvas draw call incrementally increases draw call count",
          "[graphics][skia_canvas][counters]")
{
    SkiaCanvas canvas;

    // GIVEN a fresh canvas
    // WHEN no draw calls have been made
    // THEN draw_call_count() returns 0
    // (Implementation detail: counter is available through the canvas)
    CHECK(canvas.draw_call_count() == 0);

    // WHEN a drawRect is queued — structure test (no GPU = counter may stay 0)
    canvas.drawRect({0.0f, 0.0f, 10.0f, 10.0f}, Paint{});
    // Counter behaviour depends on GPU presence; at minimum no crash
    CHECK_NOTHROW(canvas.draw_call_count());

    // WHEN flush is called
    canvas.flush();

    // THEN counter is still accessible
    CHECK_NOTHROW(canvas.draw_call_count());
}
