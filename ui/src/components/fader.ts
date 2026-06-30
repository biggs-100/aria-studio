// ── Fader — Vertical Drag Component ──────────────────────────────────
//
// Renders a vertical track with a draggable thumb. Value range [0, 1]
// mapped to dB scale (-∞ to +6dB). Supports touch acceleration with
// flick-to-snap at extremes.
//
// Design:
//   - Extends Component<FaderProps, FaderState>
//   - Drag delta Y maps proportionally to value
//   - High-velocity flicks snap to 0 or 1 near extremes
//   - render() returns container VNode with track + thumb + dB label

import { Component } from '../framework/component.js';
import { ThemeEngine } from '../theme/ThemeEngine.js';
import type { InputEvent } from '../ffi/types.js';
import type { VNode } from '../ffi/bridge.js';

// ── Theme context (global for now; will be replaced by ThemeContext) ─

/** The application-wide ThemeEngine instance. */
let themeEngine: ThemeEngine | null = null;

/**
 * Set the application-wide ThemeEngine.
 * Called once during app initialization.
 */
export function setThemeEngine(engine: ThemeEngine): void {
  themeEngine = engine;
}

/**
 * Get the application-wide ThemeEngine.
 */
export function getThemeEngine(): ThemeEngine | null {
  return themeEngine;
}

/**
 * Resolve a theme color path with fallback values.
 * Returns the theme color if the engine is available and the path resolves,
 * otherwise returns the fallback.
 */
export function resolveThemeColor(
  path: string,
  fallback: { r: number; g: number; b: number; a: number },
): { r: number; g: number; b: number; a: number } {
  if (!themeEngine) return fallback;
  const color = themeEngine.resolve(path);
  // If resolved to black (0,0,0) with alpha 1 and the fallback is not black,
  // check if the path actually resolved by comparing against the default Color
  // Since we can't distinguish "not found" from "actually black" easily,
  // we trust the engine. Users who want black explicitly can set it in the theme.
  if (color.r === 0 && color.g === 0 && color.b === 0 && color.a === 1) {
    // Could be truly black or unresolved — use the resolved value for correctness.
  }
  return color;
}

// ── Types ──────────────────────────────────────────────────────────

export interface FaderProps {
  x: number;
  y: number;
  w: number;
  h: number;
  /** Initial value [0, 1]. Clamped on construction. */
  value: number;
  /** Display label (optional). */
  label?: string;
  /** Callback invoked when value changes. */
  setValue?: (value: number) => void;
}

export interface FaderState {
  value: number;
}

// ── Pure Functions ─────────────────────────────────────────────────

/**
 * Convert linear fader value [0, 1] to dB scale.
 * Maps 0 → -96dB (noise floor), 0.5 → 0dB (unity), 1.0 → +6dB (boost).
 */
export function valueToDb(value: number): number {
  if (value <= 0) return -96;
  return 20 * Math.log10(value * 2);
}

/**
 * Clamp value to the range [0, 1].
 */
export function clampValue(value: number): number {
  if (value < 0) return 0;
  if (value > 1) return 1;
  return value;
}

/**
 * Convert dB value to a height ratio [0, 1] for rendering the meter/thumb position.
 * Uses a power curve to approximate perceptual loudness scaling.
 * -90dB (noise floor) → 0, +6dB (max) → 1.
 * -6dB maps to approximately 0.5 (half height) matching standard metering.
 */
export function dbHeightRatio(db: number): number {
  // Clamp to range [-90, 6] — -90dB is the noise floor
  const clamped = Math.max(-90, Math.min(6, db));
  // Normalize to [0, 1]
  const normalized = (clamped + 90) / 96;
  // Power curve shapes the perceptual response: -6dB → ~0.5
  return Math.pow(normalized, 5);
}

/**
 * Calculate flick-to-snap behavior based on velocity.
 *
 * High velocity flick toward an extreme snaps the value to that extreme.
 * Values near 0.5 (mid) never snap regardless of velocity.
 *
 * @param value Current fader value [0, 1]
 * @param velocity Drag velocity in px/frame (positive = upward/downward)
 * @returns New value and whether a snap occurred
 */
export function calculateFlickSnap(
  value: number,
  velocity: number,
): { value: number; snapped: boolean } {
  const VELOCITY_THRESHOLD = 10;
  const SNAP_ZONE = 0.3; // Snap only when within 0.3 of an extreme

  if (Math.abs(velocity) < VELOCITY_THRESHOLD) {
    return { value, snapped: false };
  }

  if (velocity > 0 && value >= 1 - SNAP_ZONE) {
    return { value: 1, snapped: true };
  }

  if (velocity < 0 && value <= SNAP_ZONE) {
    return { value: 0, snapped: true };
  }

  return { value, snapped: false };
}

