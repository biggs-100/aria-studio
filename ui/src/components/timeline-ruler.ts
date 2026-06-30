// ── TimelineRuler — Bar/Beat Tick Ruler ──────────────────────────────
//
// Renders bar and beat tick marks with time signature labels and
// loop region highlights. Tick density adapts to zoom level.
//
// Design:
//   - Bar ticks: strong line with bar number text at bar boundaries
//   - Beat ticks: shorter lines within bars
//   - Subdivision ticks: very short lines at beat subdivisions (zoom ≥ 0.3)
//   - Loop region: semi-transparent overlay between loopStart/loopEnd
//   - Zoom-driven density from pixelsPerPPQN

import { Component } from '../framework/component.js';
import { resolveThemeColor } from './fader.js';
import type { VNode } from '../ffi/bridge.js';

// ── Types ──────────────────────────────────────────────────────────

export interface TimelineRulerProps {
  x: number;
  y: number;
  w: number;
  h: number;
  /** Pixels per PPQN (pulses per quarter note) — drives zoom level. */
  pixelsPerPPQN: number;
  /** First bar to render (1-indexed). */
  startBar: number;
  /** Last bar to render (inclusive). */
  endBar: number;
  /** Optional loop region start bar. */
  loopStart?: number;
  /** Optional loop region end bar. */
  loopEnd?: number;
  /** Time signature as [beatsPerBar, beatUnit]. Default [4, 4]. */
  timeSignature?: [number, number];
}

// ── Tick Level Enum ───────────────────────────────────────────────

/** Tick is at a subdivision of a beat (fine). */
export const TICK_SUBDIVISION = 'subdivision' as const;
/** Tick is at a beat boundary (medium). */
export const TICK_BEAT = 'beat' as const;
/** Tick is at a bar boundary (coarse). */
export const TICK_BAR = 'bar' as const;

export type TickLevel = typeof TICK_SUBDIVISION | typeof TICK_BEAT | typeof TICK_BAR;

// ── Constants ──────────────────────────────────────────────────────

/** PPQN (pulses per quarter note) — standard MIDI resolution. */
const PPQN = 960;

/** Zoom thresholds for tick density levels. */
const ZOOM_BAR_THRESHOLD = 0.03; // px/PPQN below this = only bar ticks
const ZOOM_BEAT_THRESHOLD = 0.08; // px/PPQN below this = bar + beat ticks

/** Tick visual heights as ratio of ruler height. */
const TICK_HEIGHT_BAR = 1.0;
const TICK_HEIGHT_BEAT = 0.55;
const TICK_HEIGHT_SUBDIVISION = 0.3;

// ── Pure Functions ─────────────────────────────────────────────────

/**
 * Determine tick density level from pixelsPerPPQN (zoom).
 */
export function getTickDensity(pixelsPerPPQN: number): TickLevel {
  if (pixelsPerPPQN >= ZOOM_BEAT_THRESHOLD * 3) {
    return TICK_SUBDIVISION;
  }
  if (pixelsPerPPQN >= ZOOM_BAR_THRESHOLD) {
    return TICK_BEAT;
  }
  return TICK_BAR;
}

// ── TimelineRuler Component ───────────────────────────────────────

export class TimelineRuler extends Component<TimelineRulerProps, {}> {
  constructor(props: TimelineRulerProps) {
    super({
      ...props,
      timeSignature: props.timeSignature ?? [4, 4],
    });
    this.state = {};
  }

  // ── Rendering ─────────────────────────────────────────────────

