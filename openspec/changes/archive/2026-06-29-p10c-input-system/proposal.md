# Proposal: P10c Input System

## Intent

Complete the P10a input foundation with focus management, keyboard shortcuts, touch/pen dispatch, and accessibility metadata. P10a delivered mouse/key event dispatch — focus order, shortcut registration, touch, pen, and a11y are all missing.

## Scope

### In Scope
- FocusManager (tab order, focus/blur events, focus ring, programmatic focus)
- ShortcutManager (register, dispatch, priority-based conflict resolution)
- Touch/pen event types in EventDispatcher
- Accessibility metadata on Widget base (name, role, description)
- keyboard navigation (Tab, Enter, Escape, Arrow keys on interactive widgets)

### Out of Scope
- Screen reader integration / platform a11y APIs
- Gesture recognition (swipe, pinch-zoom)
- Voice control

## Capabilities

### New Capabilities
- `input-focus-shortcut`: FocusManager + ShortcutManager
- `input-event-extensions`: Touch/pen event types, accessibility metadata on Widget

### Modified Capabilities
- `gpu-widget-system`: Extended Widget base with accessibility fields and focus state
- `render-loop`: Focus/blur events dispatched as part of input processing phase

## Approach

FocusManager and ShortcutManager as ServiceLocator services. FocusManager maintains a Z-order tab list, dispatches focus/blur events, renders focus ring via SkiaCanvas. ShortcutManager stores key chords, uses priority (focused widget first, then propagate). Touch/pen extends EventDispatcher's EventType enum and dispatch methods. Widget base gets `accessible_name`, `accessible_role`, `tab_order` fields.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/graphics/focus_manager.h/.cc` | New | Focus management, tab order, focus ring |
| `src/graphics/shortcut_manager.h/.cc` | New | Shortcut registration and dispatch |
| `src/graphics/input_types.h` | New | TouchEvent, PenEvent structs |
| `src/graphics/ffi/event_dispatcher.h/.cc` | Modified | Touch/pen dispatch, new event types |
| `src/graphics/widget.h` | Modified | accessibility fields, focus state, tab_order |
| `src/graphics/render_loop.cc` | Modified | Focus event dispatch in input phase |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Shortcut conflicts between components | Low | Priority-based: focused component resolves first, then propagate up |
| Tab order breaks with dynamic widget trees | Low | Default to insertion order; explicit `tab_index` override available |

## Rollback Plan

Revert FocusManager/ShortcutManager addition. Widget base extension (accessibility fields) is additive + default-initialized — safe to keep. EventDispatcher extension reverts cleanly.

## Dependencies

- EventDispatcher (existing, P10a)
- Widget tree / hit-testing (existing, P10a)
- ServiceLocator (existing, P0)

## Success Criteria

- [ ] Tab key navigates focus through widgets in declared order
- [ ] Focus/blur events fire and update widget visual state
- [ ] ShortcutManager registers chords, dispatches to focused widget first
- [ ] Conflict detection warns on duplicate registration
- [ ] Touch/pen events dispatch through EventDispatcher
- [ ] Widgets carry accessible metadata consumed by focus ring
