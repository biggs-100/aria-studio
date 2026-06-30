#include <catch2/catch_test_macros.hpp>

#include "graphics/ffi/event_dispatcher.h"
#include "graphics/focus_manager.h"
#include "graphics/graphics_types.h"
#include "graphics/input_types.h"
#include "graphics/shortcut_manager.h"
#include "graphics/widget.h"
#include "graphics/widget_rect.h"

#include <memory>
#include <vector>

using namespace aria;
using namespace aria::ffi;

// =====================================================================
// Input System TDD Tests
//
// Test Strategy:
//   - Verify FocusManager tab-order traversal (next/prev/wrap)
//   - Verify FocusManager set_focus dispatches focus/blur events
//   - Verify ShortcutManager priority dispatch and propagation
//   - Verify ShortcutManager conflict detection
//   - Verify EventDispatcher touch/pen dispatch methods
//   - Verify Widget effective_role inheritance
//
//   TDD notes:
//     - RED:   Tests reference new classes before creation.
//     - GREEN: Implement minimum to compile and pass.
//     - TRIANGULATE: Edge cases (empty list, wrap, propagation chains).
// =====================================================================

// ═════════════════════════════════════════════════════════════════
// Helper: create a focusable widget with a given tab_index.
// ═════════════════════════════════════════════════════════════════

namespace {

/// Create a simple focusable widget for testing.
struct TestWidget {
    std::unique_ptr<RectWidget> widget;
    WidgetID id;
    bool focus_gained = false;
    bool focus_lost = false;

    TestWidget(int tab_index, Rect bounds) {
        widget = std::make_unique<RectWidget>();
        id = widget->id();
        widget->set_tab_index(tab_index);
        widget->set_bounds(bounds);
        widget->on_focus([this]() { focus_gained = true; });
        widget->on_blur([this]() { focus_lost = true; });
    }
};

/// Create a container with multiple focusable children.
struct FocusTestTree {
    std::unique_ptr<RectWidget> root;
    std::vector<std::unique_ptr<TestWidget>> items;

    void add_child(int tab_index, Rect bounds) {
        auto tw = std::make_unique<TestWidget>(tab_index, bounds);
        auto* ptr = tw->widget.get();
        items.push_back(std::move(tw));
        root->add_child(std::unique_ptr<Widget>(ptr));
        // Release from unique_ptr since root now owns it
        items.back()->widget.release();
    }

    void rebuild(FocusManager& fm) {
        fm.rebuild_list(root.get());
    }
};

} // anonymous namespace

// ═════════════════════════════════════════════════════════════════
// Input types
// ═════════════════════════════════════════════════════════════════

TEST_CASE("TouchEvent has default values", "[input][touch]") {
    TouchEvent ev;

    CHECK(ev.x == 0.0f);
    CHECK(ev.y == 0.0f);
    CHECK(ev.touch_id == 0);
}

TEST_CASE("PenEvent has default values", "[input][pen]") {
    PenEvent ev;

    CHECK(ev.x == 0.0f);
    CHECK(ev.y == 0.0f);
    CHECK(ev.pressure == 0.0f);
    CHECK(ev.tilt_x == 0.0f);
    CHECK(ev.tilt_y == 0.0f);
    CHECK(ev.barrel_button == false);
}

TEST_CASE("TouchEvent can hold multi-touch coordinates", "[input][touch]") {
    TouchEvent ev;
    ev.x = 300.0f;
    ev.y = 200.0f;
    ev.touch_id = 0;

    CHECK(ev.x == 300.0f);
    CHECK(ev.y == 200.0f);
    CHECK(ev.touch_id == 0);

    TouchEvent ev2;
    ev2.x = 100.0f;
    ev2.y = 50.0f;
    ev2.touch_id = 1;

    CHECK(ev2.touch_id == 1);
    CHECK(ev2.x == 100.0f);
}

