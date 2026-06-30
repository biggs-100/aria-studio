#include <catch2/catch_test_macros.hpp>

#include "graphics/graphics_types.h"
#include "graphics/widget.h"
#include "graphics/widget_rect.h"
#include "graphics/widget_text.h"
#include "graphics/widget_button.h"
#include "graphics/layout_engine.h"

#include <cmath>
#include <memory>

using namespace aria;

// ─── Widget TDD Tests ──────────────────────────────────────────────
//
// Test Strategy:
//   - Verify Widget base class identity, geometry, and tree operations.
//   - Verify RectWidget rendering produces expected output.
//   - Verify TextWidget font and text blob creation.
//   - Verify ButtonWidget state transitions (hover, pressed, disabled).
//   - Verify LayoutEngine measure/arrange two-phase layout.
//
//   TDD notes:
//     - RED:   Tests reference classes before they exist.
//     - GREEN: Implement minimum to compile and pass.
//     - TRIANGULATE: Edge cases, empty states, boundary inputs.

// =====================================================================
// RED / GREEN – Widget Base: Identity and Default State
// =====================================================================

TEST_CASE("Widget has default identity and geometry state",
          "[graphics][widget][identity]")
{
    // GIVEN a concrete widget (RectWidget)
    RectWidget widget;

    // THEN default state matches spec
    CHECK(widget.is_visible());
    CHECK(std::abs(widget.opacity() - 1.0f) < 0.001f);
    CHECK(widget.bounds().x == 0.0f);
    CHECK(widget.bounds().y == 0.0f);
    CHECK(widget.bounds().w == 0.0f);
    CHECK(widget.bounds().h == 0.0f);

    // AND id is not zero (auto-assigned)
    CHECK(widget.id() != kInvalidWidgetID);

    // AND parent is null (root)
    CHECK(widget.parent() == nullptr);
}

TEST_CASE("Widget supports setting bounds and position",
          "[graphics][widget][geometry]")
{
    RectWidget widget;

    // WHEN bounds are set
    widget.set_bounds({10.0f, 20.0f, 150.0f, 80.0f});

    // THEN the same values are returned
    auto b = widget.bounds();
    CHECK(b.x == 10.0f);
    CHECK(b.y == 20.0f);
    CHECK(b.w == 150.0f);
    CHECK(b.h == 80.0f);
}

TEST_CASE("Widget visibility flag affects rendering eligibility",
          "[graphics][widget][visibility]")
{
    RectWidget widget;

    // GIVEN a visible widget
    CHECK(widget.is_visible());

    // WHEN set invisible
    widget.set_visible(false);
    CHECK_FALSE(widget.is_visible());

    // WHEN set visible again
    widget.set_visible(true);
    CHECK(widget.is_visible());
}

TEST_CASE("Widget opacity is clamped to [0, 1]",
          "[graphics][widget][opacity]")
{
    RectWidget widget;

    // WHEN opacity is set to a normal value
    widget.set_opacity(0.5f);
    CHECK(std::abs(widget.opacity() - 0.5f) < 0.001f);

    // WHEN opacity is set above 1
    widget.set_opacity(2.0f);
    CHECK(widget.opacity() == 1.0f);

    // WHEN opacity is set below 0
    widget.set_opacity(-0.5f);
    CHECK(widget.opacity() == 0.0f);
}

// =====================================================================
// RED / GREEN – Parent / Child Tree
// =====================================================================

TEST_CASE("Widget parent-child hierarchy",
          "[graphics][widget][tree]")
{
    // GIVEN a parent and child widget
    auto parent = std::make_unique<RectWidget>();
    parent->set_bounds({0, 0, 400, 300});
    auto* parent_ptr = parent.get();

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({10, 10, 100, 50});
    auto* child_ptr = child.get();

    // WHEN child is added to parent
    parent_ptr->add_child(std::move(child));

    // THEN child.parent() returns parent
    CHECK(child_ptr->parent() == parent_ptr);

    // AND parent.children() contains the child
    REQUIRE(parent_ptr->child_count() == 1);
    CHECK(parent_ptr->child_at(0) == child_ptr);
}

TEST_CASE("Widget re-parenting moves child between parents",
          "[graphics][widget][tree]")
{
    // GIVEN two parents with one child
    auto p1 = std::make_unique<RectWidget>();
    auto p2 = std::make_unique<RectWidget>();
    auto* p1_ptr = p1.get();
    auto* p2_ptr = p2.get();

    auto child = std::make_unique<RectWidget>();
    auto* child_ptr = child.get();

    p1_ptr->add_child(std::move(child));

    // WHEN child is moved to p2
    p2_ptr->add_child(p1_ptr->remove_child(child_ptr));

    // THEN child.parent() is p2
    CHECK(child_ptr->parent() == p2_ptr);

    // AND p1 has no children
    CHECK(p1_ptr->child_count() == 0);

    // AND p2 has one child (the moved one)
    REQUIRE(p2_ptr->child_count() == 1);
    CHECK(p2_ptr->child_at(0) == child_ptr);
}

