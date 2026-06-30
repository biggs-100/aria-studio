// ── ChannelStrip — Composite Channel Component ───────────────────────
//
// Composes Fader + MeterBar + mute/solo buttons + pan knob placeholder
// + routing label into a fixed-width vertical column.
//
// Design:
//   - Extends Component<ChannelStripProps, ChannelStripState>
//   - Mounts Fader and MeterBar as child components
//   - Renders mute/solo buttons as rect + text VNodes directly
//   - Narrow strip (< 40px width) hides the MeterBar
//   - Pan rendered as a simple position indicator

import { Component } from '../framework/component.js';
import { Fader, resolveThemeColor } from './fader.js';
import { MeterBar } from './meter-bar.js';
import type { VNode } from '../ffi/bridge.js';

// ── Types ──────────────────────────────────────────────────────────

export interface ChannelStripProps {
  x: number;
  y: number;
  w: number;
  h: number;
  /** Channel name label. */
  label: string;
  /** Initial fader volume [0, 1]. */
  initialVolume: number;
  /** Mute state. */
  mute: boolean;
  /** Solo state. */
  solo: boolean;
  /** Pan position (-1 left, 0 center, 1 right). Default 0. */
  pan?: number;
}

export interface ChannelStripState {
  volume: number;
  isMuted: boolean;
  isSoloed: boolean;
  panValue: number;
}

// ── Layout constants ───────────────────────────────────────────────

/** Minimum strip width to show the meter bar. */
const MIN_METER_WIDTH = 40;
/** Height allocated for the meter bar. */
const METER_HEIGHT_RATIO = 0.55;
/** Height allocated for the fader. */
const FADER_HEIGHT_RATIO = 0.25;
/** Button area height. */
const BUTTON_AREA_HEIGHT = 40;
/** Label height. */
const LABEL_HEIGHT = 20;

// ── ChannelStrip Component ─────────────────────────────────────────

export class ChannelStrip extends Component<ChannelStripProps, ChannelStripState> {
  /** Child Fader component for volume control. */
  private _fader: Fader;
  /** Child MeterBar component for level display. */
  private _meter: MeterBar | null = null;

  constructor(props: ChannelStripProps) {
    super(props);
    this.state = {
      volume: props.initialVolume,
      isMuted: props.mute,
      isSoloed: props.solo,
      panValue: props.pan ?? 0,
    };

    // Create child Fader
    const faderY = props.y + props.h * METER_HEIGHT_RATIO;
    this._fader = new Fader({
      x: props.x,
      y: faderY,
      w: props.w,
      h: props.h * FADER_HEIGHT_RATIO,
      value: props.initialVolume,
      label: '',
      setValue: (v: number) => {
        this.setState({ volume: v });
      },
    });

    // Create child MeterBar if wide enough
    if (props.w >= MIN_METER_WIDTH) {
      this._meter = new MeterBar({
        x: props.x + 2,
        y: props.y + 2,
        w: props.w - 4,
        h: props.h * METER_HEIGHT_RATIO - 4,
        peak: -24,
        rms: -30,
      });
    }
  }

  // ── Lifecycle ──────────────────────────────────────────────────

  onMount(): void {
    super.onMount();
    this._fader.mount(this);
    if (this._meter) {
      this._meter.mount(this);
    }
  }

  onUnmount(): void {
    super.onUnmount();
    this._fader.unmount();
    if (this._meter) {
      this._meter.unmount();
    }
  }

  // ── Rendering ─────────────────────────────────────────────────