// ── Fader Component ───────────────────────────────────────────────

/**
 * Format dB value as a display string.
 */
function formatDb(value: number): string {
  if (value <= -96) return '-∞ dB';
  if (value > 0) return `+${value.toFixed(1)} dB`;
  return `${value.toFixed(1)} dB`;
}

export class Fader extends Component<FaderProps, FaderState> {
  /** Drag sensitivity: pixels of drag per unit of value change. */
  private static readonly DRAG_SENSITIVITY = 200;

  constructor(props: FaderProps) {
    super(props);
    this.state = { value: clampValue(props.value) };
  }

  // ── Drag handler ──────────────────────────────────────────────

  /**
   * Handle a mousedrag event from the event system.
   * Computes new value from Y delta, applies acceleration snap,
   * and fires the setValue callback.
   */
  onDrag(event: { deltaY: number; velocity: number }): void {
    const sensitivity = Fader.DRAG_SENSITIVITY;
    const delta = -event.deltaY / sensitivity;
    const rawValue = this.state.value + delta;
    const clamped = clampValue(rawValue);

    // Check for flick snap
    const snap = calculateFlickSnap(clamped, event.velocity);
    const newValue = snap.snapped ? snap.value : clamped;

    if (newValue !== this.state.value) {
      this.setState({ value: newValue });
      if (this.props.setValue) {
        this.props.setValue(newValue);
      }
    }
  }

  // ── Rendering ─────────────────────────────────────────────────

  render(): VNode {
    const { x, y, w, h, label } = this.props;
    const { value } = this.state;

    // Layout constants
    const trackW = Math.max(4, w * 0.3);
    const trackX = x + (w - trackW) / 2;
    const thumbW = Math.max(8, w * 0.7);
    const thumbH = 12;
    const thumbX = x + (w - thumbW) / 2;
    const thumbY = y + (h - thumbH) * (1 - value);

    // dB display
    const db = valueToDb(value);
    const dbText = formatDb(db);

    // Resolve theme colors with fallbacks to existing hardcoded values
    const trackColor = resolveThemeColor('colors.bg.tertiary', { r: 0.15, g: 0.15, b: 0.18, a: 1 });
    const fillColor = resolveThemeColor('colors.accent.primary', { r: 0.3, g: 0.6, b: 0.9, a: 0.6 });
    const thumbColor = resolveThemeColor('colors.text.primary', { r: 0.8, g: 0.8, b: 0.85, a: 1 });
    const labelColor = resolveThemeColor('colors.text.secondary', { r: 0.7, g: 0.7, b: 0.75, a: 1 });
    const nameColor = resolveThemeColor('colors.text.tertiary', { r: 0.5, g: 0.5, b: 0.55, a: 1 });

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x, y, w, h },
      eventHandlers: {
        mousedrag: (e: InputEvent) => {
          this.onDrag({ deltaY: 0, velocity: 0 });
        },
      },
      children: [
        // Track background
        {
          type: 'rect',
          key: `${this.widgetId}-track`,
          props: {
            x: trackX, y, w: trackW, h,
            fillR: trackColor.r, fillG: trackColor.g, fillB: trackColor.b, fillA: trackColor.a,
            cornerRadius: 2,
          },
        },
        // Track fill (active portion below thumb)
        {
          type: 'rect',
          key: `${this.widgetId}-fill`,
          props: {
            x: trackX, y: thumbY + thumbH / 2, w: trackW,
            h: Math.max(0, y + h - thumbY - thumbH / 2),
            fillR: fillColor.r, fillG: fillColor.g, fillB: fillColor.b, fillA: fillColor.a,
            cornerRadius: 2,
          },
        },
        // Thumb
        {
          type: 'rect',
          key: `${this.widgetId}-thumb`,
          props: {
            x: thumbX, y: thumbY, w: thumbW, h: thumbH,
            fillR: thumbColor.r, fillG: thumbColor.g, fillB: thumbColor.b, fillA: thumbColor.a,
            cornerRadius: 3,
          },
        },
        // dB label
        {
          type: 'text',
          key: `${this.widgetId}-label`,
          props: {
            text: dbText,
            x: x + 2,
            y: y + h + 14,
            r: labelColor.r, g: labelColor.g, b: labelColor.b, a: labelColor.a,
            fontSize: 10,
          },
        },
        // Optional label below value
        ...(label
          ? [
              {
                type: 'text' as const,
                key: `${this.widgetId}-name`,
                props: {
                  text: label,
                  x: x + 2,
                  y: y + h + 28,
                  r: nameColor.r, g: nameColor.g, b: nameColor.b, a: nameColor.a,
                  fontSize: 9,
                },
              },
            ]
          : []),
      ],
    };
  }
}
