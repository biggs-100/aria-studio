// ── Command Builder Helpers ─────────────────────────────────────────
//
// Pure functions that construct DrawCommand objects for each draw type.
// These mirror the C++ dispatch logic in bridge.cc.

import type { DrawCommand } from './types.js';

// ── Colour helper ─────────────────────────────────────────────────

/** RGBA colour represented as 0..1 floats. */
export interface Color {
  r: number;
  g: number;
  b: number;
  a: number;
}

// ── State commands ─────────────────────────────────────────────────

/** Create a clear command that fills the entire surface with a colour. */
export function clear(color: Color): DrawCommand {
  return {
    type: 'clear',
    r: color.r,
    g: color.g,
    b: color.b,
    a: color.a,
  };
}

/** Create a save command (push canvas state). */
export function save(): DrawCommand {
  return { type: 'save' };
}

/** Create a restore command (pop canvas state). */
export function restore(): DrawCommand {
  return { type: 'restore' };
}

/** Create a flush command (submit GPU commands). */
export function flush(): DrawCommand {
  return { type: 'flush' };
}

// ── Draw commands ──────────────────────────────────────────────────

/** Rectangle fill/stroke options. */
export interface RectParams {
  x: number;
  y: number;
  w: number;
  h: number;
  fill?: Color;
  stroke?: Color;
  strokeWidth?: number;
  cornerRadius?: number;
}

/** Create a draw_rect command. */
export function drawRect(params: RectParams): DrawCommand {
  const cmd: DrawCommand = {
    type: 'draw_rect',
    x: params.x,
    y: params.y,
    w: params.w,
    h: params.h,
  };

  if (params.fill) {
    cmd.r = params.fill.r;
    cmd.g = params.fill.g;
    cmd.b = params.fill.b;
    cmd.a = params.fill.a;
  }

  if (params.stroke) {
    cmd.sr = params.stroke.r;
    cmd.sg = params.stroke.g;
    cmd.sb = params.stroke.b;
    cmd.sa = params.stroke.a;
  }

  if (params.strokeWidth !== undefined) {
    cmd.stroke_width = params.strokeWidth;
  }

  if (params.cornerRadius !== undefined) {
    cmd.corner_radius = params.cornerRadius;
  }

  return cmd;
}

/** Circle draw options. */
export interface CircleParams {
  cx: number;
  cy: number;
  radius: number;
  fill?: Color;
  stroke?: Color;
  strokeWidth?: number;
}

/** Create a draw_circle command. */
export function drawCircle(params: CircleParams): DrawCommand {
  const cmd: DrawCommand = {
    type: 'draw_circle',
    cx: params.cx,
    cy: params.cy,
    radius: params.radius,
  };

  if (params.fill) {
    cmd.r = params.fill.r;
    cmd.g = params.fill.g;
    cmd.b = params.fill.b;
    cmd.a = params.fill.a;
  }

  if (params.stroke) {
    cmd.sr = params.stroke.r;
    cmd.sg = params.stroke.g;
    cmd.sb = params.stroke.b;
    cmd.sa = params.stroke.a;
  }

  if (params.strokeWidth !== undefined) {
    cmd.stroke_width = params.strokeWidth;
  }

  return cmd;
}

/** Text draw options. */
export interface TextParams {
  text: string;
  x: number;
  y: number;
  color?: Color;
  fontFamily?: string;
  fontSize?: number;
}

/** Create a draw_text command. */
export function drawText(params: TextParams): DrawCommand {
  const cmd: DrawCommand = {
    type: 'draw_text',
    text: params.text,
    x: params.x,
    y: params.y,
  };

  if (params.color) {
    cmd.r = params.color.r;
    cmd.g = params.color.g;
    cmd.b = params.color.b;
    cmd.a = params.color.a;
  }

  if (params.fontFamily !== undefined) {
    cmd.font_family = params.fontFamily;
  }

  if (params.fontSize !== undefined) {
    cmd.font_size = params.fontSize;
  }

  return cmd;
}

/** Clip rect options. */
export interface ClipRectParams {
  x: number;
  y: number;
  w: number;
  h: number;
}

/** Create a clip_rect command. */
export function clipRect(params: ClipRectParams): DrawCommand {
  return {
    type: 'clip_rect',
    x: params.x,
    y: params.y,
    w: params.w,
    h: params.h,
  };
}
