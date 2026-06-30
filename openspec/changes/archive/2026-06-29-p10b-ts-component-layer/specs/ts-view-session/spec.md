# TS View Session Specification

## Purpose

Port of the Canvas 2D SessionView to the GPU Component tree. Clip slot grid, scene launch buttons, crossfader, and follow action indicators SHALL render via the FFI bridge. Zero Canvas 2D calls SHALL remain after port.

## Requirements

### Requirement: Clip Slot Grid

The session grid SHALL render as a 2D component layout where columns are tracks and rows are scenes. Each cell SHALL show clip name, color, and playing state. Virtual culling SHALL skip cells outside the viewport.

#### Scenario: Grid renders visible cells only

- GIVEN 32 tracks × 64 scenes, viewport showing rows 0-12 and cols 0-6
- WHEN the frame renders
- THEN only (13×7)=91 cells produce draw commands via the FFI bridge

#### Scenario: Playing clip shows green indicator

- GIVEN a clip slot in 'Playing' state
- WHEN its cell component renders
- THEN the cell background is highlighted green and a green dot appears at the top-right

### Requirement: Scene Launch Buttons

Each scene row SHALL have a launch button in the scene strip column. Clicking the button SHALL dispatch a scene launch via the data source. The button SHALL show a flash animation on trigger.

#### Scenario: Launch button press fires scene

- GIVEN a scene at row index 5
- WHEN the launch button component is clicked
- THEN `dataSource.launchScene(sceneId)` is called with the correct SceneID

#### Scenario: Triggered scene flash

- GIVEN a scene with a slot in 'Triggered' state
- WHEN the frame renders during the trigger quantize window
- THEN the launch button renders with an amber flash overlay

### Requirement: Crossfader Widget

The crossfader SHALL render as a horizontal track with a draggable thumb. Position values SHALL be [-1, +1]. Dragging SHALL call `dataSource.setCrossfaderPosition`. A/B labels SHALL render at track ends.

#### Scenario: Crossfader drag updates position

- GIVEN crossfader at position 0 (center)
- WHEN the thumb is dragged 40px right
- THEN the new position computes to approximately +0.5
- AND `setCrossfaderPosition(0.5)` is dispatched

### Requirement: Follow Action Indicators

Cells with follow actions SHALL render an icon or label indicating the configured action (Stop, PlayNext, PlayRandom, ContinueAsLoop). The indicator SHALL be visible without hovering.

#### Scenario: Follow action icon displayed

- GIVEN a clip slot with follow_action='PlayNext'
- WHEN the cell renders
- THEN a small right-arrow icon is drawn at the bottom-right corner of the cell

#### Scenario: No follow action hides indicator

- GIVEN a clip slot with follow_action='None'
- WHEN the cell renders
- THEN no follow action icon is drawn
