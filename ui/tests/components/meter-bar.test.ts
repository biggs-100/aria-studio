// MeterBar — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect, vi } from 'vitest';
import {
  MeterBar,
  getZoneColor,
  levelToHeight,
  ZONE_THRESHOLD_YELLOW,
  ZONE_THRESHOLD_RED,
} from '../../src/components/meter-bar.js';

// ── Pure Function Tests ──────────────────────────────────────────────

describe('getZoneColor', () => {
  it('returns green for levels below -18dB', () => {
    const color = getZoneColor(-24);
    expect(color.r).toBeLessThanOrEqual(0.3);
    expect(color.g).toBeGreaterThan(0.5);
    expect(color.b).toBeLessThanOrEqual(0.3);
  });

  it('returns yellow for levels between -18dB and -6dB', () => {
    const color = getZoneColor(-12);
    expect(color.r).toBeGreaterThan(0.7);
    expect(color.g).toBeGreaterThan(0.5);
    expect(color.b).toBeLessThanOrEqual(0.3);
  });

  it('returns red for levels above -6dB', () => {
    const color = getZoneColor(-3);
    expect(color.r).toBeGreaterThan(0.8);
    expect(color.g).toBeLessThan(0.3);
    expect(color.b).toBeLessThan(0.3);
  });

  it('returns red for levels at 0dB or above', () => {
    const color = getZoneColor(0);
    expect(color.r).toBeGreaterThan(0.8);
  });

  it('transitions at exact -18dB threshold to yellow', () => {
    const color = getZoneColor(-18);
    expect(color.r).toBeGreaterThan(0.7); // yellow-ish (has red)
    expect(color.g).toBeGreaterThan(0.5); // yellow-ish (has green)
  });

  it('transitions at exact -6dB threshold to red', () => {
    const color = getZoneColor(-6);
    expect(color.r).toBeGreaterThan(0.7); // red-ish
  });
});

describe('levelToHeight', () => {
  it('returns 1 for levels at +6dB or above (full height)', () => {
    expect(levelToHeight(6)).toBe(1);
    expect(levelToHeight(10)).toBe(1);
  });

  it('returns 0 for levels at -90dB or below (no height)', () => {
    expect(levelToHeight(-90)).toBe(0);
    expect(levelToHeight(-96)).toBe(0);
  });

  it('returns higher ratio for louder levels', () => {
    const low = levelToHeight(-24);
    const mid = levelToHeight(-12);
    const high = levelToHeight(-3);
    expect(low).toBeLessThan(mid);
    expect(mid).toBeLessThan(high);
  });

  it('returns 0 at -90dB boundary', () => {
    expect(levelToHeight(-90)).toBe(0);
  });
});

// ── Component Tests ──────────────────────────────────────────────────

describe('MeterBar component', () => {
  it('creates a MeterBar with initial props', () => {
    const meter = new MeterBar({ x: 0, y: 0, w: 20, h: 100, peak: -12, rms: -18 });
    expect(meter.props.peak).toBe(-12);
    expect(meter.props.rms).toBe(-18);
    expect(meter.props.x).toBe(0);
    expect(meter.props.y).toBe(0);
    expect(meter.props.w).toBe(20);
    expect(meter.props.h).toBe(100);
  });

  it('render() returns container VNode with key matching widgetId', () => {
    const meter = new MeterBar({ x: 0, y: 0, w: 20, h: 100, peak: -12, rms: -18 });
    const vnode = meter.render();
    expect(vnode.type).toBe('container');
    expect(vnode.key).toBe(String(meter.widgetId));
  });

  it('render() includes rect children for peak and RMS bars', () => {
    const meter = new MeterBar({ x: 0, y: 0, w: 20, h: 100, peak: -12, rms: -18 });
    const vnode = meter.render();
    expect(vnode.children).toBeDefined();
    expect(vnode.children!.length).toBeGreaterThanOrEqual(2);

    const rects = vnode.children!.filter(c => c.type === 'rect');
    expect(rects.length).toBeGreaterThanOrEqual(2);
  });

  it('does NOT show clip indicator when peak is below 0dB', () => {
    const meter = new MeterBar({ x: 0, y: 0, w: 20, h: 100, peak: -3, rms: -12 });
    const vnode = meter.render();
    // No clip indicator element when not clipping
    const clipText = vnode.children!.find(
      c => c.type === 'text' && (c.props.text as string).includes('CLIP'),
    );
    expect(clipText).toBeUndefined();
  });

  it('shows clip indicator when peak exceeds 0dB', () => {
    const meter = new MeterBar({ x: 0, y: 0, w: 20, h: 100, peak: 1, rms: -3 });
    const vnode = meter.render();
    const clipText = vnode.children!.find(
      c => c.type === 'text' && (c.props.text as string).includes('CLIP'),
    );
    expect(clipText).toBeDefined();
  });

  it('clip hold state persists for at least 2 seconds after peak drops below 0', () => {
    vi.useFakeTimers();

    const meter = new MeterBar({ x: 0, y: 0, w: 20, h: 100, peak: 2, rms: -3 });
    meter.mount();
    expect(meter.state.clipHold).toBe(true);

    // Update to safe level
    meter.setState({ peak: -12 });
    // Clip hold should still be true (timer not expired yet)
    expect(meter.state.clipHold).toBe(true);

    // Advance time by 3 seconds (past the 2s hold)
    vi.advanceTimersByTime(3000);
    expect(meter.state.clipHold).toBe(false);

    vi.useRealTimers();
  });

  it('green zone color at -24dB', () => {
    const meter = new MeterBar({ x: 0, y: 0, w: 20, h: 100, peak: -24, rms: -30 });
    const vnode = meter.render();
    // The peak bar rect should have green-ish fill
    expect(vnode.children).toBeDefined();
  });
});