  render(): VNode {
    const { x, y, w, h, pixelsPerPPQN, startBar, endBar, loopStart, loopEnd } = this.props;
    const [beatsPerBar] = this.props.timeSignature!;

    const density = getTickDensity(pixelsPerPPQN);
    const children: VNode[] = [];

    // Background
    const bgColor = resolveThemeColor('colors.waveform.background', { r: 0.15, g: 0.15, b: 0.17, a: 1 });
    const borderColor = resolveThemeColor('colors.border.primary', { r: 0.3, g: 0.3, b: 0.35, a: 1 });
    children.push({
      type: 'rect',
      key: `${this.widgetId}-bg`,
      props: {
        x, y, w, h,
        fillR: bgColor.r, fillG: bgColor.g, fillB: bgColor.b, fillA: bgColor.a,
      },
    });

    // Bottom border line
    children.push({
      type: 'rect',
      key: `${this.widgetId}-border`,
      props: {
        x, y: y + h - 1, w, h: 1,
        fillR: borderColor.r, fillG: borderColor.g, fillB: borderColor.b, fillA: borderColor.a,
      },
    });

    // Calculate pixels per bar
    const pxPerBeat = pixelsPerPPQN * PPQN / 4; // quarter note width
    const pxPerBar = pxPerBeat * beatsPerBar;

    // Render loop region overlay (if defined)
    if (loopStart !== undefined && loopEnd !== undefined) {
      const loopX = x + (loopStart - startBar) * pxPerBar;
      const loopW = (loopEnd - loopStart) * pxPerBar;
      const selectionColor = resolveThemeColor('colors.selection.fill', { r: 0.3, g: 0.6, b: 0.9, a: 0.2 });
      children.push({
        type: 'rect',
        key: `${this.widgetId}-loop`,
        props: {
          x: loopX, y, w: loopW, h,
          fillR: selectionColor.r, fillG: selectionColor.g, fillB: selectionColor.b, fillA: selectionColor.a,
        },
      });
    }

    // Render ticks for visible range
    const tickColor = resolveThemeColor('colors.waveform.grid', { r: 0.7, g: 0.7, b: 0.75, a: 1 });
    const textPrimaryColor = resolveThemeColor('colors.text.primary', { r: 0.7, g: 0.7, b: 0.75, a: 1 });
    const textSecondaryColor = resolveThemeColor('colors.text.secondary', { r: 0.5, g: 0.5, b: 0.6, a: 1 });
    const beatColor = resolveThemeColor('colors.waveform.grid', { r: 0.5, g: 0.5, b: 0.55, a: 1 });
    const subColor = resolveThemeColor('colors.waveform.grid', { r: 0.35, g: 0.35, b: 0.4, a: 0.6 });

    for (let bar = startBar; bar <= endBar; bar++) {
      const barX = x + (bar - startBar) * pxPerBar;

      // Bar tick (strong line)
      children.push({
        type: 'rect',
        key: `${this.widgetId}-bar-${bar}`,
        props: {
          x: barX, y, w: 1, h: h * TICK_HEIGHT_BAR,
          fillR: tickColor.r, fillG: tickColor.g, fillB: tickColor.b, fillA: tickColor.a,
        },
      });

      // Bar number
      children.push({
        type: 'text',
        key: `${this.widgetId}-bar-label-${bar}`,
        props: {
          text: String(bar),
          x: barX + 3,
          y: y + 12,
          r: textPrimaryColor.r, g: textPrimaryColor.g, b: textPrimaryColor.b, a: textPrimaryColor.a,
          fontSize: 10,
        },
      });

      // Time signature marker on bar 1
      if (bar === startBar) {
        const sigStr = `${beatsPerBar}/4`;
        children.push({
          type: 'text',
          key: `${this.widgetId}-sig`,
          props: {
            text: sigStr,
            x: barX + 3,
            y: y + h - 4,
            r: textSecondaryColor.r, g: textSecondaryColor.g, b: textSecondaryColor.b, a: textSecondaryColor.a,
            fontSize: 8,
          },
        });
      }

      // Beat and subdivision ticks (if zoomed in enough)
      if (density === TICK_BEAT || density === TICK_SUBDIVISION) {
        for (let beat = 0; beat < beatsPerBar; beat++) {
          const beatX = barX + beat * pxPerBeat;

          // Beat tick (medium line)
          children.push({
            type: 'rect',
            key: `${this.widgetId}-beat-${bar}-${beat}`,
            props: {
              x: beatX, y: y + h * (1 - TICK_HEIGHT_BEAT), w: 1, h: h * TICK_HEIGHT_BEAT,
              fillR: beatColor.r, fillG: beatColor.g, fillB: beatColor.b, fillA: beatColor.a,
            },
          });

          // Subdivision ticks (if zoomed in far enough)
          if (density === TICK_SUBDIVISION) {
            for (let sub = 1; sub < 4; sub++) {
              const subX = beatX + sub * (pxPerBeat / 4);
              children.push({
                type: 'rect',
                key: `${this.widgetId}-sub-${bar}-${beat}-${sub}`,
                props: {
                  x: subX, y: y + h * (1 - TICK_HEIGHT_SUBDIVISION),
                  w: 1, h: h * TICK_HEIGHT_SUBDIVISION,
                  fillR: subColor.r, fillG: subColor.g, fillB: subColor.b, fillA: subColor.a,
                },
              });
            }
          }
        }
      }
    }

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x, y, w, h },
      children,
    };
  }
}
