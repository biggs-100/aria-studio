// ── Draw Command Type Definitions ───────────────────────────────────
//
// Mirrors the C++ DrawCommandType enum in src/graphics/ffi/command_types.h.
// These types define the JSON contract between TypeScript and C++.

/** Draw command types matching the C++ DrawCommandType enum. */
export type DrawCommandType =
  | 'clear'
  | 'draw_rect'
  | 'draw_circle'
  | 'draw_text'
  | 'flush'
  | 'save'
  | 'restore'
  | 'clip_rect';

/** Parameters for a draw command. Not all fields are used by all types. */
export interface DrawCommandParams {
  // Fill / clear colour (RGBA 0..1)
  r?: number;
  g?: number;
  b?: number;
  a?: number;

  // Stroke colour (RGBA 0..1)
  sr?: number;
  sg?: number;
  sb?: number;
  sa?: number;
  stroke_width?: number;
  corner_radius?: number;

  // Rectangle / clip bounds
  x?: number;
  y?: number;
  w?: number;
  h?: number;

  // Circle
  cx?: number;
  cy?: number;
  radius?: number;

  // Text
  text?: string;
  font_family?: string;
  font_size?: number;
}

/** A single draw command sent to the C++ bridge. */
export interface DrawCommand {
  type: DrawCommandType;
  /** Widget ID this command belongs to (set when dirty-subtree serialization). */
  widget_id?: number;
  r?: number;
  g?: number;
  b?: number;
  a?: number;
  sr?: number;
  sg?: number;
  sb?: number;
  sa?: number;
  stroke_width?: number;
  corner_radius?: number;
  x?: number;
  y?: number;
  w?: number;
  h?: number;
  cx?: number;
  cy?: number;
  radius?: number;
  text?: string;
  font_family?: string;
  font_size?: number;
}

/** Devicer pixel ratio for HiDPI scaling. */
export type DevicePixelRatio = number;

/** Input event types dispatched from C++ to TypeScript. */
export type InputEventType =
  | 'mousemove'
  | 'mousedown'
  | 'mouseup'
  | 'mousedrag'
  | 'keydown'
  | 'keyup';

/** An input event forwarded from the C++ EventDispatcher. */
export interface InputEvent {
  type: InputEventType;
  /** Widget ID from C++ hit-test; maps to component via Bridge.widgetMap. */
  widget_id: number;
  /** Logical pixel X (HiDPI-scaled). */
  x: number;
  /** Logical pixel Y (HiDPI-scaled). */
  y: number;
  /** Mouse button: 0=left, 1=middle, 2=right. */
  button: number;
  /** Platform key code. */
  keyCode: number;
  shift: boolean;
  ctrl: boolean;
  alt: boolean;
  meta: boolean;
}

/** Callback signature for input events from C++. */
export type EventHandler = (event: InputEvent) => void;
