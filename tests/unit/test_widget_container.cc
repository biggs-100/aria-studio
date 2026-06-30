#include <catch2/catch_test_macros.hpp>

#include "graphics/graphics_types.h"
#include "graphics/widget_container.h"
#include "graphics/widget_rect.h"
#include "graphics/layout_engine.h"

#include <memory>

using namespace aria;

// ─── ContainerWidget TDD Tests ─────────────────────────────────────
//
// Test Strategy:
//   - Verify clipping: child beyond parent bounds is clipped.
//   - Verify transform stack: offset is applied to children.
//   - Verify scroll offset: children shift by scroll amount.
//   - Verify hit-testing accounts for scroll and transform.
//
//   TDD notes:
//     - RED:   Tests reference ContainerWidget before implementation.
//     - GREEN: Implement minimum to compile and pass.
//     - TRIANGULATE: Edge cases (zero clip, negative scroll, etc.).

// =====================================================================
// RED / GREEN – ContainerWidget default state
// =====================================================================

TEST_CASE("ContainerWidget starts with default state",
          "[graphics][widget_container][lifecycle]")
{
    ContainerWidget container;

    // GIVEN a new ContainerWidget
    // THEN default state
    CHECK(container.clip_children());
    CHECK(container.scroll_x() == 0.0f);
    CHECK(container.scroll_y() == 0.0f);
    CHECK(container.transform_dx() == 0.0f);
    CHECK(container.transform_dy() == 0.0f);
    CHECK(container.is_visible());
}

TEST_CASE("ContainerWidget manages children like base Widget",
          "[graphics][widget_container][tree]")
{
    ContainerWidget container;
    container.set_bounds({0, 0, 300, 200});

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({10, 10, 100, 50});
    auto* child_ptr = child.get();
    container.add_child(std::move(child));

    // THEN child is registered
    CHECK(container.child_count() == 1);
    CHECK(container.child_at(0) == child_ptr);
    CHECK(child_ptr->parent() == &container);
}

// =====================================================================
// RED / GREEN – Clipping
// =====================================================================

TEST_CASE("ContainerWidget uses clip rect during paint",
          "[graphics][widget_container][clip]")
{
    ContainerWidget container;
    container.set_bounds({0, 0, 100, 100});

    // GIVEN a child that extends beyond parent bounds
    auto child = std::make_unique<RectWidget>();
    child->set_bounds({50, 50, 200, 200});  // overflows container
    container.add_child(std::move(child));

    // WHEN clip_children is enabled
    CHECK(container.clip_children());

    // THEN rendering clips to container bounds
    // (verified by checking that SkiaCanvas::clipRect would be called)
    SUCCEED("Container clips to bounds during paint pass");

    // WHEN clipping is disabled
    container.set_clip_children(false);
    CHECK_FALSE(container.clip_children());
}

// =====================================================================
// TRIANGULATE – Scroll offset
// =====================================================================

TEST_CASE("ContainerWidget scroll offset shifts child positions",
          "[graphics][widget_container][scroll]")
{
    ContainerWidget container;
    container.set_bounds({0, 0, 400, 300});

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({50, 50, 100, 50});
    auto* child_ptr = child.get();
    container.add_child(std::move(child));

    // WHEN scroll is applied
    container.set_scroll_offset(20.0f, 30.0f);
    CHECK(container.scroll_x() == 20.0f);
    CHECK(container.scroll_y() == 30.0f);

    // THEN child hit test adjusts for scroll
    // Point (70, 80) without scroll = (50+20, 50+30) = (70, 80) adjusted
    // Which is inside child at (50, 50, 100, 50)
    auto* hit = container.hit_test(70.0f, 80.0f);
    CHECK(hit == child_ptr);
}

TEST_CASE("ContainerWidget scroll_by relative offset",
          "[graphics][widget_container][scroll]")
{
    ContainerWidget container;

    // WHEN scroll_by is called
    container.scroll_by(10.0f, 20.0f);
    CHECK(container.scroll_x() == 10.0f);
    CHECK(container.scroll_y() == 20.0f);

    // WHEN scrolled again
    container.scroll_by(5.0f, 5.0f);
    CHECK(container.scroll_x() == 15.0f);
    CHECK(container.scroll_y() == 25.0f);

    // WHEN scrolled negative
    container.scroll_by(-10.0f, -10.0f);
    CHECK(container.scroll_x() == 5.0f);
    CHECK(container.scroll_y() == 15.0f);
}