TEST_CASE("PenEvent carries pressure, tilt, and barrel state", "[input][pen]") {
    PenEvent ev;
    ev.x = 400.0f;
    ev.y = 300.0f;
    ev.pressure = 0.75f;
    ev.tilt_x = 15.0f;
    ev.tilt_y = 0.0f;
    ev.barrel_button = false;

    CHECK(ev.x == 400.0f);
    CHECK(ev.y == 300.0f);
    CHECK(ev.pressure == 0.75f);
    CHECK(ev.tilt_x == 15.0f);
    CHECK(ev.tilt_y == 0.0f);
    CHECK(ev.barrel_button == false);

    // Barrel button can be true
    PenEvent ev2;
    ev2.barrel_button = true;
    CHECK(ev2.barrel_button == true);
}

// ═════════════════════════════════════════════════════════════════
// FocusManager: tab navigation
// ═════════════════════════════════════════════════════════════════

TEST_CASE("FocusManager: focus_next cycles through tab_order",
          "[focus][navigation]")
{
    // GIVEN three focusable widgets with tab_index 1, 2, 3
    FocusManager fm;
    FocusTestTree tree;
    tree.root = std::make_unique<RectWidget>();
    tree.root->set_bounds({0, 0, 500, 500});

    tree.add_child(3, {0, 0, 100, 50});  // item[0] — tab 3
    tree.add_child(1, {0, 0, 100, 50});  // item[1] — tab 1
    tree.add_child(2, {0, 0, 100, 50});  // item[2] — tab 2

    tree.rebuild(fm);

    // WHEN focus_next() is called starting from none
    fm.focus_next();

    // THEN the first tab in order is focused (tab_index 1 = item[1])
    CHECK(fm.focused_widget() == tree.items[1]->id);

    // WHEN focus_next() again
    fm.focus_next();

    // THEN tab_index 2 (item[2]) is focused
    CHECK(fm.focused_widget() == tree.items[2]->id);

    // WHEN focus_next() again
    fm.focus_next();

    // THEN tab_index 3 (item[0]) is focused
    CHECK(fm.focused_widget() == tree.items[0]->id);
}

TEST_CASE("FocusManager: focus_next wraps to first at end",
          "[focus][navigation][wrap]")
{
    // GIVEN two focusable widgets A(1), B(2) with B focused
    FocusManager fm;
    FocusTestTree tree;
    tree.root = std::make_unique<RectWidget>();
    tree.root->set_bounds({0, 0, 500, 500});

    tree.add_child(1, {0, 0, 100, 50});  // item[0] — tab 1 (A)
    tree.add_child(2, {0, 0, 100, 50});  // item[1] — tab 2 (B)

    tree.rebuild(fm);
    fm.set_focus(tree.items[1]->id);  // Focus B

    // WHEN focus_next() from last item
    fm.focus_next();

    // THEN focus wraps to first (A)
    CHECK(fm.focused_widget() == tree.items[0]->id);
}

TEST_CASE("FocusManager: focus_previous wraps to last at beginning",
          "[focus][navigation][wrap]")
{
    // GIVEN two focusable widgets A(1), B(2) with A focused
    FocusManager fm;
    FocusTestTree tree;
    tree.root = std::make_unique<RectWidget>();
    tree.root->set_bounds({0, 0, 500, 500});

    tree.add_child(1, {0, 0, 100, 50});  // item[0] — tab 1 (A)
    tree.add_child(2, {0, 0, 100, 50});  // item[1] — tab 2 (B)

    tree.rebuild(fm);
    fm.set_focus(tree.items[0]->id);  // Focus A

    // WHEN focus_previous() from first item
    fm.focus_previous();

    // THEN focus wraps to last (B)
    CHECK(fm.focused_widget() == tree.items[1]->id);
}

