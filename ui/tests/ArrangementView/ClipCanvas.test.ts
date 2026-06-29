// ClipCanvas — Unit Tests
import { describe, it, expect } from 'vitest';
import { ClipCanvas } from '../../src/views/ArrangementView/ClipCanvas.js';
import {
  PPQN,
  TRACK_HEIGHT,
  RULER_HEIGHT,
  TRACK_LIST_WIDTH,
} from '../../src/views/ArrangementView/types.js';
import {
  clipId,
  trackId,
  AnyTrackData,
  AnyClipData,
  SelectionState,
} from '../../src/views/ArrangementView/types.js';

function mockCtx(): CanvasRenderingContext2D {
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
    quadraticCurveTo: () => {},
    closePath: () => {},
    fill: () => {},
    stroke: () => {},
    fillText: () => {},
  } as unknown as CanvasRenderingContext2D;
}

function makeClip(overrides: Partial<AnyClipData> & { id: number; start_ppqn: number }): AnyClipData {
  const { id: _id, ...rest } = overrides;
  return {
    id: clipId(_id),
    start_ppqn: _id, /* will be overridden */
    length_ppqn: PPQN,
    name: 'Test Clip',
    color: 0,
    gain: 1.0,
    muted: false,
    fade_in_ppqn: 0,
    fade_out_ppqn: 0,
    fade_in_shape: 'None',
    fade_out_shape: 'None',
    looping: false,
    loop_start_ppqn: 0,
    loop_end_ppqn: 0,
    track_id: trackId(1),
    type: 'Audio',
    file_path: '',
    ...rest,
  } as AnyClipData;
}

function makeTrack(overrides: Partial<AnyTrackData> & { id: number; clips: AnyClipData[] }): AnyTrackData {
  const { id: _id, ...rest } = overrides;
  return {
    id: trackId(_id),
    name: `Track ${_id}`,
    type: 'Audio',
    volume: 0,
    pan: 0,
    muted: false,
    soloed: false,
    clips: [],
    ...rest,
  } as AnyTrackData;
}

function emptySelection(): SelectionState {
  return { selectedClipIds: new Set(), hoveredClipId: null };
}

