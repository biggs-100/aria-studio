# Input Focus and Shortcut Specification

## Purpose

Provide focus management (tab order, focus/blur events, focus ring) and keyboard shortcut registration and dispatch with conflict resolution — completing the input system for keyboard-driven workflows.

## Requirements

### Requirement: FocusManager

The `FocusManager` SHALL maintain a focus-ordered list of widgets derived from their `tab_order` property and insertion order. `FocusManager::focus_next()` and `FocusManager::focus_previous()` SHALL move focus forward/backward through the list, dispatching `kFocusLost` on the previously focused widget and `kFocusGained` on the newly focused widget. `FocusManager::set_focus(WidgetID)` SHALL programmatically focus a specific widget. The focus ring SHALL be rendered as a themed outline on the focused widget during the paint pass.

#### Scenario: Tab navigates forward through focusable widgets

- GIVEN three focusable widgets A, B, C with tab_order 1, 2, 3, and A currently focused
- WHEN `focus_next()` is called
- THEN B receives `kFocusGained` event
- AND A receives `kFocusLost` event
- AND `focused_widget()` returns B's WidgetID

#### Scenario: Tab wraps around at end of list

- GIVEN focusable widgets A, B and B is focused
- WHEN `focus_next()` is called
- THEN focus wraps to A
- AND A receives `kFocusGained`

#### Scenario: Programmatic focus sets a specific widget

- GIVEN widget B is currently focused
- WHEN `set_focus(C_id)` is called for widget C
- THEN C receives `kFocusGained`
- AND B receives `kFocusLost`
- AND `focused_widget()` returns C_id

#### Scenario: Non-focusable widgets are skipped

- GIVEN widgets A (focusable), B (non-focusable, enabled=false), C (focusable) in order
- WHEN `focus_next()` is called with A focused
- THEN C receives focus
- AND B receives no focus event

#### Scenario: Focus ring renders on focused widget

- GIVEN widget A is focused and visible
- WHEN the paint pass renders
- THEN a focus ring (themed outline) is drawn around A's bounds
- AND no focus ring is drawn for non-focused widgets

### Requirement: ShortcutManager

The `ShortcutManager` SHALL support registration of keyboard shortcuts via `register(KeyChord, callback, priority)`. `KeyChord` SHALL combine a key + modifiers (Ctrl, Shift, Alt, Meta). `dispatch(KeyChord)` SHALL invoke the callback of the highest-priority registered handler. If the focused widget's registered handler does not consume the shortcut (returns false), the manager SHALL propagate to the next highest priority. `conflicts(KeyChord)` SHALL return all handlers registered for the same chord. Duplicate registration SHALL log a warning.

#### Scenario: Dispatch fires the highest-priority handler

- GIVEN KeyChord `Ctrl+S` registered by A (priority 10) and B (priority 5)
- WHEN `dispatch(Ctrl+S)` is called
- THEN A's callback SHALL be invoked
- AND B's callback SHALL NOT be invoked

#### Scenario: Unconsumed shortcut propagates

- GIVEN `Ctrl+D` registered by focused widget F (priority 100, returns false) and global G (priority 10, returns true)
- WHEN `dispatch(Ctrl+D)` is called
- THEN F's callback SHALL be invoked first (returns false)
- AND G's callback SHALL be invoked next (returns true)

#### Scenario: Unregistered shortcut is silently ignored

- GIVEN no handler registered for `Alt+F4`
- WHEN `dispatch(Alt+F4)` is called
- THEN no callback SHALL be invoked
- AND no error SHALL be raised

#### Scenario: Conflict detection on duplicate registration

- GIVEN `Ctrl+Z` already registered by handler X
- WHEN handler Y registers `Ctrl+Z`
- THEN a warning SHALL be logged
- AND `conflicts(Ctrl+Z)` SHALL return both X and Y