TEST_CASE("FocusManager: non-focusable widgets are skipped",
          "[focus][navigation][skip]")
{
    // GIVEN widgets A (focusable), B (disabled), C (focusable)
    FocusManager fm;
    auto root = std::make_unique<RectWidget>();
    root->set_bounds({0, 0, 500, 500});

    auto a = std::make_unique<RectWidget>();
    a->set_tab_index(1);
    a->set_bounds({0, 0, 100, 50});
    auto* a_ptr = a.get();
    auto a_id = a->id();
    root->add_child(std::move(a));

    auto b = std::make_unique<RectWidget>();
    b->set_tab_index(2);
    b->set_bounds({100, 0, 100, 50});
    b->set_enabled(false);  // non-focusable
    root->add_child(std::move(b));

    auto c = std::make_unique<RectWidget>();
    c->set_tab_index(3);
    c->set_bounds({200, 0, 100, 50});
    auto* c_ptr = c.get();
    auto c_id = c->id();
    root->add_child(std::move(c));

    fm.rebuild_list(root.get());

    // WHEN focus_next() from A with A focused
    fm.set_focus(a_id);
    fm.focus_next();

    // THEN C receives focus (B is skipped)
    CHECK(fm.focused_widget() == c_id);
}

TEST_CASE("FocusManager: empty tab order handles gracefully",
          "[focus][safety]")
{
    FocusManager fm;

    // GIVEN no focusable widgets
    // WHEN operations are called
    // THEN no crash
    CHECK_NOTHROW(fm.focus_next());
    CHECK_NOTHROW(fm.focus_previous());
    CHECK(fm.focused_widget() == kInvalidWidgetID);
}

TEST_CASE("FocusManager: rebuild_list with null root clears order",
          "[focus][safety]")
{
    FocusManager fm;

    fm.rebuild_list(nullptr);

    // THEN tab order is empty
    CHECK(fm.focused_widget() == kInvalidWidgetID);
    CHECK_NOTHROW(fm.focus_next());
}

// ═════════════════════════════════════════════════════════════════
// FocusManager: programmatic set_focus
// ═════════════════════════════════════════════════════════════════

TEST_CASE("FocusManager: set_focus dispatches focus/blur events via widget",
          "[focus][events]")
{
    // GIVEN two focusable widgets with blink callbacks
    FocusManager fm;
    auto root = std::make_unique<RectWidget>();
    root->set_bounds({0, 0, 500, 500});

    auto a = std::make_unique<RectWidget>();
    a->set_tab_index(1);
    a->set_bounds({0, 0, 100, 50});
    auto a_id = a->id();
    bool a_focused = false;
    bool a_blurred = false;
    a->on_focus([&]() { a_focused = true; });
    a->on_blur([&]() { a_blurred = true; });
    auto* a_ptr = a.get();
    root->add_child(std::move(a));

    auto b = std::make_unique<RectWidget>();
    b->set_tab_index(2);
    b->set_bounds({100, 0, 100, 50});
    auto b_id = b->id();
    bool b_focused = false;
    bool b_blurred = false;
    b->on_focus([&]() { b_focused = true; });
    b->on_blur([&]() { b_blurred = true; });
    auto* b_ptr = b.get();
    root->add_child(std::move(b));

    fm.rebuild_list(root.get());

    // WHEN set_focus(A_id)
    fm.set_focus(a_id);
    a_ptr->set_focused(true);
    CHECK(a_focused);
    CHECK_FALSE(a_blurred);

    // WHEN set_focus moves from A to B
    fm.set_focus(b_id);
    a_ptr->set_focused(false);
    b_ptr->set_focused(true);

    // THEN A lost focus, B gained focus
    CHECK(a_blurred);
    CHECK(b_focused);
    CHECK(fm.focused_widget() == b_id);
}

// ═════════════════════════════════════════════════════════════════
// ShortcutManager: priority dispatch
// ═════════════════════════════════════════════════════════════════