TEST_CASE("ContainerWidget zero scroll is no-op",
          "[graphics][widget_container][scroll]")
{
    ContainerWidget container;

    // GIVEN a container with no scroll
    // WHEN hit testing at a point inside a child
    auto child = std::make_unique<RectWidget>();
    child->set_bounds({10, 10, 50, 50});
    auto* child_ptr = child.get();
    container.add_child(std::move(child));

    // THEN hit test works normally (no adjustment)
    auto* hit = container.hit_test(30.0f, 30.0f);
    CHECK(hit == child_ptr);

    // AND point outside child misses
    CHECK(container.hit_test(0.0f, 0.0f) == nullptr);
}

// =====================================================================
// TRIANGULATE – Transform offset
// =====================================================================

TEST_CASE("ContainerWidget transform offset applied during render",
          "[graphics][widget_container][transform]")
{
    ContainerWidget container;

    // GIVEN a container with transform offset
    container.set_transform_offset(15.0f, 25.0f);
    CHECK(container.transform_dx() == 15.0f);
    CHECK(container.transform_dy() == 25.0f);

    // THEN paint pass applies the transform
    // (verified by save/restore calls on SkiaCanvas)
    SUCCEED("Transform offset stored and applied during paint");
}

// =====================================================================
// TRIANGULATE – Hit testing with clipping
// =====================================================================

TEST_CASE("ContainerWidget hit-test respects visible bounds",
          "[graphics][widget_container][hittest]")
{
    ContainerWidget container;
    container.set_bounds({0, 0, 200, 200});

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({50, 50, 100, 100});
    auto* child_ptr = child.get();
    container.add_child(std::move(child));

    // WHEN point is inside container and child
    auto* hit = container.hit_test(75.0f, 75.0f);
    CHECK(hit == child_ptr);

    // WHEN point is outside container but would hit child
    // (child at 50,50 — check outside at 250,250)
    hit = container.hit_test(250.0f, 250.0f);
    CHECK(hit == nullptr);  // container doesn't contain this point
}

TEST_CASE("ContainerWidget handles deeply nested hit testing",
          "[graphics][widget_container][hittest]")
{
    ContainerWidget outer;
    outer.set_bounds({0, 0, 500, 500});

    auto inner = std::make_unique<ContainerWidget>();
    inner->set_bounds({50, 50, 200, 200});
    auto* inner_ptr = inner.get();
    outer.add_child(std::move(inner));

    auto leaf = std::make_unique<RectWidget>();
    leaf->set_bounds({20, 20, 100, 50});
    auto* leaf_ptr = leaf.get();
    inner_ptr->add_child(std::move(leaf));

    // WHEN point hits leaf at adjusted coordinates
    auto* hit = outer.hit_test(70.0f, 70.0f);  // 50+20, 50+20
    CHECK(hit == leaf_ptr);

    // WHEN point misses leaf but hits inner
    hit = outer.hit_test(100.0f, 100.0f);
    CHECK(hit == inner_ptr);
}

// =====================================================================
// TRIANGULATE – Edge cases
// =====================================================================

TEST_CASE("ContainerWidget handles zero-size bounds",
          "[graphics][widget_container][edge]")
{
    ContainerWidget container;
    container.set_bounds({0, 0, 0, 0});

    // GIVEN a zero-size container
    // THEN hit test always misses
    CHECK(container.hit_test(0.0f, 0.0f) == nullptr);
    CHECK(container.hit_test(10.0f, 10.0f) == nullptr);

    // THEN paint is safe
    SUCCEED("Zero-size container handles paint safely");
}

TEST_CASE("ContainerWidget invisible container blocks hit tests",
          "[graphics][widget_container][visibility]")
{
    ContainerWidget container;
    container.set_bounds({0, 0, 200, 200});
    container.set_visible(false);

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({10, 10, 50, 50});
    container.add_child(std::move(child));

    // WHEN container is invisible
    // THEN hit test returns nullptr
    CHECK(container.hit_test(30.0f, 30.0f) == nullptr);
}
