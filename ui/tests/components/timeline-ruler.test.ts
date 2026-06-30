// TimelineRuler — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect } from 'vitest';
import {
  TimelineRuler,
  getTickDensity,
  TICK_SUBDIVISION,
  TICK_BEAT,
  TICK_BAR,
} from '../../src/components/timeline-ruler.js';

// ── Pure Function Tests ──────────────────────────────────────────────

describe('getTickDensity', () => {
  it('returns BAR ticks at very zoomed-out levels (low pixelsPerPPQN)', () => {
    const density = getTickDensity(0.02);
    expect(density).toBe(TICK_BAR);
  });

  it('returns BEAT ticks at medium zoom levels', () => {
    const density = getTickDensity(0.1);
    expect(density).toBe(TICK_BEAT);
  });

  it('returns SUBDIVISION ticks at zoomed-in levels (high pixelsPerPPQN)', () => {
    const density = getTickDensity(0.5);
    expect(density).toBe(TICK_SUBDIVISION);
  });

  it('returns BAR ticks at extremely low pixelsPerPPQN', () => {
    const density = getTickDensity(0.005);
    expect(density).toBe(TICK_BAR);
  });

  it('returns SUBDIVISION at very high pixelsPerPPQN', () => {
    const density = getTickDensity(2);
    expect(density).toBe(TICK_SUBDIVISION);
  });
});

// ── Component Tests ──────────────────────────────────────────────────

describe('TimelineRuler component', () => {
  it('creates a TimelineRuler with default props', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1,
      startBar: 1,
      endBar: 8,
      timeSignature: [4, 4],
    });
    expect(ruler.props.startBar).toBe(1);
    expect(ruler.props.endBar).toBe(8);
    expect(ruler.props.pixelsPerPPQN).toBe(0.1);
    expect(ruler.props.timeSignature).toEqual([4, 4]);
  });

  it('render() returns container VNode with key matching widgetId', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1, startBar: 1, endBar: 8,
    });
    const vnode = ruler.render();
    expect(vnode.type).toBe('container');
    expect(vnode.key).toBe(String(ruler.widgetId));
  });

  it('render() includes rect children for tick marks', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1, startBar: 1, endBar: 8,
    });
    const vnode = ruler.render();
    expect(vnode.children).toBeDefined();
    expect(vnode.children!.length).toBeGreaterThan(0);

    const rects = vnode.children!.filter(c => c.type === 'rect');
    expect(rects.length).toBeGreaterThanOrEqual(3); // at least a few ticks
  });

  it('render() includes text labels for bar numbers', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1, startBar: 1, endBar: 4,
    });
    const vnode = ruler.render();
    const texts = vnode.children!.filter(c => c.type === 'text');
    expect(texts.length).toBeGreaterThanOrEqual(1);

    // Should have bar numbers
    const barNumbers = texts.filter(
      t => typeof t.props.text === 'string' && /^\d+$/.test(t.props.text as string),
    );
    expect(barNumbers.length).toBeGreaterThanOrEqual(1);
  });

  it('loop region renders as a colored rect overlay when loopStart/loopEnd provided', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1, startBar: 1, endBar: 8,
      loopStart: 2, loopEnd: 4,
    });
    const vnode = ruler.render();

    // Should have a semi-transparent overlay rect for loop region
    const overlay = vnode.children!.find(
      c => c.key === `${ruler.widgetId}-loop`,
    );
    expect(overlay).toBeDefined();
    expect(overlay!.type).toBe('rect');
    // Loop overlay should be semi-transparent
    const fillA = overlay!.props.fillA as number;
    expect(fillA).toBeGreaterThan(0);
    expect(fillA).toBeLessThan(1);
  });

  it('does NOT render loop overlay when loopStart/loopEnd are undefined', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1, startBar: 1, endBar: 8,
    });
    const vnode = ruler.render();
    const overlays = vnode.children!.filter(
      c => c.key?.includes('loop'),
    );
    expect(overlays.length).toBe(0);
  });

  it('renders more ticks at higher zoom density', () => {
    const zoomedOut = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.02, startBar: 1, endBar: 4,
    });
    const zoomedIn = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.5, startBar: 1, endBar: 4,
    });

    const outVNode = zoomedOut.render();
    const inVNode = zoomedIn.render();

    // Zoomed in should have more/more-detailed ticks
    const outRects = outVNode.children!.filter(c => c.type === 'rect');
    const inRects = inVNode.children!.filter(c => c.type === 'rect');
    expect(inRects.length).toBeGreaterThanOrEqual(outRects.length);
  });

  it('time signature marker renders when explicitly set', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1, startBar: 1, endBar: 4,
      timeSignature: [3, 4],
    });
    const vnode = ruler.render();
    const sigText = vnode.children!.find(
      c => c.type === 'text' && (c.props.text as string).includes('3/4'),
    );
    expect(sigText).toBeDefined();
  });

  it('default time signature is 4/4', () => {
    const ruler = new TimelineRuler({
      x: 0, y: 0, w: 800, h: 30,
      pixelsPerPPQN: 0.1, startBar: 1, endBar: 4,
    });
    expect(ruler.props.timeSignature).toEqual([4, 4]);
  });
});
