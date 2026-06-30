// ARIA DAW — UI Component Library
//
// Exports all DAW GPU-rendered widgets built atop the component framework.
// Legacy Canvas 2D components (Button, Slider) are preserved for backward
// compatibility behind the useGpuComponents toggle.

// ── Legacy Canvas 2D components ──────────────────────────────────────

export interface Component {
  readonly id: string;
  readonly type: string;
  render(): void;
  destroy(): void;
}

export class Button implements Component {
  readonly id: string;
  readonly type = 'Button';

  constructor(id: string, _label: string) {
    this.id = id;
  }

  render(): void {
    // GPU-rendered button placeholder
  }

  destroy(): void {}
}

export class Slider implements Component {
  readonly id: string;
  readonly type = 'Slider';
  private value = 0.5;

  constructor(id: string) {
    this.id = id;
  }

  setValue(v: number): void {
    this.value = Math.max(0, Math.min(1, v));
  }

  getValue(): number {
    return this.value;
  }

  render(): void {}
  destroy(): void {}
}

// ── GPU Component Framework exports ─────────────────────────────────
export { Component as FrameworkComponent } from '../framework/component.js';

// ── DAW Component Library — P10b GPU-rendered widgets ────────────────
export { Fader, valueToDb, clampValue, dbHeightRatio, calculateFlickSnap } from './fader.js';
export type { FaderProps, FaderState } from './fader.js';

export { MeterBar, getZoneColor, levelToHeight, ZONE_THRESHOLD_YELLOW, ZONE_THRESHOLD_RED } from './meter-bar.js';
export type { MeterBarProps, MeterBarState } from './meter-bar.js';

export { ChannelStrip } from './channel-strip.js';
export type { ChannelStripProps, ChannelStripState } from './channel-strip.js';

export { TimelineRuler, getTickDensity, TICK_SUBDIVISION, TICK_BEAT, TICK_BAR } from './timeline-ruler.js';
export type { TimelineRulerProps, TickLevel } from './timeline-ruler.js';