TEST_CASE("ShortcutManager: dispatch fires highest-priority handler",
          "[shortcut][dispatch]")
{
    ShortcutManager sm;

    KeyChord chord;
    chord.key = 83;    // 'S'
    chord.ctrl = true;

    int fired_priority = 0;
    bool b_fired = false;

    // GIVEN priority 10 and priority 5 handlers for Ctrl+S
    sm.register_shortcut(chord, [&]() { fired_priority = 10; return true; }, 10);
    sm.register_shortcut(chord, [&]() { fired_priority = 5; return true; }, 5);

    // WHEN dispatch Ctrl+S
    bool consumed = sm.dispatch(chord);

    // THEN highest-priority (10) fires, lower does not
    CHECK(consumed);
    CHECK(fired_priority == 10);
}

TEST_CASE("ShortcutManager: unconsumed shortcut propagates to next",
          "[shortcut][propagation]")
{
    ShortcutManager sm;

    KeyChord chord;
    chord.key = 68;    // 'D'
    chord.ctrl = true;

    std::vector<int> fire_order;

    // GIVEN F: priority 100 (returns false), G: priority 10 (returns true)
    sm.register_shortcut(chord, [&]() {
        fire_order.push_back(100);
        return false;
    }, 100);
    sm.register_shortcut(chord, [&]() {
        fire_order.push_back(10);
        return true;
    }, 10);

    // WHEN dispatch Ctrl+D
    bool consumed = sm.dispatch(chord);

    // THEN F fires first (returns false), G fires next (returns true)
    CHECK(consumed);
    REQUIRE(fire_order.size() == 2);
    CHECK(fire_order[0] == 100);
    CHECK(fire_order[1] == 10);
}

TEST_CASE("ShortcutManager: unregistered shortcut is silently ignored",
          "[shortcut][safety]")
{
    ShortcutManager sm;
    bool fired = false;

    // GIVEN no handlers for Alt+F4
    KeyChord chord;
    chord.key = 115;   // F4
    chord.alt = true;

    sm.register_shortcut(chord, [&]() { fired = true; return true; }, 0);

    // WHEN dispatch a different chord
    KeyChord other;
    other.key = 65;  // 'A'
    bool consumed = sm.dispatch(other);

    // THEN nothing fires
    CHECK_FALSE(consumed);
    CHECK_FALSE(fired);
}

TEST_CASE("ShortcutManager: conflicts() returns all handlers for chord",
          "[shortcut][conflict]")
{
    ShortcutManager sm;

    KeyChord chord;
    chord.key = 90;    // 'Z'
    chord.ctrl = true;

    auto h1 = []() { return true; };
    auto h2 = []() { return false; };

    sm.register_shortcut(chord, h1, 0);
    sm.register_shortcut(chord, h2, 0);

    // WHEN conflicts() is queried
    auto conflicts = sm.conflicts(chord);

    // THEN both handlers are returned
    CHECK(conflicts.size() == 2);
}

TEST_CASE("ShortcutManager: unregister removes all handlers for chord",
          "[shortcut][unregister]")
{
    ShortcutManager sm;

    KeyChord chord;
    chord.key = 65;  // 'A'
    chord.ctrl = true;

    sm.register_shortcut(chord, []() { return true; }, 0);
    CHECK(sm.conflicts(chord).size() == 1);

    // WHEN unregister is called
    sm.unregister_shortcut(chord);

    // THEN no handlers remain
    CHECK(sm.conflicts(chord).empty());
}

// ═════════════════════════════════════════════════════════════════
// KeyChord equality
// ═════════════════════════════════════════════════════════════════

TEST_CASE("KeyChord equality comparison works correctly",
          "[shortcut][keychord]")
{
    KeyChord a;
    a.key = 83; a.ctrl = true;

    KeyChord b;
    b.key = 83; b.ctrl = true;

    CHECK(a == b);

    KeyChord c;
    c.key = 83; c.ctrl = true; c.shift = true;
    CHECK_FALSE(a == c);
    CHECK(a != c);

    KeyChord d;
    d.key = 65;
    CHECK_FALSE(a == d);
}