  render(): VNode {
    const { x, y, w, h, label } = this.props;
    const { volume, isMuted, isSoloed, panValue } = this.state;
    const showMeter = w >= MIN_METER_WIDTH;

    const children: VNode[] = [];

    // Background
    const bgColor = resolveThemeColor('colors.bg.secondary', { r: 0.18, g: 0.18, b: 0.2, a: 1 });
    const labelColor = resolveThemeColor('colors.text.primary', { r: 0.8, g: 0.8, b: 0.85, a: 1 });
    children.push({
      type: 'rect',
      key: `${this.widgetId}-bg`,
      props: {
        x, y, w, h,
        fillR: bgColor.r, fillG: bgColor.g, fillB: bgColor.b, fillA: bgColor.a,
        cornerRadius: 2,
      },
    });

    // Label at top
    children.push({
      type: 'text',
      key: `${this.widgetId}-label`,
      props: {
        text: label,
        x: x + 2,
        y: y + 12,
        r: labelColor.r, g: labelColor.g, b: labelColor.b, a: labelColor.a,
        fontSize: 9,
      },
    });

    // Meter area
    if (showMeter && this._meter) {
      children.push(this._meter.render());
    }

    // Fader area
    children.push(this._fader.render());

    // Button area (mute + solo)
    const btnAreaY = y + h * (METER_HEIGHT_RATIO + FADER_HEIGHT_RATIO);
    const btnW = Math.max(8, (w - 6) / 2);
    const btnH = 16;
    const btnY2 = btnAreaY + 4;

    // Mute button
    const mutedBg = resolveThemeColor('colors.bg.surface', { r: 0.3, g: 0.3, b: 0.35, a: 1 });
    const errorColor = resolveThemeColor('colors.text.error', { r: 0.9, g: 0.3, b: 0.2, a: 1 });
    const muteColor = isMuted ? errorColor : mutedBg;

    children.push({
      type: 'rect',
      key: `${this.widgetId}-mute`,
      props: {
        x: x + 2, y: btnY2, w: btnW, h: btnH,
        fillR: muteColor.r, fillG: muteColor.g, fillB: muteColor.b, fillA: muteColor.a,
        cornerRadius: 2,
      },
    });
    children.push({
      type: 'text',
      key: `${this.widgetId}-mute-label`,
      props: {
        text: 'M',
        x: x + 2 + btnW / 2 - 3,
        y: btnY2 + btnH / 2 + 3,
        r: 1, g: 1, b: 1, a: 1,
        fontSize: 9,
      },
    });

    // Solo button
    const warningColor = resolveThemeColor('colors.text.warning', { r: 0.9, g: 0.8, b: 0.1, a: 1 });
    const soloColor = isSoloed ? warningColor : mutedBg;

    children.push({
      type: 'rect',
      key: `${this.widgetId}-solo`,
      props: {
        x: x + 4 + btnW, y: btnY2, w: btnW, h: btnH,
        fillR: soloColor.r, fillG: soloColor.g, fillB: soloColor.b, fillA: soloColor.a,
        cornerRadius: 2,
      },
    });
    children.push({
      type: 'text',
      key: `${this.widgetId}-solo-label`,
      props: {
        text: 'S',
        x: x + 4 + btnW + btnW / 2 - 3,
        y: btnY2 + btnH / 2 + 3,
        r: 1, g: 1, b: 1, a: 1,
        fontSize: 9,
      },
    });

    // Pan indicator (simple text + position bar)
    const panStr = panValue === 0 ? 'C' : panValue < 0 ? `L${Math.abs(Math.round(panValue * 100))}` : `R${Math.round(panValue * 100)}`;
    const secondaryColor = resolveThemeColor('colors.text.secondary', { r: 0.6, g: 0.6, b: 0.65, a: 1 });
    children.push({
      type: 'text',
      key: `${this.widgetId}-pan`,
      props: {
        text: panStr,
        x: x + 2,
        y: btnY2 + btnH + 14,
        r: secondaryColor.r, g: secondaryColor.g, b: secondaryColor.b, a: secondaryColor.a,
        fontSize: 8,
      },
    });

    // Volume value at bottom
    const volText = `${Math.round(volume * 100)}%`;
    children.push({
      type: 'text',
      key: `${this.widgetId}-vol`,
      props: {
        text: volText,
        x: x + w - 28,
        y: btnY2 + btnH + 14,
        r: secondaryColor.r, g: secondaryColor.g, b: secondaryColor.b, a: secondaryColor.a,
        fontSize: 8,
      },
    });

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x, y, w, h },
      children,
    };
  }
}
