// SelectionManager — Unit Tests
import { describe, it, expect, beforeEach } from 'vitest';
import { SelectionManager } from '../../src/views/ArrangementView/SelectionManager.js';
import {
  PPQN,
  TRACK_HEIGHT,
  RULER_HEIGHT,
  TRACK_LIST_WIDTH,
  snapToGrid,
  AnyTrackData,
  AnyClipData,
} from '../../src/views/ArrangementView/types.js';
import { clipId, trackId } from '../../src/views/ArrangementView/types.js';

function makeClip(overrides: Partial<AnyClipData> & { id: number; start_ppqn: number }): AnyClipData {
  const { id: _id, ...rest } = overrides;
  return {
    id: clipId(_id),
    start_ppqn: _id, /* placeholder, overridden by rest */
    length_ppqn: PPQN,
    name: 'Test',
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

function makeTrack(overrides: { id: number; clips: AnyClipData[] }): AnyTrackData {
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
  };
}

describe('SelectionManager', () => {
  let mgr: SelectionManager;
  let tracks: AnyTrackData[];
  const pixelsPerPPQN = 40 / PPQN; // 40px per beat

  beforeEach(() => {
    mgr = new SelectionManager();
    tracks = [
      makeTrack({
        id: 1,
        clips: [
          makeClip({ id: 10, start_ppqn: 0 }),
          makeClip({ id: 11, start_ppqn: PPQN * 2 }),
        ],
      }),
      makeTrack({
        id: 2,
        clips: [
          makeClip({ id: 20, start_ppqn: PPQN }),
        ],
      }),
    ];
  });

  describe('click selection', () => {
    it('selects a clip on click', () => {
      // Click on clip 10 (first clip on track 1, starts at PPQN 0)
      const clickX = TRACK_LIST_WIDTH + 5; // 5px into the clip
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2; // Middle of track 1

      const hit = mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      expect(hit).not.toBeNull();
      expect(hit!.id.value).toBe(10);
      expect(mgr.state.selectedClipIds.has(10)).toBe(true);
      expect(mgr.state.selectedClipIds.size).toBe(1);
    });

    it('replaces selection on click without SHIFT', () => {
      // Select clip 10 first
      mgr.selectOnly(makeClip({ id: 10, start_ppqn: 0 }));
      expect(mgr.state.selectedClipIds.has(10)).toBe(true);

      // Click on clip 20 (track 2)
      const clickX = TRACK_LIST_WIDTH + PPQN * pixelsPerPPQN + 5;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      // Clip 10 should be deselected, clip 20 selected
      expect(mgr.state.selectedClipIds.has(10)).toBe(false);
      expect(mgr.state.selectedClipIds.has(20)).toBe(true);
      expect(mgr.state.selectedClipIds.size).toBe(1);
    });

    it('extends selection with SHIFT+click', () => {
      // Select clip 10 first
      mgr.selectOnly(makeClip({ id: 10, start_ppqn: 0 }));

      // SHIFT-click on clip 20
      const clickX = TRACK_LIST_WIDTH + PPQN * pixelsPerPPQN + 5;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, true, clickX, clickY,
      );

      expect(mgr.state.selectedClipIds.has(10)).toBe(true);
      expect(mgr.state.selectedClipIds.has(20)).toBe(true);
      expect(mgr.state.selectedClipIds.size).toBe(2);
    });

    it('deselects on empty space click', () => {
      mgr.selectOnly(makeClip({ id: 10, start_ppqn: 0 }));

      // Click on empty space (after all clips)
      const clickX = TRACK_LIST_WIDTH + PPQN * 10 * pixelsPerPPQN;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      expect(mgr.state.selectedClipIds.size).toBe(0);
    });
  });

  describe('drag and snap', () => {
    it('starts drag on mouse down then mouse move', () => {
      const clickX = TRACK_LIST_WIDTH + 5;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      expect(mgr.isDragging).toBe(false); // hasn't moved yet

      // Move 100px right
      mgr.onMouseMove(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, clickX + 100, clickY,
      );

      expect(mgr.isDragging).toBe(true);
    });

    it('returns delta PPQN on drag with grid snap enabled (16th notes)', () => {
      const clickX = TRACK_LIST_WIDTH + 5;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        true, PPQN / 4, false, clickX, clickY,
      );

      // Drag to a position that should snap to the nearest 16th (240 PPQN)
      // 310 PPQN in pixels = TRACK_LIST_WIDTH + 310 * pixelsPerPPQN
      const dragPPQN = 310;
      const dragX = TRACK_LIST_WIDTH + dragPPQN * pixelsPerPPQN;

      const delta = mgr.onMouseMove(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        true, PPQN / 4, dragX, clickY,
      );

      expect(delta).not.toBeNull();
      if (delta !== null) {
        // With snap to 240 PPQN grid, 310 rounded to nearest 240 = 240
        // delta = 240 - 0 = 240
        expect(delta).toBe(240);
      }
    });

    it('computes correct delta without grid snap', () => {
      // Click at clip start (PPQN 0 corresponds to pixel TRACK_LIST_WIDTH)
      const clickX = TRACK_LIST_WIDTH;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      // Drag to 480 PPQN in pixels
      const dragPPQN = 480;
      const dragX = TRACK_LIST_WIDTH + dragPPQN * pixelsPerPPQN;

      const delta = mgr.onMouseMove(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, dragX, clickY,
      );

      expect(delta).not.toBeNull();
      if (delta !== null) {
        // clip started at 0, dragged to 480 PPQN
        expect(Math.abs(delta - 480)).toBeLessThan(10);
      }
    });
  });

  describe('mouse up finalization', () => {
    it('returns new positions on mouse up after drag', () => {
      const clickX = TRACK_LIST_WIDTH; // clip 10 starts at PPQN 0
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      // Drag 480 PPQN right
      const dragX = TRACK_LIST_WIDTH + 480 * pixelsPerPPQN;
      mgr.onMouseMove(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, dragX, clickY,
      );

      const result = mgr.onMouseUp(tracks, pixelsPerPPQN, false, PPQN / 4);

      expect(result).not.toBeNull();
      expect(result!.has(10)).toBe(true);
      expect(result!.get(10)! > 0).toBe(true);
    });

    it('returns null when mouse up without drag', () => {
      const clickX = TRACK_LIST_WIDTH + 5;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      // Mouse up without moving
      const result = mgr.onMouseUp(tracks, pixelsPerPPQN, false, PPQN / 4);
      expect(result).toBeNull();
    });
  });

  describe('cancel and clear', () => {
    it('clears selection on Escape', () => {
      mgr.selectOnly(makeClip({ id: 10, start_ppqn: 0 }));
      expect(mgr.state.selectedClipIds.size).toBe(1);

      mgr.clearSelection();
      expect(mgr.state.selectedClipIds.size).toBe(0);
    });

    it('cancels drag on cancelDrag', () => {
      const clickX = TRACK_LIST_WIDTH + 5;
      const clickY = RULER_HEIGHT + TRACK_HEIGHT / 2;

      mgr.onMouseDown(
        tracks, 0, 0, pixelsPerPPQN, TRACK_LIST_WIDTH,
        false, PPQN / 4, false, clickX, clickY,
      );

      mgr.cancelDrag();
      expect(mgr.isDragging).toBe(false);
    });
  });
});

describe('snapToGrid', () => {
  it('snaps to nearest grid point', () => {
    // Grid 240 PPQN (16th notes)
    expect(snapToGrid(0, 240)).toBe(0);
    expect(snapToGrid(100, 240)).toBe(0);   // nearest is 0
    expect(snapToGrid(120, 240)).toBe(240); // nearest 240
    expect(snapToGrid(239, 240)).toBe(240); // nearest 240
    expect(snapToGrid(240, 240)).toBe(240);
    expect(snapToGrid(310, 240)).toBe(240); // nearest 240 (Scenario: Drag snaps to grid)
    expect(snapToGrid(490, 240)).toBe(480); // nearest 480
    expect(snapToGrid(960, 240)).toBe(960);
  });

  it('returns exact value when grid is 0 or negative', () => {
    expect(snapToGrid(123, 0)).toBe(123);
    expect(snapToGrid(123, -1)).toBe(123);
  });

  it('snaps to PPQN (beat) grid', () => {
    expect(snapToGrid(1, PPQN)).toBe(0);
    expect(snapToGrid(480, PPQN)).toBe(960);
    expect(snapToGrid(960, PPQN)).toBe(960);
    expect(snapToGrid(1440, PPQN)).toBe(1920);
  });
});