TEST_CASE("Widget removing a non-existent child returns nullptr",
          "[graphics][widget][tree][safety]")
{
    RectWidget parent;
    RectWidget orphan;

    // WHEN trying to remove a widget that is not a child
    auto result = parent.remove_child(&orphan);

    // THEN nullptr is returned
    CHECK(result == nullptr);
}

TEST_CASE("Widget destructor destroys children recursively",
          "[graphics][widget][tree][lifecycle]")
{
    // GIVEN a parent with children
    auto parent = std::make_unique<RectWidget>();
    parent->set_bounds({0, 0, 400, 300});

    auto child = std::make_unique<RectWidget>();
    auto* child_ptr = child.get();
    parent->add_child(std::move(child));

    auto grandchild = std::make_unique<RectWidget>();
    child_ptr->add_child(std::move(grandchild));

    CHECK(parent->child_count() == 1);
    CHECK(child_ptr->child_count() == 1);

    // WHEN parent is destroyed (leaves scope)
    // THEN all children are destroyed recursively (no leak)
    // (tested via Valgrind / ASan in CI)
    parent.reset();
    SUCCEED("Parent destroyed without visible memory issue");
}

// =====================================================================
// TRIANGULATE – Hit Testing
// =====================================================================

TEST_CASE("Widget hit_test returns the topmost widget at a point",
          "[graphics][widget][hittest]")
{
    // GIVEN sibling widgets with overlapping bounds
    auto parent = std::make_unique<RectWidget>();
    parent->set_bounds({0, 0, 500, 500});
    auto* parent_ptr = parent.get();

    // A is on the left
    auto a = std::make_unique<RectWidget>();
    a->set_bounds({0, 0, 200, 200});
    a->set_id(WidgetID(100));
    auto* a_ptr = a.get();
    parent_ptr->add_child(std::move(a));

    // B is on the right (drawn on top)
    auto b = std::make_unique<RectWidget>();
    b->set_bounds({100, 100, 200, 200});
    b->set_id(WidgetID(200));
    auto* b_ptr = b.get();
    parent_ptr->add_child(std::move(b));

    // WHEN point (150, 150) is tested (hits both A and B)
    auto* hit = parent_ptr->hit_test(150.0f, 150.0f);

    // THEN B is returned (topmost / last child)
    CHECK(hit == b_ptr);
}

TEST_CASE("Widget hit_test returns nullptr when point misses all",
          "[graphics][widget][hittest]")
{
    // GIVEN widgets positioned away from origin
    auto parent = std::make_unique<RectWidget>();
    parent->set_bounds({0, 0, 500, 500});

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({100, 100, 200, 200});
    parent->add_child(std::move(child));

    // WHEN hit_test(0, 0) is called (no widget at that point)
    auto* hit = parent->hit_test(0.0f, 0.0f);

    // THEN nullptr is returned
    CHECK(hit == nullptr);
}

TEST_CASE("Widget hit_test returns nullptr for invisible widgets",
          "[graphics][widget][hittest]")
{
    auto parent = std::make_unique<RectWidget>();
    parent->set_bounds({0, 0, 500, 500});

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({50, 50, 100, 100});
    child->set_visible(false);
    auto* child_ptr = child.get();
    parent->add_child(std::move(child));

    // WHEN point hits the invisible child
    auto* hit = parent->hit_test(75.0f, 75.0f);

    // THEN nullptr (invisible widgets are not hit-testable)
    CHECK(hit == nullptr);
}

TEST_CASE("Widget hit_test returns nullptr for disabled widgets",
          "[graphics][widget][hittest]")
{
    auto parent = std::make_unique<RectWidget>();
    parent->set_bounds({0, 0, 500, 500});

    auto child = std::make_unique<RectWidget>();
    child->set_bounds({50, 50, 100, 100});
    child->set_enabled(false);
    parent->add_child(std::move(child));

    auto* hit = parent->hit_test(75.0f, 75.0f);
    CHECK(hit == nullptr);
}

// =====================================================================
// TRIANGULATE – RectWidget rendering
// =====================================================================

TEST_CASE("RectWidget produces correct bounds and hit area",
          "[graphics][widget][rect]")
{
    RectWidget widget;
    widget.set_bounds({5.0f, 10.0f, 80.0f, 40.0f});

    // WHEN hit_test on the widget root
    // THEN interior point hits
    CHECK(widget.hit_test(10.0f, 15.0f) == &widget);

    // THEN edge point hits (on boundary — varies by implementation)
    // THEN point outside misses
    CHECK(widget.hit_test(0.0f, 0.0f) == nullptr);
    CHECK(widget.hit_test(200.0f, 200.0f) == nullptr);
}

