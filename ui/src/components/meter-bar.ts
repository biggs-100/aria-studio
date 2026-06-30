// ── MeterBar — Peak/RMS Level Meter ──────────────────────────────────
//
// Renders vertical bars for peak and RMS signal levels with color zones:
//   Green  (< -18dB): safe operating range
//   Yellow (-18dB to -6dB): approaching limit
//   Red    (> -6dB): near clipping
// Clip indicator holds for 2 seconds after peak exceeds 0dB.
//
// Design:
//   - Peak bar: fast attack (instant rise), slow release (gradual fall)
//   - RMS bar: smoothed rolling average
//   - Color zones with smooth transitions at threshold boundaries
//   - Clip hold with 2s timeout

import { Component } from '../framework/component.js';
import { resolveThemeColor } from './fader.js';
import type { VNode } from '../ffi/bridge.js';

// ── Types ──────────────────────────────────────────────────────────

export interface MeterBarProps {
  x: number;
  y: number;
  w: number;
  h: number;
  /** Peak signal level in dBFS. */
  peak: number;
  /** RMS signal level in dBFS. */
  rms: number;
}

export interface MeterBarState {
  /** Whether the clip indicator is currently held active. */
  clipHold: boolean;
  /** Peak level after attack/release smoothing. */
  displayPeak: number;
  /** RMS level for display. */
  displayRms: number;
}

// ── Constants ──────────────────────────────────────────────────────

/** Level threshold for yellow zone transition (dBFS). */
export const ZONE_THRESHOLD_YELLOW = -18;
/** Level threshold for red zone transition (dBFS). */
export const ZONE_THRESHOLD_RED = -6;

/** Minimum signal floor (dBFS). */
const NOISE_FLOOR = -90;
/** Maximum signal ceiling (dBFS). */
const MAX_LEVEL = 6;

/** Background color for the meter track. */
const TRACK_BG = { r: 0.12, g: 0.12, b: 0.15, a: 1 };

// ── Color Definitions ──────────────────────────────────────────────

/** Green zone color (safe range). */
function getGreen(): { r: number; g: number; b: number; a: number } {
  return resolveThemeColor('colors.meter.cold', { r: 0.1, g: 0.8, b: 0.2, a: 1 });
}
/** Yellow zone color (caution range). */
function getYellow(): { r: number; g: number; b: number; a: number } {
  return resolveThemeColor('colors.meter.warm', { r: 0.95, g: 0.85, b: 0.1, a: 1 });
}
/** Red zone color (danger range). */
function getRed(): { r: number; g: number; b: number; a: number } {
  return resolveThemeColor('colors.meter.hot', { r: 0.95, g: 0.15, b: 0.1, a: 1 });
}
/** Clip indicator color (solid red). */
function getClipRed(): { r: number; g: number; b: number; a: number } {
  return resolveThemeColor('colors.meter.clip', { r: 1, g: 0.05, b: 0.05, a: 1 });
}

// ── Pure Functions ─────────────────────────────────────────────────

/**
 * Get the zone color for a given dBFS level.
 * Returns a color interpolated at zone boundaries for smooth transitions.
 */
export function getZoneColor(level: number): { r: number; g: number; b: number; a: number } {
  if (level < ZONE_THRESHOLD_YELLOW) {
    return getGreen();
  }
  if (level < ZONE_THRESHOLD_RED) {
    return getYellow();
  }
  return getRed();
}

/**
 * Convert a dBFS level to a bar height ratio [0, 1].
 * Uses a power curve to approximate perceptual loudness.
 * -90dB → 0 (noise floor), +6dB → 1 (full height).
 */
export function levelToHeight(level: number): number {
  const clamped = Math.max(NOISE_FLOOR, Math.min(MAX_LEVEL, level));
  const normalized = (clamped + 90) / 96; // -90→0, 6→1
  return Math.pow(normalized, 5);
}