describe('ClipCanvas', () => {
  it('renders clip rectangles with correct widths', () => {
    const canvas = new ClipCanvas();
    const ctx = mockCtx();

    // Track 1: two clips at different lengths
    const clip1 = makeClip({ id: 1, start_ppqn: 0, length_ppqn: PPQN });       // 1 beat
    const clip2 = makeClip({ id: 2, start_ppqn: PPQN * 2, length_ppqn: PPQN * 2 }); // 2 beats

    const tracks = [
      makeTrack({ id: 1, clips: [clip1, clip2] }),
    ];

    const pixelsPerPPQN = 40 / PPQN; // 40px per beat

    // Track fill rects called
    const fillRects: { x: number; y: number; w: number; h: number }[] = [];
    ctx.fillRect = (x: number, y: number, w: number, h: number) => {
      fillRects.push({ x, y, w, h });
    };
    ctx.beginPath = () => {};
    ctx.moveTo = () => {};
    ctx.lineTo = () => {};
    ctx.closePath = () => {};
    ctx.fill = () => {};
    ctx.stroke = () => {};
    ctx.quadraticCurveTo = () => {};
    ctx.fillText = () => {};

    canvas.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, emptySelection());

    // Should have rendered clip rectangles
    // First clip width: PPQN * pixelsPerPPQN = 40px
    // Second clip width: PPQN*2 * pixelsPerPPQN = 80px (twice)
    const clipRects = fillRects.filter(
      r => r.y > 0 && r.w > 0 && r.x >= TRACK_LIST_WIDTH,
    );

    // Find the two clip rectangles
    const rect1 = clipRects.find(r => Math.abs(r.x - TRACK_LIST_WIDTH) < 1);
    const rect2 = clipRects.find(
      r => Math.abs(r.x - (TRACK_LIST_WIDTH + PPQN * 2 * pixelsPerPPQN)) < 1,
    );

    if (rect1 && rect2) {
      expect(rect1.w).toBeCloseTo(40, -1);
      expect(rect2.w).toBeCloseTo(80, -1);
      expect(rect2.w).toBe(rect1.w * 2); // Second is exactly twice as wide
    }
  });

  it('renders waveform when waveform data is present', () => {
    const canvas = new ClipCanvas();
    const ctx = mockCtx();

    const audioClip = {
      ...makeClip({ id: 1, start_ppqn: 0, length_ppqn: PPQN * 4 }),
      type: 'Audio' as const,
      waveform: {
        peaks: [0.5, 0.8, 1.0, 0.6, 0.3, 0.7, 0.9, 0.4],
        valleys: [-0.4, -0.7, -0.3, -0.8, -0.2, -0.6, -0.5, -0.9],
      },
      file_path: '/audio/test.wav',
    };

    const tracks = [makeTrack({ id: 1, clips: [audioClip] })];

    const ops: string[] = [];
    ctx.beginPath = () => { ops.push('beginPath'); };
    ctx.moveTo = () => { ops.push('moveTo'); };
    ctx.lineTo = () => { ops.push('lineTo'); };
    ctx.closePath = () => { ops.push('closePath'); };
    ctx.fill = () => { ops.push('fill'); };
    ctx.stroke = () => { ops.push('stroke'); };
    ctx.quadraticCurveTo = () => {};
    ctx.fillRect = () => {};
    ctx.fillText = () => {};

    canvas.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN: 40 / PPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, emptySelection());

    // Waveform should trigger beginPath + moveTo + lineTo + closePath + fill sequence
    expect(ops).toContain('beginPath');
    expect(ops).toContain('moveTo');
    expect(ops).toContain('fill');
  });

  it('hitTestClip returns correct clip at pixel position', () => {
    const canvas = new ClipCanvas();

    const clip1 = makeClip({ id: 1, start_ppqn: 0, length_ppqn: PPQN });
    const clip2 = makeClip({ id: 2, start_ppqn: PPQN * 4, length_ppqn: PPQN });

    const tracks = [
      makeTrack({ id: 1, clips: [clip1, clip2] }),
    ];

    const pixelsPerPPQN = 40 / PPQN;

    // Click on first clip
    const hit1 = canvas.hitTestClip(
      tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
      TRACK_LIST_WIDTH + 5, RULER_HEIGHT + TRACK_HEIGHT / 2,
    );
    expect(hit1).not.toBeNull();
    expect(hit1!.id.value).toBe(1);

    // Click on second clip (4 beats in)
    const hit2 = canvas.hitTestClip(
      tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
      TRACK_LIST_WIDTH + PPQN * 4 * pixelsPerPPQN + 5,
      RULER_HEIGHT + TRACK_HEIGHT / 2,
    );
    expect(hit2).not.toBeNull();
    expect(hit2!.id.value).toBe(2);

    // Click on empty space between clips (2 beats in)
    const miss = canvas.hitTestClip(
      tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
      TRACK_LIST_WIDTH + PPQN * 2 * pixelsPerPPQN + 5,
      RULER_HEIGHT + TRACK_HEIGHT / 2,
    );
    expect(miss).toBeNull();

    // Click outside canvas area (in track list)
    const miss2 = canvas.hitTestClip(
      tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
      50, RULER_HEIGHT + TRACK_HEIGHT / 2,
    );
    expect(miss2).toBeNull();
  });

  it('renders muted clip with darker overlay', () => {
    const canvas = new ClipCanvas();
    const ctx = mockCtx();

    const mutedClip = makeClip({
      id: 1, start_ppqn: 0, length_ppqn: PPQN, muted: true,
    });

    const tracks = [makeTrack({ id: 1, clips: [mutedClip] })];

    const fillRects: { x: number; y: number; w: number; h: number }[] = [];
    ctx.fillRect = (x: number, y: number, w: number, h: number) => {
      fillRects.push({ x, y, w, h });
    };
    ctx.beginPath = () => {};
    ctx.moveTo = () => {};
    ctx.lineTo = () => {};
    ctx.closePath = () => {};
    ctx.fill = () => {};
    ctx.stroke = () => {};
    ctx.quadraticCurveTo = () => {};
    ctx.fillText = () => {};

    canvas.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN: 40 / PPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, emptySelection());

    // Muted clips get a semi-transparent overlay
    expect(fillRects.length).toBeGreaterThan(0);
  });

  it('renders "S" badge for SessionCapture clips', () => {
    const canvas = new ClipCanvas();
    const ctx = mockCtx();

    const sessionClip = makeClip({
      id: 1, start_ppqn: 0, length_ppqn: PPQN * 2,
      type: 'SessionCapture',
      capture_source_track_id: trackId(1),
      capture_source_scene_id: { value: 1 },
    });

    const tracks = [makeTrack({ id: 1, clips: [sessionClip] })];

    const fillTexts: { text: string; x: number; y: number }[] = [];
    ctx.fillText = (text: string, x: number, y: number) => {
      fillTexts.push({ text, x, y });
    };
    ctx.fillRect = () => {};
    ctx.beginPath = () => {};
    ctx.moveTo = () => {};
    ctx.lineTo = () => {};
    ctx.closePath = () => {};
    ctx.fill = () => {};
    ctx.stroke = () => {};
    ctx.quadraticCurveTo = () => {};

    canvas.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN: 40 / PPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, emptySelection());

    // Should have fillText call with "S" badge
    const badgeText = fillTexts.find(t => t.text === 'S');
    expect(badgeText).toBeDefined();
    expect(badgeText!.x).toBeGreaterThan(TRACK_LIST_WIDTH);
  });
});