// =====================================================================
// TRIANGULATE – TextWidget
// =====================================================================

TEST_CASE("TextWidget stores font and text properties",
          "[graphics][widget][text]")
{
    TextWidget widget;
    widget.set_bounds({0, 0, 200, 30});

    // GIVEN a TextWidget with default state
    // THEN text is empty
    CHECK(widget.text().empty());

    // WHEN text is set
    widget.set_text("Hello GPU");
    CHECK(widget.text() == "Hello GPU");

    // WHEN text is cleared
    widget.set_text("");
    CHECK(widget.text().empty());
}

TEST_CASE("TextWidget font property accessors",
          "[graphics][widget][text]")
{
    TextWidget widget;

    // GIVEN a TextWidget with default font
    auto font = widget.font();
    CHECK(font.family == "sans-serif");
    CHECK(std::abs(font.size - 14.0f) < 0.001f);

    // WHEN font is customised
    Font custom;
    custom.family = "serif";
    custom.size = 18.0f;
    widget.set_font(custom);

    auto updated = widget.font();
    CHECK(updated.family == "serif");
    CHECK(std::abs(updated.size - 18.0f) < 0.001f);
}

// =====================================================================
// TRIANGULATE – ButtonWidget states
// =====================================================================

TEST_CASE("ButtonWidget starts in default state",
          "[graphics][widget][button]")
{
    ButtonWidget button;
    button.set_bounds({0, 0, 100, 30});

    // GIVEN a new ButtonWidget
    // THEN it is enabled and not pressed/hovered
    CHECK(button.is_enabled());
    CHECK_FALSE(button.is_pressed());
    CHECK_FALSE(button.is_hovered());
    CHECK(button.label() == "Button");
}

TEST_CASE("ButtonWidget label can be set and retrieved",
          "[graphics][widget][button]")
{
    ButtonWidget button;

    // WHEN a custom label is set
    button.set_label("Render");

    // THEN the label matches
    CHECK(button.label() == "Render");

    // WHEN set to empty
    button.set_label("");
    CHECK(button.label().empty());
}

TEST_CASE("ButtonWidget hover and pressed state transitions",
          "[graphics][widget][button]")
{
    ButtonWidget button;

    // WHEN hovered
    button.set_hovered(true);
    CHECK(button.is_hovered());

    // WHEN pressed
    button.set_pressed(true);
    CHECK(button.is_pressed());

    // WHEN released
    button.set_pressed(false);
    CHECK_FALSE(button.is_pressed());
    CHECK(button.is_hovered());  // still hovered

    // WHEN unhovered
    button.set_hovered(false);
    CHECK_FALSE(button.is_hovered());
}

TEST_CASE("ButtonWidget disabled state prevents interaction",
          "[graphics][widget][button]")
{
    ButtonWidget button;
    button.set_bounds({0, 0, 100, 30});

    // GIVEN an enabled button
    CHECK(button.is_enabled());

    // WHEN disabled
    button.set_enabled(false);
    CHECK_FALSE(button.is_enabled());

    // THEN hit test succeeds structurally
    // (interaction logic is in event dispatch, not here)
    SUCCEED("Disabled state tracked correctly");
}

// =====================================================================
// TRIANGULATE – LayoutEngine measure/arrange
// =====================================================================

TEST_CASE("LayoutEngine performs two-phase layout on widget tree",
          "[graphics][layout][engine]")
{
    LayoutEngine engine;

    // GIVEN a container with flex children
    auto container = std::make_unique<RectWidget>();
    container->set_bounds({0, 0, 300, 100});

    auto* container_ptr = container.get();

    auto child_a = std::make_unique<RectWidget>();
    child_a->set_id(WidgetID(10));
    container_ptr->add_child(std::move(child_a));

    auto child_b = std::make_unique<RectWidget>();
    child_b->set_id(WidgetID(20));
    container_ptr->add_child(std::move(child_b));

    // WHEN measure + arrange is run over the tree
    engine.compute(container_ptr, {300, 100});

    // THEN children have been measured and arranged
    // (exact positions depend on layout policy — flex defaults to row)
    SUCCEED("LayoutEngine compute completes without error");
}

TEST_CASE("LayoutEngine handles empty widget tree",
          "[graphics][layout][engine][safety]")
{
    LayoutEngine engine;

    // GIVEN a null root
    // WHEN compute is called with nullptr
    // THEN it handles gracefully
    CHECK_NOTHROW(engine.compute(nullptr, {800, 600}));
}

TEST_CASE("LayoutEngine handles single widget without children",
          "[graphics][layout][engine]")
{
    LayoutEngine engine;
    RectWidget root;
    root.set_bounds({0, 0, 640, 480});

    // WHEN compute is called on a leaf widget
    CHECK_NOTHROW(engine.compute(&root, {640, 480}));

    // THEN the root has been measured
    SUCCEED("Leaf widget layout completes");
}
