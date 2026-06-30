# TypeScript ↔ C++ FFI Bridge Specification

## Purpose

Bridge TypeScript component declarations to C++ GPU draw commands and route input events back to TypeScript handlers.

## Requirements

### Requirement: Component Tree Serialization

The TypeScript UI SHALL serialize the virtual component tree into a flat command buffer. Each node SHALL encode its type, bounds, paint style, and child references. Serialization SHALL produce a sequence of draw commands consumable by C++.

#### Scenario: Single button serialization

- GIVEN a Button component with bounds [10,10,100,40] and "Click me" text
- WHEN `bridge.serialize(component)` is called
- THEN a flat buffer is produced with one Rect command (bounds + fill color) and one TextBlob command (text string + font)

#### Scenario: Empty component tree

- GIVEN a root component with zero children
- WHEN serialization completes
- THEN the buffer contains only the root container command
- AND the draw count is 1

### Requirement: SkiaCanvas Command Buffer

The C++ bridge SHALL receive the serialized buffer and execute each command against the active SkiaCanvas. Command types SHALL include: `DrawRect`, `DrawTextBlob`, `DrawImage`, `SetClip`, `PushTransform`, `PopTransform`.

#### Scenario: Command buffer execution

- GIVEN a serialized buffer with 3 draw commands
- WHEN `bridge.execute(buffer)` is called in C++
- THEN `SkiaCanvas::drawRect` is called 3 times in order
- AND `canvas->flush()` is called after all commands execute

#### Scenario: Empty command buffer

- GIVEN a buffer with zero commands
- WHEN `bridge.execute(buffer)` is called
- THEN no SkiaCanvas calls are made
- AND execution returns immediately with success

### Requirement: Event Dispatch

Mouse, keyboard, and touch events from the OS SHALL be forwarded from C++ to TypeScript. The bridge SHALL encode event type, position (logical pixels), button state, and modifiers. TypeScript handlers SHALL receive events via a registered callback.

#### Scenario: Mouse click dispatched to TS

- GIVEN a mouse click at screen position (200, 150)
- WHEN the event reaches the C++ bridge
- THEN a `MouseDown` event struct is serialized with x=200, y=150, button=Left
- AND the TypeScript `onMouseDown` handler is called

#### Scenario: No registered handler

- GIVEN no registered event handler in TypeScript
- WHEN a keyboard event arrives at the bridge
- THEN the event is silently dropped
- AND no crash or memory leak occurs

### Requirement: HiDPI Scaling

The bridge SHALL apply display scale factor to convert physical pixels to logical pixels. Mouse coordinates SHALL be divided by the scale factor before dispatch. Draw commands from TypeScript SHALL be multiplied by the scale factor before Skia execution.

#### Scenario: HiDPI coordinate transform

- GIVEN a 200% scale display (scale factor 2.0)
- WHEN a mouse event arrives at physical (400, 300)
- THEN TypeScript receives logical (200, 150)
- AND draw commands at logical (100, 100) render at physical (200, 200)

#### Scenario: 100% scale passthrough

- GIVEN a 100% scale display (scale factor 1.0)
- WHEN any event or draw command passes through the bridge
- THEN coordinates are passed unchanged