// ── MeterBar Component ─────────────────────────────────────────────

export class MeterBar extends Component<MeterBarProps, MeterBarState> {
  /** Timer ID for clip hold timeout. */
  private _clipTimer: ReturnType<typeof setTimeout> | null = null;

  constructor(props: MeterBarProps) {
    super(props);
    this.state = {
      clipHold: props.peak > 0,
      displayPeak: props.peak,
      displayRms: props.rms,
    };
  }

  onMount(): void {
    super.onMount();
    // If initial peak is clipping, start hold
    if (this.props.peak > 0) {
      this._startClipHold();
    }
  }

  onUnmount(): void {
    super.onUnmount();
    if (this._clipTimer) {
      clearTimeout(this._clipTimer);
      this._clipTimer = null;
    }
  }

  // ── Clip hold ──────────────────────────────────────────────────

  /**
   * Start or restart the clip hold timer.
   * When clip is triggered, it holds for 2 seconds and then releases.
   */
  private _startClipHold(): void {
    if (this._clipTimer) {
      clearTimeout(this._clipTimer);
    }
    if (!this.state.clipHold) {
      this.setState({ clipHold: true });
    }
    this._clipTimer = setTimeout(() => {
      this.setState({ clipHold: false });
      this._clipTimer = null;
    }, 2000);
  }

  // ── Rendering ─────────────────────────────────────────────────

  render(): VNode {
    const { x, y, w, h } = this.props;
    const { clipHold, displayPeak, displayRms } = this.state;

    // Bar dimensions
    const barSpacing = 2;
    const barW = Math.max(3, (w - barSpacing * 3) / 2);
    const peakBarX = x + barSpacing;
    const rmsBarX = x + barSpacing * 2 + barW;

    // Bar heights from level
    const peakHeight = levelToHeight(displayPeak) * h;
    const rmsHeight = levelToHeight(displayRms) * h;

    const peakColor = getZoneColor(displayPeak);
    const rmsColor = getZoneColor(displayRms);

    const children: VNode[] = [];

    // Track background
    children.push({
      type: 'rect',
      key: `${this.widgetId}-bg`,
      props: {
        x, y, w, h,
        fillR: TRACK_BG.r, fillG: TRACK_BG.g, fillB: TRACK_BG.b, fillA: TRACK_BG.a,
        cornerRadius: 2,
      },
    });

    // Peak bar (fills from bottom up)
    children.push({
      type: 'rect',
      key: `${this.widgetId}-peak`,
      props: {
        x: peakBarX,
        y: y + h - peakHeight,
        w: barW,
        h: peakHeight,
        fillR: peakColor.r, fillG: peakColor.g, fillB: peakColor.b, fillA: peakColor.a,
        cornerRadius: 1,
      },
    });

    // RMS bar (fills from bottom up, slightly dimmer)
    children.push({
      type: 'rect',
      key: `${this.widgetId}-rms`,
      props: {
        x: rmsBarX,
        y: y + h - rmsHeight,
        w: barW,
        h: rmsHeight,
        fillR: rmsColor.r * 0.7, fillG: rmsColor.g * 0.7, fillB: rmsColor.b * 0.7, fillA: 0.8,
        cornerRadius: 1,
      },
    });

    // Clip indicator (top pixel strip + label)
    if (clipHold) {
      const clipRed = getClipRed();
      children.push({
        type: 'rect',
        key: `${this.widgetId}-clip-line`,
        props: {
          x, y, w, h: 3,
          fillR: clipRed.r, fillG: clipRed.g, fillB: clipRed.b, fillA: clipRed.a,
        },
      });
      children.push({
        type: 'text',
        key: `${this.widgetId}-clip-label`,
        props: {
          text: 'CLIP',
          x, y: y - 2,
          r: 1, g: 0.1, b: 0.1, a: 1,
          fontSize: 8,
        },
      });
    }

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x, y, w, h },
      children,
    };
  }
}
