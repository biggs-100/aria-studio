# TS Docking System Specification

## Purpose

Full-featured docking system: DockManager with a layout tree, DockSplitPanel with draggable dividers, DockTabBar with reorderable and closable tabs, and DockFloatingWindow. Layout state SHALL serialize to and deserialize from JSON.

## Requirements

### Requirement: DockManager (Root Container)

The DockManager SHALL own the root layout tree. It SHALL accept panel registration by type string with a factory function. Panels SHALL be opened at a location, moved between containers, and closed via their component ID.

#### Scenario: Open panel at location

- GIVEN a DockManager with an empty root
- WHEN `openPanel('mixer', { type: 'right' })` is called
- THEN a new split container appears with the mixer panel on the right side at default 50% ratio

#### Scenario: Close removes panel from tree

- GIVEN a mixer panel displayed in a tab group
- WHEN `closePanel('mixer')` is called
- THEN the mixer component is unmounted and removed from the layout tree

### Requirement: DockSplitPanel (Drag Resize)

DockSplitPanel SHALL render a divider between two child panels. Dragging the divider SHALL resize adjacent panels. The ratio SHALL persist and be available in serialized layout.

#### Scenario: Divider drag resizes panels

- GIVEN a horizontal split at 50% ratio
- WHEN the divider is dragged 50px left
- THEN the left panel shrinks by 50px and the right panel grows by 50px

#### Scenario: Divider at min size

- GIVEN a split with a panel at min width 100px
- WHEN the divider is dragged to make that panel < 100px
- THEN the divider SHALL NOT pass the minimum-size boundary

### Requirement: DockTabBar (Drag Reorder)

DockTabBar SHALL render tab headers for each open panel in a group. Tabs SHALL be reorderable by drag. Each tab SHALL have a close button. Dragging a tab outside the bar SHALL create a DockFloatingWindow.

#### Scenario: Tab reorder by drag

- GIVEN a tab bar with [Mixer, Browser, Arrangement]
- WHEN the Browser tab is dragged left past Mixer
- THEN the new order is [Browser, Mixer, Arrangement]

#### Scenario: Tab detached to float

- GIVEN a tab bar with [Mixer, Browser]
- WHEN the Browser tab is dragged 50px outside the tab bar
- THEN a DockFloatingWindow is created containing the Browser panel

### Requirement: JSON Layout Persistence

DockManager SHALL serialize the full layout tree to JSON and restore from JSON. Serialized state SHALL include panel types, split ratios, tab order, and floating window positions.

#### Scenario: Round-trip serialization

- GIVEN a dock layout with 2 splits, 3 tabs, and 1 floating window
- WHEN `saveLayout()` produces JSON, then `loadLayout(json)` restores it
- THEN the restored layout matches the original panel arrangement and positions

#### Scenario: Missing panel type on restore

- GIVEN a JSON layout referencing an unregistered panel type
- WHEN `loadLayout(json)` is called
- THEN the missing panel SHALL be replaced with a placeholder component
- AND no panels in the layout are orphaned
