# Script UI Specification

## Purpose

Allow Lua scripts to create floating UI windows with interactive widgets (buttons, labels, sliders, text inputs) and bind Lua callbacks to widget events.

## Requirements

### Requirement: Window Creation

The system MUST allow scripts to create floating UI windows via `aria.ui.window()`.

#### Scenario: Create floating window

- GIVEN A script is running
- WHEN `aria.ui.window("My Script", 400, 300)` is called
- THEN A floating window with title "My Script", width 400, and height 300 appears on screen

#### Scenario: Multiple windows

- GIVEN A script creates two window instances
- WHEN Both are created
- THEN Two separate floating windows are displayed independently

#### Scenario: Invalid dimensions

- GIVEN A script passes width or height ≤ 0
- WHEN `aria.ui.window("Bad", -100, 300)` is called
- THEN An error is returned; no window is created

#### Scenario: Empty title

- GIVEN A script passes an empty string as title
- WHEN `aria.ui.window("", 400, 300)` is called
- THEN An error is returned; no window is created

### Requirement: Widget Creation

The system MUST allow scripts to add standard widgets to script-owned windows.

#### Scenario: Add button

- GIVEN A window object exists
- WHEN `window:button("Click Me")` is called
- THEN A button with label "Click Me" is added to the window

#### Scenario: Add labeled slider

- GIVEN A window object exists
- WHEN `window:slider("Volume", 0, 100, 50)` is called
- THEN A slider labeled "Volume" with range 0–100 and default value 50 is added

#### Scenario: Add text input

- GIVEN A window object exists
- WHEN `window:text_input("Track Name", "default")` is called
- THEN A text input labeled "Track Name" prefilled with "default" is added

#### Scenario: Unsupported widget type

- GIVEN A script requests an unsupported widget type
- WHEN `window:widget("chart", {})` is called
- THEN An error is returned; no widget is created

### Requirement: Event Binding

The system MUST allow scripts to bind Lua callback functions to widget events.

#### Scenario: Button click fires callback

- GIVEN A button exists with a bound `on_click` callback
- WHEN The button is clicked by the user
- THEN The Lua callback function is invoked

#### Scenario: Slider value change fires callback

- GIVEN A slider exists with a bound `on_change` callback
- WHEN The user drags the slider to a new value
- THEN The callback fires with the new value as argument

#### Scenario: Callback throws Lua error

- GIVEN A button callback contains a runtime error
- WHEN The button is clicked
- THEN The error is caught and logged; the window remains open and functional

### Requirement: Auto-Cleanup

The system MUST destroy all script-created windows when the owning script is unloaded.

#### Scenario: Windows destroyed on unload

- GIVEN A script has 3 open windows
- WHEN `ScriptManager::unload(scriptId)` is called
- THEN All 3 windows are closed and their graphics resources freed

#### Scenario: Windows destroyed on script error

- GIVEN A script throws an unhandled Lua error
- WHEN The error terminates the script
- THEN All windows owned by that script are closed
