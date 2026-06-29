// FadeOverlay — Unit Tests
import { describe, it, expect } from 'vitest';
import { FadeOverlay } from '../../src/views/ArrangementView/FadeOverlay.js';
import {
  PPQN,
  TRACK_HEIGHT,
  RULER_HEIGHT,
  TRACK_LIST_WIDTH,
  AnyTrackData,
  AnyClipData,
} from '../../src/views/ArrangementView/types.js';
import { clipId, trackId } from '../../src/views/ArrangementView/types.js';

function mockCtx(): CanvasRenderingContext2D {
  return {
    fillStyle: '',
    strokeStyle: '',
    lineWidth: 1,
    font: '',
    textBaseline: 'alphabetic',
    textAlign: 'left',
    fillRect: () => {},
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

function makeClip(overrides: Partial<AnyClipData> & { id: number; start_ppqn: number }): AnyClipData {
  const { id: _id, ...rest } = overrides;
  return {
    id: clipId(_id),
    start_ppqn: _id, /* placeholder, overridden by rest */
    length_ppqn: PPQN * 4,
    name: 'Test',
    color: 0,
    gain: 1.0,
    muted: false,
    fade_in_ppqn: 0,
    fade_out_ppqn: 0,
    fade_in_shape: 'LinearIn',
    fade_out_shape: 'LinearOut',
    looping: false,
    loop_start_ppqn: 0,
    loop_end_ppqn: 0,
    track_id: trackId(1),
    type: 'Audio',
    file_path: '',
    ...rest,
  } as AnyClipData;
}

function makeTrack(overrides: { id: number; clips: AnyClipData[] }): AnyTrackData {
  const { id: _id, ...rest } = overrides;
  return {
    id: trackId(_id),
    name: 'Track',
    type: 'Audio',
    volume: 0,
    pan: 0,
    muted: false,
    soloed: false,
    clips: [],
    ...rest,
  };
}

describe('FadeOverlay', () => {
  const pixelsPerPPQN = 40 / PPQN; // 40px per beat

  it('renders fade-in triangle when fade_in_ppqn > 0', () => {
    const overlay = new FadeOverlay();
    const ctx = mockCtx();
    const paths: string[] = [];

    ctx.beginPath = () => { paths.push('begin'); };
    ctx.moveTo = () => { paths.push('move'); };
    ctx.lineTo = () => { paths.push('line'); };
    ctx.closePath = () => { paths.push('close'); };
    ctx.fill = () => { paths.push('fill'); };
    ctx.stroke = () => { paths.push('stroke'); };
    ctx.arc = () => {};
    ctx.fillRect = () => {};
    ctx.fillText = () => {};

    const clip = makeClip({
      id: 1, start_ppqn: 0, fade_in_ppqn: PPQN, // 1-beat fade-in
    });

    const tracks = [makeTrack({ id: 1, clips: [clip] })];

    overlay.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, new Set([1])); // selected

    // Fade-in should generate at least a path
    expect(paths.length).toBeGreaterThan(0);
  });

  it('renders fade-out triangle when fade_out_ppqn > 0', () => {
    const overlay = new FadeOverlay();
    const ctx = mockCtx();
    const paths: string[] = [];

    ctx.beginPath = () => { paths.push('begin'); };
    ctx.moveTo = () => { paths.push('move'); };
    ctx.lineTo = () => { paths.push('line'); };
    ctx.closePath = () => { paths.push('close'); };
    ctx.fill = () => { paths.push('fill'); };
    ctx.stroke = () => { paths.push('stroke'); };
    ctx.arc = () => {};
    ctx.fillRect = () => {};
    ctx.fillText = () => {};

    const clip = makeClip({
      id: 1, start_ppqn: 0, fade_out_ppqn: PPQN, // 1-beat fade-out
    });

    const tracks = [makeTrack({ id: 1, clips: [clip] })];

    overlay.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, new Set([1]));

    expect(paths.length).toBeGreaterThan(0);
  });

  it('does NOT render fade when both fades are zero', () => {
    const overlay = new FadeOverlay();
    const ctx = mockCtx();
    let pathCalled = false;

    ctx.beginPath = () => { pathCalled = true; };
    ctx.moveTo = () => {};
    ctx.lineTo = () => {};
    ctx.closePath = () => {};
    ctx.fill = () => {};
    ctx.stroke = () => {};
    ctx.arc = () => {};
    ctx.fillRect = () => {};

    const clip = makeClip({
      id: 1, start_ppqn: 0, fade_in_ppqn: 0, fade_out_ppqn: 0,
    });

    const tracks = [makeTrack({ id: 1, clips: [clip] })];

    overlay.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, new Set());

    expect(pathCalled).toBe(false);
  });

  it('hitTestFadeHandle detects fade-in handle hit', () => {
    const overlay = new FadeOverlay();

    const clip = makeClip({
      id: 1, start_ppqn: 0, fade_in_ppqn: PPQN, // 1-beat fade-in
    });

    const tracks = [makeTrack({ id: 1, clips: [clip] })];

    // Fade-in handle is at bottom-right of fade triangle:
    // clipX + fadeW, clipY + clipH
    // clipX = TRACK_LIST_WIDTH
    // fadeW = PPQN * pixelsPerPPQN = 40px
    const handleX = TRACK_LIST_WIDTH + PPQN * pixelsPerPPQN; // = TRACK_LIST_WIDTH + 40
    const handleY = RULER_HEIGHT + TRACK_HEIGHT - 1; // bottom of clip

    const hit = overlay.hitTestFadeHandle(
      tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
      handleX, handleY,
    );

    expect(hit).not.toBeNull();
    expect(hit!.clip.id.value).toBe(1);
    expect(hit!.side).toBe('in');
  });

  it('hitTestFadeHandle detects fade-out handle hit', () => {
    const overlay = new FadeOverlay();

    const clip = makeClip({
      id: 1, start_ppqn: 0, length_ppqn: PPQN * 4,
      fade_out_ppqn: PPQN,
    });

    const tracks = [makeTrack({ id: 1, clips: [clip] })];

    // Fade-out handle at bottom-left of fade-out triangle:
    // clipX + clipW - fadeW = TRACK_LIST_WIDTH + 4*40 - 40 = TRACK_LIST_WIDTH + 120
    const clipW = PPQN * 4 * pixelsPerPPQN; // 160px
    const fadeW = PPQN * pixelsPerPPQN; // 40px
    const handleX = TRACK_LIST_WIDTH + clipW - fadeW; // = TRACK_LIST_WIDTH + 120
    const handleY = RULER_HEIGHT + TRACK_HEIGHT - 1;

    const hit = overlay.hitTestFadeHandle(
      tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
      handleX, handleY,
    );

    expect(hit).not.toBeNull();
    expect(hit!.clip.id.value).toBe(1);
    expect(hit!.side).toBe('out');
  });

  it('hitTestFadeHandle returns null outside fade handle', () => {
    const overlay = new FadeOverlay();

    const clip = makeClip({
      id: 1, start_ppqn: 0, fade_in_ppqn: PPQN,
    });

    const tracks = [makeTrack({ id: 1, clips: [clip] })];

    // Hit in center of clip, not near fade handle
    const miss = overlay.hitTestFadeHandle(
      tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
      TRACK_LIST_WIDTH + 20, RULER_HEIGHT + TRACK_HEIGHT / 2,
    );

    expect(miss).toBeNull();
  });

  it('renders crossfade between overlapping clips', () => {
    const overlay = new FadeOverlay();
    const ctx = mockCtx();
    const ops: string[] = [];

    ctx.beginPath = () => { ops.push('begin'); };
    ctx.moveTo = () => { ops.push('move'); };
    ctx.lineTo = () => { ops.push('line'); };
    ctx.closePath = () => { ops.push('close'); };
    ctx.fill = () => { ops.push('fill'); };
    ctx.stroke = () => { ops.push('stroke'); };
    ctx.arc = () => {};
    ctx.fillRect = () => {};
    ctx.fillText = () => {};

    // Two clips overlapping by 1 beat
    const clipA = makeClip({ id: 1, start_ppqn: 0, length_ppqn: PPQN * 4 });
    const clipB = makeClip({ id: 2, start_ppqn: PPQN * 3, length_ppqn: PPQN * 4 });

    const tracks = [makeTrack({ id: 1, clips: [clipA, clipB] })];

    overlay.render(ctx, tracks, {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 2000,
      viewHeight: 600,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    }, new Set());

    // Crossfade should render: beginPath, moveTo, lineTo, etc.
    // The crossfade region spans 1 beat (overlap)
    expect(ops).toContain('begin');
    expect(ops).toContain('move');
    expect(ops).toContain('fill');
  });
});
