// CrossfaderWidget — Unit Tests
import { describe, it, expect } from 'vitest';
import { CrossfaderWidget } from '../../src/views/SessionView/CrossfaderWidget.js';
import {
  CROSSFADER_HEIGHT,
  CrossfaderCurve,
} from '../../src/views/SessionView/types.js';

function mockCtx(): CanvasRenderingContext2D {
  return {
    fillStyle: '', strokeStyle: '', lineWidth: 1, font: '',
    textBaseline: 'alphabetic', textAlign: 'left',
    fillRect: () => {}, strokeRect: () => {},
    beginPath: () => {}, moveTo: () => {}, lineTo: () => {},
    arc: () => {}, quadraticCurveTo: () => {},
    closePath: () => {}, fill: () => {}, stroke: () => {},
    fillText: () => {},
  } as unknown as CanvasRenderingContext2D;
}

const WIDTH = 800;

describe('CrossfaderWidget', () => {
  it('renders without crashing at center position', () => {
    const widget = new CrossfaderWidget();
    const ctx = mockCtx();

    expect(() => widget.render(ctx, { position: 0, curve: 'EqualPower' }, {
      viewWidth: WIDTH,
    })).not.toThrow();
  });

  it('renders at extreme positions without crashing', () => {
    const widget = new CrossfaderWidget();
    const ctx = mockCtx();

    expect(() => widget.render(ctx, { position: -1, curve: 'Linear' }, {
      viewWidth: WIDTH,
    })).not.toThrow();

    expect(() => widget.render(ctx, { position: 1, curve: 'EqualPower' }, {
      viewWidth: WIDTH,
    })).not.toThrow();
  });

  it('renders with both curve types without crashing', () => {
    const widget = new CrossfaderWidget();
    const ctx = mockCtx();

    expect(() => widget.render(ctx, { position: 0.3, curve: 'Linear' }, {
      viewWidth: WIDTH,
    })).not.toThrow();

    expect(() => widget.render(ctx, { position: -0.5, curve: 'EqualPower' }, {
      viewWidth: WIDTH,
    })).not.toThrow();
  });

  it('accepts mouse down on handle and returns new position on move', () => {
    const widget = new CrossfaderWidget();
    const ctx = mockCtx();
    const config = { viewWidth: WIDTH };

    // Start at center
    widget.render(ctx, { position: 0, curve: 'Linear' }, config);

    // Mouse down on handle area (center of widget, half height)
    const handleX = WIDTH * 0.5; // center
    const handleY = CROSSFADER_HEIGHT / 2;

    const hit = widget.onMouseDown(handleX, handleY, config);
    expect(hit).toBe(true);

    // Drag to the right → positive position
    const pos = widget.onMouseMove(WIDTH * 0.75, config);
    expect(pos).not.toBeNull();
    expect(pos! > 0).toBe(true);

    // Drag to the left → negative position
    const pos2 = widget.onMouseMove(WIDTH * 0.25, config);
    expect(pos2).not.toBeNull();
    expect(pos2! < 0).toBe(true);
  });

  it('clamp: drag beyond track returns -1 or +1', () => {
    const widget = new CrossfaderWidget();
    const ctx = mockCtx();
    const config = { viewWidth: WIDTH };

    widget.render(ctx, { position: 0, curve: 'EqualPower' }, config);

    // Click to start drag
    widget.onMouseDown(WIDTH * 0.5, CROSSFADER_HEIGHT / 2, config);

    // Drag far left
    const farLeft = widget.onMouseMove(-50, config);
    expect(farLeft).not.toBeNull();
    expect(farLeft!).toBeCloseTo(-1, 4);

    // Drag far right
    const farRight = widget.onMouseMove(WIDTH + 50, config);
    expect(farRight).not.toBeNull();
    expect(farRight!).toBeCloseTo(1, 4);
  });

  it('does not move when not dragging', () => {
    const widget = new CrossfaderWidget();
    const config = { viewWidth: WIDTH };

    const pos = widget.onMouseMove(400, config);
    expect(pos).toBeNull();
  });

  it('stops dragging on mouse up', () => {
    const widget = new CrossfaderWidget();
    const ctx = mockCtx();
    const config = { viewWidth: WIDTH };

    widget.render(ctx, { position: 0, curve: 'Linear' }, config);

    widget.onMouseDown(WIDTH * 0.5, CROSSFADER_HEIGHT / 2, config);
    expect(widget.isDragging).toBe(true);

    widget.onMouseUp();
    expect(widget.isDragging).toBe(false);

    // After mouse up, move should not produce a position
    const pos = widget.onMouseMove(700, config);
    expect(pos).toBeNull();
  });

  it('accepts click anywhere on the track', () => {
    const widget = new CrossfaderWidget();
    const ctx = mockCtx();
    const config = { viewWidth: WIDTH };

    widget.render(ctx, { position: 0, curve: 'Linear' }, config);

    // Click on the track itself (not exactly on handle)
    // Track occupies roughly 15%-85% of width
    const trackPx = WIDTH * 0.3;
    const hit = widget.onMouseDown(trackPx, CROSSFADER_HEIGHT / 2, config);
    expect(hit).toBe(true);
  });
});
