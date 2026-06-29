// ArrangementView — Integration Tests
import { describe, it, expect, beforeEach } from 'vitest';
import { ArrangementView } from '../../src/views/ArrangementView/index.js';
import {
  PPQN,
  TRACK_LIST_WIDTH,
  ArrangementDataSource,
  AnyTrackData,
  AnyClipData,
  isGroupTrack,
} from '../../src/views/ArrangementView/types.js';
import { clipId, trackId } from '../../src/views/ArrangementView/types.js';

// ─── Mock Data Source ───────────────────────────────────────────

function makeClip(overrides: Partial<AnyClipData> & { id: number; start_ppqn: number }): AnyClipData {
  const { id: _id, ...rest } = overrides;
  return {
    id: clipId(_id),
    start_ppqn: _id, /* placeholder, overridden by rest */
    length_ppqn: PPQN * 4,
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

function makeTrack(overrides: Partial<AnyTrackData> & { id: number; clips?: AnyClipData[] }): AnyTrackData {
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

class MockDataSource implements ArrangementDataSource {
  private tracks: AnyTrackData[] = [];

  constructor(tracks: AnyTrackData[]) {
    this.tracks = tracks;
  }

  getTracks(): AnyTrackData[] {
    return this.tracks;
  }

  getVisibleTracks(): AnyTrackData[] {
    // Collect IDs of children of folded groups
    const foldedChildIds = new Set<number>();
    for (const t of this.tracks) {
      if (isGroupTrack(t) && t.folded) {
        for (const c of t.children) {
          foldedChildIds.add(c.value);
        }
      }
    }
    // Filter out children of folded groups
    return this.tracks.filter(t => !foldedChildIds.has(t.id.value));
  }

  getPlayheadPPQN(): number {
    return 0;
  }

  getPixelsPerPPQN(): number {
    return 40 / PPQN;
  }

  isGridSnapEnabled(): boolean {
    return true;
  }

  getGridPpqn(): number {
    return PPQN / 4; // 16th notes
  }
}

describe('ArrangementView', () => {
  it('creates with data source and has correct name', () => {
    const ds = new MockDataSource([]);
    const view = new ArrangementView(ds);

    expect(view.name).toBe('Arrangement');
  });

  it('returns selection state', () => {
    const ds = new MockDataSource([]);
    const view = new ArrangementView(ds);

    const selection = view.getSelection();
    expect(selection.selectedClipIds).toBeDefined();
    expect(selection.selectedClipIds.size).toBe(0);
    expect(selection.hoveredClipId).toBeNull();
  });

  it('updates data source', () => {
    const ds1 = new MockDataSource([]);
    const view = new ArrangementView(ds1);

    const ds2 = new MockDataSource([
      makeTrack({ id: 1, clips: [makeClip({ id: 10, start_ppqn: 0 })] }),
    ]);

    view.setDataSource(ds2);

    const tracks = ds2.getTracks();
    expect(tracks.length).toBe(1);
    expect(tracks[0].clips.length).toBe(1);
  });

  it('handles activate/deactivate without crashing', () => {
    const ds = new MockDataSource([]);
    const view = new ArrangementView(ds);

    expect(() => view.activate()).not.toThrow();
    expect(() => view.render()).not.toThrow();
    expect(() => view.deactivate()).not.toThrow();
  });
});

describe('Visible tracks with folded GroupTrack', () => {
  it('hides children when GroupTrack is folded', () => {
    const groupTrack = makeTrack({
      id: 1,
      name: 'Group 1',
      type: 'Group',
      clips: [],
      children: [trackId(2), trackId(3)],
      group_mode: 'Summing',
      folded: true,
    }) as AnyTrackData;

    const child1 = makeTrack({ id: 2, name: 'Child 1', clips: [] });
    const child2 = makeTrack({ id: 3, name: 'Child 2', clips: [] });

    const allTracks = [groupTrack, child1, child2];
    const ds = new MockDataSource(allTracks);

    const visible = ds.getVisibleTracks();
    const visibleIds = visible.map(t => t.id.value);

    // Group should be visible, children should not
    expect(visibleIds).toContain(1);
    expect(visibleIds).not.toContain(2);
    expect(visibleIds).not.toContain(3);
    expect(visible.length).toBe(1);
  });

  it('shows children when GroupTrack is NOT folded', () => {
    const groupTrack = makeTrack({
      id: 1,
      name: 'Group 1',
      type: 'Group',
      clips: [],
      children: [trackId(2)],
      group_mode: 'Summing',
      folded: false,
    }) as AnyTrackData;

    const child1 = makeTrack({ id: 2, name: 'Child 1', clips: [] });

    const allTracks = [groupTrack, child1];
    const ds = new MockDataSource(allTracks);

    const visible = ds.getVisibleTracks();
    const visibleIds = visible.map(t => t.id.value);

    expect(visibleIds).toContain(1);
    expect(visibleIds).toContain(2);
    expect(visible.length).toBe(2);
  });
});

describe('ArrangementView — capture from session', () => {
  it('captureFromSession creates a SessionCaptureClip via callback', () => {
    const ds = new MockDataSource([]);
    const view = new ArrangementView(ds);

    let capturedClip: AnyClipData | null = null;
    let capturedTrack: TrackID | null = null;
    let capturedStartPPQN: number | null = null;

    view.onCaptureFromSession = (
      clip: AnyClipData, track: TrackID, startPPQN: number,
    ) => {
      capturedClip = clip;
      capturedTrack = track;
      capturedStartPPQN = startPPQN;
    };

    const sourceClip = makeClip({ id: 10, start_ppqn: 0, length_ppqn: PPQN });

    view.captureFromSession(
      trackId(2), // source track
      { value: 3 }, // source scene
      sourceClip,
      trackId(5), // target track
      PPQN * 8, // target start
    );

    expect(capturedClip).not.toBeNull();
    expect(capturedClip!.type).toBe('SessionCapture');
    expect((capturedClip! as any).capture_source_track_id.value).toBe(2);
    expect((capturedClip! as any).capture_source_scene_id.value).toBe(3);
    expect(capturedTrack!.value).toBe(5);
    expect(capturedStartPPQN).toBe(PPQN * 8);
  });

  it('captureFromSession preserves clip properties', () => {
    const ds = new MockDataSource([]);
    const view = new ArrangementView(ds);

    let capturedClip: AnyClipData | null = null;
    view.onCaptureFromSession = (clip: AnyClipData) => {
      capturedClip = clip;
    };

    const sourceClip = makeClip({
      id: 20, start_ppqn: 0, length_ppqn: PPQN * 2,
      name: 'My Session Clip', gain: 0.75,
    });

    view.captureFromSession(
      trackId(1), { value: 1 }, sourceClip,
      trackId(1), 0,
    );

    expect(capturedClip).not.toBeNull();
    expect(capturedClip!.name).toContain('My Session Clip');
    expect(capturedClip!.gain).toBe(0.75);
    expect(capturedClip!.length_ppqn).toBe(PPQN * 2);
  });

  it('captureFromSession emits callback with correct target position', () => {
    const ds = new MockDataSource([]);
    const view = new ArrangementView(ds);

    let capturedStart: number | null = null;
    view.onCaptureFromSession = (_clip, _track, startPPQN) => {
      capturedStart = startPPQN;
    };

    const sourceClip = makeClip({ id: 30, start_ppqn: 0, length_ppqn: PPQN });

    view.captureFromSession(
      trackId(1), { value: 1 }, sourceClip,
      trackId(3), PPQN * 16,
    );

    expect(capturedStart).toBe(PPQN * 16);
  });
});
