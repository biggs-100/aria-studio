// Fader — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect } from 'vitest';
import { Fader, valueToDb, clampValue, dbHeightRatio, calculateFlickSnap } from '../../src/components/fader.js';

// ── Pure Function Tests ──────────────────────────────────────────────

describe('valueToDb', () => {
  it('maps 0.5 to approximately 0dB (unity gain)', () => {
    expect(valueToDb(0.5)).toBeCloseTo(0, 0);
  });

  it('maps 1.0 to approximately +6dB (maximum boost)', () => {
    expect(valueToDb(1.0)).toBeCloseTo(6.02, 1);
  });

  it('maps 0.25 to approximately -6dB (half amplitude relative to 0.5)', () => {
    // 0.25 is half of 0.5, so it's -6dB from unity (0dB at 0.5)
    expect(valueToDb(0.25)).toBeCloseTo(-6.02, 1);
  });

  it('maps 0.0 to -Infinity (represented as -96 dB noise floor)', () => {
    const db = valueToDb(0);
    expect(db).toBeLessThanOrEqual(-90);
  });

  it('maps 0.75 to approximately +3.5dB', () => {
    const db = valueToDb(0.75);
    expect(db).toBeGreaterThan(0);
    expect(db).toBeLessThan(6.1);
  });
});

describe('clampValue', () => {
  it('returns value unchanged when within [0, 1]', () => {
    expect(clampValue(0.5)).toBe(0.5);
    expect(clampValue(0)).toBe(0);
    expect(clampValue(1)).toBe(1);
  });

  it('clamps negative values to 0', () => {
    expect(clampValue(-0.1)).toBe(0);
    expect(clampValue(-1)).toBe(0);
  });

  it('clamps values above 1 to 1', () => {
    expect(clampValue(1.1)).toBe(1);
    expect(clampValue(5)).toBe(1);
  });
});

describe('dbHeightRatio', () => {
  it('returns 1 for +6dB (full height)', () => {
    expect(dbHeightRatio(6)).toBe(1);
  });

  it('returns 0 for -90dB or below (no height)', () => {
    expect(dbHeightRatio(-90)).toBeLessThanOrEqual(0.001);
    expect(dbHeightRatio(-96)).toBe(0);
  });

  it('returns 0.5 at approximately -6dB (half height)', () => {
    const ratio = dbHeightRatio(-6);
    expect(ratio).toBeGreaterThan(0.4);
    expect(ratio).toBeLessThan(0.6);
  });

  it('returns ratio between 0 and 1 for intermediate values', () => {
    const ratio = dbHeightRatio(-24);
    expect(ratio).toBeGreaterThan(0);
    expect(ratio).toBeLessThan(1);
  });
});

describe('calculateFlickSnap', () => {
  it('does not snap when velocity is below threshold', () => {
    const result = calculateFlickSnap(0.5, 0.1);
    expect(result.snapped).toBe(false);
    expect(result.value).toBe(0.5);
  });

  it('snaps to 1 when flicking upward with high velocity and value > 0.5', () => {
    const result = calculateFlickSnap(0.8, 15);
    expect(result.snapped).toBe(true);
    expect(result.value).toBe(1);
  });

  it('snaps to 0 when flicking downward with high velocity and value < 0.5', () => {
    const result = calculateFlickSnap(0.2, -15);
    expect(result.snapped).toBe(true);
    expect(result.value).toBe(0);
  });

  it('does not snap when at mid value regardless of velocity', () => {
    const result = calculateFlickSnap(0.5, 20);
    expect(result.snapped).toBe(false);
    expect(result.value).toBe(0.5);
  });
});

// ── Component Tests ──────────────────────────────────────────────────

describe('Fader component', () => {
  it('creates a Fader with initial props', () => {
    const fader = new Fader({ x: 10, y: 10, w: 30, h: 150, value: 0.5, label: 'Vol' });
    expect(fader.props.value).toBe(0.5);
    expect(fader.props.label).toBe('Vol');
    expect(fader.props.x).toBe(10);
    expect(fader.props.y).toBe(10);
    expect(fader.props.w).toBe(30);
    expect(fader.props.h).toBe(150);
  });

  it('initializes state with clamped value', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 1.5, label: 'Test' });
    // Value should be clamped to [0, 1]
    expect(fader.state.value).toBe(1);
  });

  it('render() returns container VNode with key matching widgetId', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 0.5, label: 'Vol' });
    const vnode = fader.render();
    expect(vnode.type).toBe('container');
    expect(vnode.key).toBe(String(fader.widgetId));
    expect(vnode.props.x).toBe(0);
    expect(vnode.props.y).toBe(0);
    expect(vnode.props.w).toBe(30);
    expect(vnode.props.h).toBe(150);
  });

  it('render() includes track, thumb, and label children', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 0.5, label: 'Vol' });
    const vnode = fader.render();
    expect(vnode.children).toBeDefined();
    expect(vnode.children!.length).toBeGreaterThanOrEqual(3);

    const types = vnode.children!.map(c => c.type);
    expect(types).toContain('rect');
    expect(types).toContain('text');
  });

  it('render() includes eventHandlers for mousedrag', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 0.5, label: 'Vol' });
    const vnode = fader.render();
    expect(vnode.eventHandlers).toBeDefined();
    expect(vnode.eventHandlers!.mousedrag).toBeDefined();
  });

  it('setValue callback is called on drag', () => {
    let calledValue = -1;
    const fader = new Fader({
      x: 0, y: 0, w: 30, h: 150, value: 0.5, label: 'Vol',
      setValue: (v: number) => { calledValue = v; },
    });
    fader.mount();

    // Drag upward by 60px → should increase value significantly
    fader.onDrag({ deltaY: -60, velocity: 0 });
    expect(calledValue).toBeGreaterThan(0.5);
    expect(calledValue).toBeLessThanOrEqual(1);
  });

  it('handles drag event updating value proportionally', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 0.5, label: 'Vol' });
    fader.mount();

    // Simulate dragging upward (negative delta Y = value increase)
    fader.onDrag({ deltaY: -30, velocity: 0 });
    expect(fader.state.value).toBeGreaterThan(0.5);
    expect(fader.state.value).toBeLessThanOrEqual(1);
  });

  it('handles drag event downward decreasing value', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 0.8, label: 'Vol' });
    fader.mount();

    fader.onDrag({ deltaY: 30, velocity: 0 });
    expect(fader.state.value).toBeLessThan(0.8);
  });

  it('dB label matches current value', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 0.5, label: 'Vol' });
    const vnode = fader.render();
    const textNode = vnode.children!.find(c => c.type === 'text');
    expect(textNode).toBeDefined();
    expect(textNode!.props.text).toContain('dB');
  });

  it('touch flick snaps value to extreme', () => {
    const fader = new Fader({ x: 0, y: 0, w: 30, h: 150, value: 0.8, label: 'Vol' });
    fader.mount();

    // High positive velocity = fast upward flick, should snap to 1
    fader.onDrag({ deltaY: -5, velocity: 20 });
    const snapped = fader.state.value >= 1;
    // Either it snapped to 1 or is very close
    expect(fader.state.value).toBeGreaterThanOrEqual(0.95);
  });
});
