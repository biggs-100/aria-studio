# GPU Widget System Specification

## Purpose

Provide a base widget tree in C++ with layout, paint, hit-testing, clipping, and transform — the foundation for all GPU-rendered UI panels.

## Requirements

### Requirement: Widget Base Class

Every widget SHALL inherit from a `Widget` base class providing identity (`WidgetID`, `type()`), geometry (`bounds`, `position`, `size`), visual state (`visible`, `opacity`), and interaction state (`enabled`, `hovered`, `pressed`).

#### Scenario: Widget creation with default state

- GIVEN a new RectWidget
- WHEN constructed with no arguments
- THEN `is_visible()` returns true
- AND `opacity()` returns 1.0
- AND `bounds()` returns an empty Rect at (0,0)

### Requirement: Parent/Child Hierarchy

Widgets SHALL form a tree via `set_parent()`, `add_child()`, and `remove_child()`. A widget MUST have at most one parent. Removing a child SHALL transfer ownership.

#### Scenario: Adding and removing children

- GIVEN a parent widget P
- WHEN child C is added via `P.add_child(C)`
- THEN `C.parent()` returns P
- AND `P.children()` contains C

#### Scenario: Re-parenting a widget

- GIVEN widget C with parent P1
- WHEN `P2.add_child(C)` is called
- THEN C is removed from P1's children
- AND C.parent() returns P2

### Requirement: Layout Pass

The system SHALL perform a two-phase layout: measure (determine preferred size) then arrange (assign final bounds). `LayoutEngine::compute(root, viewport)` SHALL traverse the full tree.

#### Scenario: Flex row layout

- GIVEN a FlexRow container with child A (flex: 1) and child B (flex: 2)
- WHEN `LayoutEngine::compute(root, (300, 100))` is called
- THEN A.bounds().width is 100
- AND B.bounds().width is 200

### Requirement: Paint Pass

The system SHALL traverse the widget tree depth-first, calling `Widget::render(SkiaCanvas*)` on each visible widget. Clipping SHALL be applied per widget.

#### Scenario: Paint pass excludes invisible widgets

- GIVEN a parent with child C where `C.set_visible(false)`
- WHEN a paint pass runs
- THEN `C.render()` is never called
- AND the canvas receives draw calls only for visible widgets

### Requirement: Hit-Testing

The system SHALL resolve a screen point to the frontmost widget using depth-first reverse traversal. `hit_test(x, y)` SHALL return the deepest visible, enabled widget containing the point.

#### Scenario: Point hits overlapping widgets

- GIVEN sibling widgets A and B where A is drawn after B (on top)
- WHEN hit_test(150, 200) returns a hit for both
- THEN A is returned (topmost)

#### Scenario: Point hits no widget

- GIVEN a tree of widgets at positions not covering (0, 0)
- WHEN hit_test(0, 0) is called
- THEN nullptr is returned

### Requirement: Clipping

Child content SHALL be clipped to the parent's bounds. The system SHALL push a clip rect via `SkiaCanvas::save()/restore()` before rendering each container.

#### Scenario: Child extends beyond parent bounds

- GIVEN a parent at (0, 0, 100, 100) with child at (50, 50, 200, 200)
- WHEN the paint pass renders the child
- THEN pixels outside (0, 0, 100, 100) are clipped
- AND only the visible 50x50 intersection is rendered

### Requirement: Transform Stack

Each widget MAY have an affine transform applied via `set_transform()`. The system SHALL push the transform onto the SkiaCanvas stack before rendering the widget and its children.

#### Scenario: Scaled widget with child

- GIVEN a parent with scale(2.0) transform and a child at (10, 10)
- WHEN the paint pass renders the child
- THEN the child is rendered at logical position (20, 20)
- AND child size is doubled on screen
