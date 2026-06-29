// TimeRuler — Unit Tests
import { describe, it, expect } from 'vitest';
import { TimeRuler } from '../../src/views/ArrangementView/TimeRuler.js';
import {
  TRACK_LIST_WIDTH, RULER_HEIGHT, PPQN, BAR_PPQN, BEAT_PPQN, SIXTEENTH_PPQN,
  evaluateFade,
} from '../../src/views/ArrangementView/types.js';

function mockCtx(): CanvasRenderingContext2D {
  // Minimal mock that tracks calls
  return {
    fillStyle: '',
    strokeStyle: '',
    lineWidth: 1,
    font: '',
    textBaseline: 'alphabetic',
    textAlign: 'left',
    fillRect: () => {},
    strokeRect: () => {},
    beginPath: () => {},
    moveTo: () => {},
    lineTo: () => {},
    arc: () => {},
    closePath: () => {},
    fill: () => {},
    stroke: () => {},
    fillText: () => {},
  } as unknown as CanvasRenderingContext2D;
}

describe('TimeRuler', () => {
  it('renders ruler background and playhead', () => {
    const ruler = new TimeRuler();
    const ctx = mockCtx();
    let fillRectCalled = false;

    ctx.fillRect = (_x: number, _y: number, _w: number, _h: number) => {
      if (_x === TRACK_LIST_WIDTH && _y === 0) fillRectCalled = true;
    };

    ruler.render(ctx, {
      scrollX: 0,
      viewWidth: 1000,
      pixelsPerPPQN: 40 / PPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, 0);

    expect(fillRectCalled).toBe(true);
  });

  it('draws playhead at correct PPQN position', () => {
    const ruler = new TimeRuler();
    const ctx = mockCtx();
    let playheadX = -1;

    ctx.beginPath = () => {};
    ctx.moveTo = (x: number, _y: number) => { playheadX = x; };
    ctx.lineTo = () => {};

    const pixelsPerPPQN = 40 / PPQN; // 40px per beat

    ruler.render(ctx, {
      scrollX: 0,
      viewWidth: 1000,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, PPQN); // Playhead at 1 beat

    // playheadX should be TRACK_LIST_WIDTH + 1 beat in pixels
    const expectedX = TRACK_LIST_WIDTH + PPQN * pixelsPerPPQN;
    expect(Math.abs(playheadX - expectedX)).toBeLessThan(0.1);
  });

  it('handles scroll offset correctly', () => {
    const ruler = new TimeRuler();
    const ctx = mockCtx();

    const pixelsPerPPQN = 40 / PPQN;
    const scrollX = 200;

    // With scroll, the playhead at PPQN 960 should appear at:
    // TRACK_LIST_WIDTH + 960 * pixelsPerPPQN - 200
    let playheadX = -1;
    ctx.moveTo = (x: number, _y: number) => { playheadX = x; };
    ctx.beginPath = () => {};
    ctx.lineTo = () => {};

    ruler.render(ctx, {
      scrollX,
      viewWidth: 1000,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, PPQN);

    const expectedX = TRACK_LIST_WIDTH + PPQN * pixelsPerPPQN - scrollX;
    expect(Math.abs(playheadX - expectedX)).toBeLessThan(0.1);
  });

  it('draws bar markers at bar boundaries', () => {
    const ruler = new TimeRuler();
    const ctx = mockCtx();
    const calls: number[] = [];

    ctx.strokeStyle = ''; // override setter
    Object.defineProperty(ctx, 'strokeStyle', {
      set(v: string) { /* noop */ },
      get: () => '#3a3a5c',
    });

    ctx.moveTo = (x: number, _y: number) => {
      calls.push(x);
    };
    ctx.beginPath = () => {};
    ctx.lineTo = () => {};
    ctx.stroke = () => {};

    const pixelsPerPPQN = 1 / PPQN; // Very zoomed out — should show bars only

    ruler.render(ctx, {
      scrollX: 0,
      viewWidth: 2000,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, 0);

    // At this zoom, grid level should be Bars
    // First bar boundary is at PPQN 0, second at BAR_PPQN * 1, etc.
    // We should see at least some grid lines
    expect(calls.length).toBeGreaterThan(0);
  });

  it('fade evaluation matches expected curves', () => {

    // LinearIn: t=0→0, t=0.5→0.5, t=1→1
    expect(evaluateFade('LinearIn', 0)).toBe(0);
    expect(evaluateFade('LinearIn', 0.5)).toBe(0.5);
    expect(evaluateFade('LinearIn', 1)).toBe(1);

    // LinearOut: t=0→1, t=0.5→0.5, t=1→0
    expect(evaluateFade('LinearOut', 0)).toBe(1);
    expect(evaluateFade('LinearOut', 0.5)).toBe(0.5);
    expect(evaluateFade('LinearOut', 1)).toBe(0);

    // EqualPowerIn: sqrt(t)
    expect(evaluateFade('EqualPowerIn', 0)).toBe(0);
    expect(evaluateFade('EqualPowerIn', 0.25)).toBe(0.5);
    expect(evaluateFade('EqualPowerIn', 1)).toBe(1);

    // EqualPowerOut: sqrt(1-t)
    expect(evaluateFade('EqualPowerOut', 0)).toBe(1);
    expect(evaluateFade('EqualPowerOut', 0.75)).toBe(0.5);
    expect(evaluateFade('EqualPowerOut', 1)).toBe(0);

    // None always returns 1
    expect(evaluateFade('None', 0)).toBe(1);
    expect(evaluateFade('None', 0.5)).toBe(1);
    expect(evaluateFade('None', 1)).toBe(1);

    // ExponentialIn: t²
    expect(evaluateFade('ExponentialIn', 0.5)).toBe(0.25);

    // ExponentialOut: 1-(1-t)²
    expect(evaluateFade('ExponentialOut', 0.5)).toBe(0.75);
  });

  it('clamps fade evaluation to [0, 1]', () => {
    expect(evaluateFade('LinearIn', -0.5)).toBe(0);
    expect(evaluateFade('LinearIn', 1.5)).toBe(1);
  });
});