// ═════════════════════════════════════════════════════════════════
// EventDispatcher: touch events
// ═════════════════════════════════════════════════════════════════

TEST_CASE("EventDispatcher: touch down dispatches correct event",
          "[event][touch]")
{
    EventDispatcher dispatcher;
    InputEvent last;
    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    // WHEN touch down at logical (300, 200), touch_id=0
    dispatcher.dispatch_touch_down(300.0f, 200.0f, 0);

    // THEN event is correctly populated
    CHECK(last.type == EventType::kTouchDown);
    CHECK(last.x == 300.0f);
    CHECK(last.y == 200.0f);
    CHECK(last.touch_id == 0);
    CHECK(last.is_touch_or_pen);
}

TEST_CASE("EventDispatcher: multi-touch moves independent IDs",
          "[event][touch]")
{
    EventDispatcher dispatcher;
    std::vector<InputEvent> received;
    dispatcher.on_event([&](const InputEvent& ev) { received.push_back(ev); });

    // GIVEN touch 0 moves to (310, 210) and touch 1 moves to (100, 50)
    dispatcher.dispatch_touch_move(310.0f, 210.0f, 0);
    dispatcher.dispatch_touch_move(100.0f, 50.0f, 1);

    // THEN each touch has its own ID and coordinates
    REQUIRE(received.size() == 2);

    CHECK(received[0].type == EventType::kTouchMove);
    CHECK(received[0].touch_id == 0);
    CHECK(received[0].x == 310.0f);
    CHECK(received[0].y == 210.0f);

    CHECK(received[1].type == EventType::kTouchMove);
    CHECK(received[1].touch_id == 1);
    CHECK(received[1].x == 100.0f);
    CHECK(received[1].y == 50.0f);
}

TEST_CASE("EventDispatcher: touch up completes gesture",
          "[event][touch]")
{
    EventDispatcher dispatcher;
    InputEvent last;
    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    dispatcher.dispatch_touch_up(300.0f, 200.0f, 0);

    CHECK(last.type == EventType::kTouchUp);
    CHECK(last.touch_id == 0);
    CHECK(last.is_touch_or_pen);
}

// ═════════════════════════════════════════════════════════════════
// EventDispatcher: pen events
// ═════════════════════════════════════════════════════════════════

TEST_CASE("EventDispatcher: pen down carries pressure and tilt",
          "[event][pen]")
{
    EventDispatcher dispatcher;
    InputEvent last;
    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    // WHEN pen down with pressure 0.75, tilt 15°/0°
    dispatcher.dispatch_pen_down(400.0f, 300.0f, 0.75f, 15.0f, 0.0f, false);

    // THEN event carries pen data
    CHECK(last.type == EventType::kPenDown);
    CHECK(last.x == 400.0f);
    CHECK(last.y == 300.0f);
    CHECK(last.pressure == 0.75f);
    CHECK(last.tilt_x == 15.0f);
    CHECK(last.tilt_y == 0.0f);
    CHECK(last.barrel_button == false);
    CHECK(last.is_touch_or_pen);
}

TEST_CASE("EventDispatcher: pen move updates pressure",
          "[event][pen]")
{
    EventDispatcher dispatcher;
    InputEvent last;
    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    dispatcher.dispatch_pen_move(420.0f, 310.0f, 0.5f, 10.0f, 5.0f, false);

    CHECK(last.type == EventType::kPenMove);
    CHECK(last.x == 420.0f);
    CHECK(last.y == 310.0f);
    CHECK(last.pressure == 0.5f);
    CHECK(last.tilt_x == 10.0f);
    CHECK(last.tilt_y == 5.0f);
}

TEST_CASE("EventDispatcher: pen up with barrel button",
          "[event][pen]")
{
    EventDispatcher dispatcher;
    InputEvent last;
    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    dispatcher.dispatch_pen_up(400.0f, 300.0f, 0.0f, 0.0f, 0.0f, true);

    CHECK(last.type == EventType::kPenUp);
    CHECK(last.barrel_button == true);
    CHECK(last.is_touch_or_pen);
}

