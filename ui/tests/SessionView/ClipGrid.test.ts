// ClipGrid — Unit Tests
import { describe, it, expect } from 'vitest';
import { ClipGrid } from '../../src/views/SessionView/ClipGrid.js';
import {
  SLOT_HEIGHT,
  COLUMN_HEADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  MIN_SLOT_WIDTH,
  sceneId,
  SessionDataSource,
  SceneData,
  ClipSlotData,
  SceneSummary,
  CrossfaderData,
  SessionViewport,
} from '../../src/views/SessionView/types.js';

import {
  TrackData,
  TrackID,
  trackId,
  ClipID,
} from '../../src/views/ArrangementView/types.js';

// ─── Mock Data Source ───────────────────────────────────────────

class MockGridDS implements SessionDataSource {
  constructor(
    private tracks: TrackData[],
    private scenes: SceneData[],
    private slots: Map<string, ClipSlotData> = new Map(),
  ) {}

  getTracks(): TrackData[] { return this.tracks; }
  getScenes(): SceneData[] { return this.scenes; }

  getSlot(trackId: TrackID, sceneId: { value: number }): ClipSlotData {
    return this.slots.get(`${trackId.value}:${sceneId.value}`) ?? {
      clip_name: '', clip_color: 0, has_clip: false,
      state: 'Stopped', follow_action: 'None',
    };
  }

  getSceneSummary(): SceneSummary {
    return { scene_id: sceneId(0), is_playing: false, any_triggered: false };
  }

  getCrossfader(): CrossfaderData { return { position: 0, curve: 'EqualPower' }; }
  setCrossfaderPosition(): void {}
  launchClip(): void {}
  launchScene(): void {}
}

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

function makeTrack(overrides: Partial<TrackData> & { id: number }): TrackData {
  return {
    id: trackId(overrides.id),
    name: `Track ${overrides.id}`,
    type: 'Audio',
    volume: 0, pan: 0, muted: false, soloed: false, clips: [],
    ...overrides,
  } as TrackData;
}

function makeScene(overrides: Partial<SceneData> & { id: number }): SceneData {
  const { id: _id, ...rest } = overrides;
  return {
    id: sceneId(_id),
    name: `Scene ${_id}`,
    color: 0, order_index: _id - 1, follow_action: 'None',
    ...rest,
  } as SceneData;
}

describe('ClipGrid', () => {
  it('renders without crashing with empty data', () => {
    const grid = new ClipGrid();
    const ctx = mockCtx();
    const ds = new MockGridDS([], []);

    const viewport: SessionViewport = {
      scrollX: 0, scrollY: 0, viewWidth: 800, viewHeight: 600,
    };

    expect(() => grid.render(ctx, [], [], ds, {
      viewport, trackCount: 0, columnWidth: MIN_SLOT_WIDTH,
    })).not.toThrow();
  });

  it('renders with one track and one scene', () => {
    const grid = new ClipGrid();
    const ctx = mockCtx();
    const tracks = [makeTrack({ id: 1 })];
    const scenes = [makeScene({ id: 1 })];
    const ds = new MockGridDS(tracks, scenes);

    const viewport: SessionViewport = {
      scrollX: 0, scrollY: 0, viewWidth: 800, viewHeight: 600,
    };

    expect(() => grid.render(ctx, tracks, scenes, ds, {
      viewport, trackCount: 1, columnWidth: 120,
    })).not.toThrow();
  });

  it('renders with multiple tracks and scenes', () => {
    const grid = new ClipGrid();
    const ctx = mockCtx();
    const tracks = [1, 2, 3].map(i => makeTrack({ id: i }));
    const scenes = [1, 2, 3, 4].map(i => makeScene({ id: i }));
    const ds = new MockGridDS(tracks, scenes);

    const viewport: SessionViewport = {
      scrollX: 0, scrollY: 0, viewWidth: 800, viewHeight: 600,
    };

    expect(() => grid.render(ctx, tracks, scenes, ds, {
      viewport, trackCount: 3, columnWidth: 120,
    })).not.toThrow();
  });

  it('renders playing clip slot with green indicator', () => {
    const grid = new ClipGrid();
    const ctx = mockCtx();
    const tracks = [makeTrack({ id: 1 })];
    const scenes = [makeScene({ id: 1 })];

    const slots = new Map<string, ClipSlotData>();
    slots.set('1:1', {
      clip_name: 'Bass',
      clip_color: 0xff44aa66,
      has_clip: true,
      state: 'Playing',
      follow_action: 'None',
    });

    const ds = new MockGridDS(tracks, scenes, slots);

    const viewport: SessionViewport = {
      scrollX: 0, scrollY: 0, viewWidth: 800, viewHeight: 600,
    };

    expect(() => grid.render(ctx, tracks, scenes, ds, {
      viewport, trackCount: 1, columnWidth: 120,
    })).not.toThrow();
  });

  it('hitTestSlot returns correct track and scene for click position', () => {
    const grid = new ClipGrid();
    const scenes = [1, 2, 3].map(i => makeScene({ id: i }));
    const columnWidth = 150;

    // Click on scene 2 (row 1), track 1 (col 0)
    const px = SCENE_STRIP_WIDTH + columnWidth / 2;
    const py = COLUMN_HEADER_HEIGHT + SLOT_HEIGHT * 1 + SLOT_HEIGHT / 2;

    const hit = grid.hitTestSlot(px, py, scenes, 0, 0, columnWidth);
    expect(hit).not.toBeNull();
    expect(hit!.trackIndex).toBe(0);
    expect(hit!.sceneId).toBe(2);

    // Click on scene 3 (row 2), track 2 (col 1)
    const px2 = SCENE_STRIP_WIDTH + columnWidth * 1.5;
    const py2 = COLUMN_HEADER_HEIGHT + SLOT_HEIGHT * 2 + SLOT_HEIGHT / 2;

    const hit2 = grid.hitTestSlot(px2, py2, scenes, 0, 0, columnWidth);
    expect(hit2).not.toBeNull();
    expect(hit2!.trackIndex).toBe(1);
    expect(hit2!.sceneId).toBe(3);
  });

  it('hitTestSlot returns null for click in scene strip area', () => {
    const grid = new ClipGrid();
    const scenes = [makeScene({ id: 1 })];

    const hit = grid.hitTestSlot(10, COLUMN_HEADER_HEIGHT + 10, scenes, 0, 0, 120);
    expect(hit).toBeNull();
  });

  it('hitTestSlot returns null for click in column header area', () => {
    const grid = new ClipGrid();
    const scenes = [makeScene({ id: 1 })];

    const hit = grid.hitTestSlot(
      SCENE_STRIP_WIDTH + 10, 5, scenes, 0, 0, 120,
    );
    expect(hit).toBeNull();
  });

  it('hitTestSlot returns null when clicking beyond last scene', () => {
    const grid = new ClipGrid();
    const scenes = [makeScene({ id: 1 })];

    const hit = grid.hitTestSlot(
      SCENE_STRIP_WIDTH + 10,
      COLUMN_HEADER_HEIGHT + SLOT_HEIGHT * 5,
      scenes, 0, 0, 120,
    );
    expect(hit).toBeNull();
  });
});
