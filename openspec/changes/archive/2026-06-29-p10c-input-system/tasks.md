# Tasks: P10c Input System

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 250–350 |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | single-pr |
| Chain strategy | pending |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

## Phase 1: Foundation Types

- [x] 1.1 Create `src/graphics/input_types.h` — `TouchEvent`, `PenEvent` structs + touch/pen enum values for `EventType`
- [x] 1.2 Add `tab_index`, `accessible_name`, `accessible_role`, `accessible_description`, and `focus_state` fields to `src/graphics/widget.h`; implement `effective_role()` inheritance
- [x] 1.3 Default-init new widget fields in `src/graphics/widget.cc`

## Phase 2: FocusManager

- [x] 2.1 Create `src/graphics/focus_manager.h`/`.cc` — tab order list from widget tree, `focus_next()`/`focus_previous()`/`set_focus()`, focus/blur event dispatch, focus ring render via `SkiaCanvas`

## Phase 3: ShortcutManager

- [x] 3.1 Create `src/graphics/shortcut_manager.h`/`.cc` — `register_shortcut(KeyChord, handler, priority)`, `dispatch(KeyChord)`, `conflicts()`, duplicate warning logging

## Phase 4: EventDispatcher Extension

- [x] 4.1 Extend `src/graphics/ffi/event_dispatcher.h` — add `kTouchDown/kTouchMove/kTouchUp/kPenDown/kPenMove/kPenUp` to `EventType`, add `is_touch_or_pen` flag to `InputEvent`
- [x] 4.2 Implement touch/pen dispatch methods in `src/graphics/ffi/event_dispatcher.cc`

## Phase 5: RenderLoop Integration

- [x] 5.1 Modify `src/graphics/render_loop.cc` — call `FocusManager` and `ShortcutManager` during input phase, render focus ring after paint pass

## Phase 6: Testing

- [x] 6.1 Unit test: `FocusManager` tab navigation with 3 widgets, wrap-around, non-focusable skip
- [x] 6.2 Unit test: `FocusManager` programmatic `set_focus()` dispatches focus/blur events
- [x] 6.3 Unit test: `ShortcutManager` priority dispatch + propagation on false return + conflict warning
- [x] 6.4 Unit test: `TouchEvent` dispatch through `EventDispatcher` with correct hit-test and coords
- [x] 6.5 Unit test: `effective_role()` inherits parent role when `kNone`, uses own when set