TEST_CASE("EventDispatcher: HiDPI scaling applies to touch coordinates",
          "[event][touch][hidpi]")
{
    EventDispatcher dispatcher;
    InputEvent last;
    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    // GIVEN 2.0x HiDPI
    dispatcher.set_device_pixel_ratio(2.0f);

    // WHEN touch at physical (600, 400)
    dispatcher.dispatch_touch_down(600.0f, 400.0f, 0);

    // THEN coordinates are scaled to logical
    CHECK(last.x == 300.0f);
    CHECK(last.y == 200.0f);
}

// ═════════════════════════════════════════════════════════════════
// Widget accessibility
// ═════════════════════════════════════════════════════════════════

TEST_CASE("Widget: default accessibility state",
          "[widget][accessibility]")
{
    RectWidget widget;

    // GIVEN a newly created widget
    // THEN all accessibility fields have safe defaults
    CHECK(widget.accessible_name().empty());
    CHECK(widget.accessible_role() == AccessibleRole::kNone);
    CHECK(widget.accessible_description().empty());
}

TEST_CASE("Widget: accessible name can be set and retrieved",
          "[widget][accessibility]")
{
    RectWidget widget;

    // WHEN accessible name is set
    widget.set_accessible_name("Play Button");

    // THEN it is returned
    CHECK(widget.accessible_name() == "Play Button");

    // WHEN cleared
    widget.set_accessible_name("");
    CHECK(widget.accessible_name().empty());
}

TEST_CASE("Widget: accessible role can be set and retrieved",
          "[widget][accessibility]")
{
    RectWidget widget;

    // WHEN role is set to kButton
    widget.set_accessible_role(AccessibleRole::kButton);
    CHECK(widget.accessible_role() == AccessibleRole::kButton);

    // WHEN changed to kSlider
    widget.set_accessible_role(AccessibleRole::kSlider);
    CHECK(widget.accessible_role() == AccessibleRole::kSlider);

    // All enum values are valid
    widget.set_accessible_role(AccessibleRole::kLabel);
    CHECK(widget.accessible_role() == AccessibleRole::kLabel);

    widget.set_accessible_role(AccessibleRole::kPanel);
    CHECK(widget.accessible_role() == AccessibleRole::kPanel);

    widget.set_accessible_role(AccessibleRole::kList);
    CHECK(widget.accessible_role() == AccessibleRole::kList);

    widget.set_accessible_role(AccessibleRole::kTextInput);
    CHECK(widget.accessible_role() == AccessibleRole::kTextInput);

    widget.set_accessible_role(AccessibleRole::kTabStop);
    CHECK(widget.accessible_role() == AccessibleRole::kTabStop);
}

TEST_CASE("Widget: accessible description is optional",
          "[widget][accessibility]")
{
    RectWidget widget;

    // GIVEN a widget with no description
    CHECK(widget.accessible_description().empty());

    // WHEN description is set
    widget.set_accessible_description("Controls master output level");
    CHECK(widget.accessible_description() == "Controls master output level");

    // WHEN cleared
    widget.set_accessible_description("");
    CHECK(widget.accessible_description().empty());
}

TEST_CASE("Widget: effective_role inherits parent role when own is kNone",
          "[widget][accessibility][inheritance]")
{
    // GIVEN a PanelWidget with kPanel role as parent
    auto parent = std::make_unique<RectWidget>();
    parent->set_accessible_role(AccessibleRole::kPanel);

    auto child = std::make_unique<RectWidget>();
    // child has kNone (default)
    auto* child_ptr = child.get();
    parent->add_child(std::move(child));

    // WHEN child effective_role is queried
    // THEN it inherits parent's kPanel
    CHECK(child_ptr->effective_role() == AccessibleRole::kPanel);
}

