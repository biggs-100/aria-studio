# TS Component Framework Specification

## Purpose

Declarative component base class with lifecycle hooks, state management via `setState()` with dirty marking, and dirty-tree scheduling that re-serializes only changed subtrees through the FFI bridge.

## Requirements

### Requirement: Component Lifecycle

Every component SHALL implement constructor, `onMount`, `onUnmount`, and `render`. `render()` SHALL return a `VNode` descriptor consumable by the FFI bridge. `onMount` SHALL fire once after the component is attached to the parent tree. `onUnmount` SHALL fire before removal.

#### Scenario: Full lifecycle sequence

- GIVEN a new component instance
- WHEN it is constructed and mounted into a parent
- THEN `constructor` runs first, followed by `onMount`
- AND `render()` is called after mount, producing a valid VNode

#### Scenario: Unmount fires cleanup

- GIVEN a mounted component with active subscriptions
- WHEN the component is removed from the tree
- THEN `onUnmount` is called before the VNode is discarded

### Requirement: setState with Dirty Marking

`setState(partial)` SHALL merge the partial state into the component's current state, then mark the component and its ancestors dirty. A dirty component SHALL be re-rendered on the next scheduled frame.

#### Scenario: setState triggers dirty subtree

- GIVEN a component with `state = { value: 0 }`
- WHEN `setState({ value: 1 })` is called
- THEN the component's state is updated to `{ value: 1 }`
- AND the component and all ancestors up to root are marked dirty

#### Scenario: No-op setState

- GIVEN a component with `state = { value: 5 }`
- WHEN `setState({ value: 5 })` is called with the same value
- THEN the component MAY skip dirty marking if a shallow equality check passes

### Requirement: Dirty-Tree Scheduling

The reconciler SHALL schedule a `requestAnimationFrame` callback on any dirty marking. During the callback, it SHALL collect all dirty subtrees and serialize only those branches via the FFI bridge, leaving clean subtrees untouched.

#### Scenario: Single dirty branch re-serialized

- GIVEN a tree of 10 components where only component #3 is dirty
- WHEN the scheduled frame fires
- THEN only the path from root to component #3 is traversed for serialization
- AND commands for clean branches are not regenerated

#### Scenario: Multiple dirty markers coalesced

- GIVEN two components marked dirty in the same microtask
- WHEN the next rAF fires
- THEN both dirty subtrees are collected and serialized in a single flush pass