TEST_CASE("Widget: effective_role returns own role when set",
          "[widget][accessibility][inheritance]")
{
    // GIVEN a parent PanelWidget with child that sets its own role
    auto parent = std::make_unique<RectWidget>();
    parent->set_accessible_role(AccessibleRole::kPanel);

    auto child = std::make_unique<RectWidget>();
    child->set_accessible_role(AccessibleRole::kSlider);
    auto* child_ptr = child.get();
    parent->add_child(std::move(child));

    // WHEN child effective_role is queried
    // THEN it returns kSlider (own role), not kPanel
    CHECK(child_ptr->effective_role() == AccessibleRole::kSlider);
}

TEST_CASE("Widget: effective_role returns kNone when no parent and no role",
          "[widget][accessibility][inheritance]")
{
    RectWidget widget;

    // GIVEN a root widget with no explicit role
    // WHEN effective_role is queried
    // THEN it returns kNone
    CHECK(widget.effective_role() == AccessibleRole::kNone);
}

TEST_CASE("Widget: effective_role walks up parent chain when needed",
          "[widget][accessibility][inheritance]")
{
    // GIVEN grandparent → parent (no role) → child (no role)
    auto gp = std::make_unique<RectWidget>();
    gp->set_accessible_role(AccessibleRole::kPanel);

    auto parent = std::make_unique<RectWidget>();
    auto* parent_ptr = parent.get();
    gp->add_child(std::move(parent));

    auto child = std::make_unique<RectWidget>();
    auto* child_ptr = child.get();
    parent_ptr->add_child(std::move(child));

    // WHEN child effective_role is queried
    // THEN it walks up to grandparent and returns kPanel
    CHECK(child_ptr->effective_role() == AccessibleRole::kPanel);
}

// ═════════════════════════════════════════════════════════════════
// Widget: tab_index
// ═════════════════════════════════════════════════════════════════

TEST_CASE("Widget: tab_index defaults to 0", "[widget][tab]")
{
    RectWidget widget;
    CHECK(widget.tab_index() == 0);
}

TEST_CASE("Widget: tab_index can be set", "[widget][tab]")
{
    RectWidget widget;
    widget.set_tab_index(5);
    CHECK(widget.tab_index() == 5);

    widget.set_tab_index(0);
    CHECK(widget.tab_index() == 0);
}

// ═════════════════════════════════════════════════════════════════
// Widget: focus state
// ═════════════════════════════════════════════════════════════════

TEST_CASE("Widget: is_focused defaults to false", "[widget][focus]")
{
    RectWidget widget;
    CHECK_FALSE(widget.is_focused());
}

TEST_CASE("Widget: set_focused invokes on_focus/on_blur callbacks",
          "[widget][focus]")
{
    RectWidget widget;
    bool focus_fired = false;
    bool blur_fired = false;

    widget.on_focus([&]() { focus_fired = true; });
    widget.on_blur([&]() { blur_fired = true; });

    // WHEN focused
    widget.set_focused(true);
    CHECK(focus_fired);
    CHECK_FALSE(blur_fired);

    // WHEN unfocused
    widget.set_focused(false);
    CHECK(blur_fired);
}

TEST_CASE("Widget: set_focused no-op when same state",
          "[widget][focus]")
{
    RectWidget widget;
    int focus_count = 0;
    int blur_count = 0;

    widget.on_focus([&]() { ++focus_count; });
    widget.on_blur([&]() { ++blur_count; });

    // GIVEN not focused
    // WHEN set_focused(false) — no change
    widget.set_focused(false);
    CHECK(focus_count == 0);
    CHECK(blur_count == 0);

    // WHEN set_focused(true) → focus fires
    widget.set_focused(true);
    CHECK(focus_count == 1);

    // WHEN set_focused(true) again — no change
    widget.set_focused(true);
    CHECK(focus_count == 1);  // Not incremented
}
