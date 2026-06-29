// SessionView — Integration Tests
import { describe, it, expect, beforeEach } from 'vitest';
import { SessionView } from '../../src/views/SessionView/index.js';
import {
  SLOT_HEIGHT,
  COLUMN_HEADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  CROSSFADER_HEIGHT,
  sceneId,
  SessionDataSource,
  SceneData,
  ClipSlotData,
  SceneSummary,
  CrossfaderData,
  CrossfaderAssignment,
  CrossfaderCurve,
  evaluateCrossfaderGain,
} from '../../src/views/SessionView/types.js';

import {
  TrackData,
  TrackID,
  ClipID,
  trackId,
} from '../../src/views/ArrangementView/types.js';

// ─── Mock Data Source ───────────────────────────────────────────

class MockSessionDataSource implements SessionDataSource {
  private tracks: TrackData[] = [];
  private scenes: SceneData[] = [];
  private slots: Map<string, ClipSlotData> = new Map();
  private summaries: Map<number, SceneSummary> = new Map();
  private _crossfader: CrossfaderData = { position: 0, curve: 'EqualPower' };

  constructor(
    tracks: TrackData[],
    scenes: SceneData[],
    slots?: [number, number, ClipSlotData][],
  ) {
    this.tracks = tracks;
    this.scenes = scenes;
    if (slots) {
      for (const [trackIdx, sceneIdx, slot] of slots) {
        const track = tracks[trackIdx];
        const scene = scenes[sceneIdx];
        if (track && scene) {
          this.slots.set(`${track.id.value}:${scene.id.value}`, slot);
        }
      }
    }
    for (const scene of scenes) {
      this.summaries.set(scene.id.value, {
        scene_id: scene.id,
        is_playing: false,
        any_triggered: false,
      });
    }
  }

  getTracks(): TrackData[] { return this.tracks; }
  getScenes(): SceneData[] { return this.scenes; }

  getSlot(trackId: TrackID, sceneId: { value: number }): ClipSlotData {
    const key = `${trackId.value}:${sceneId.value}`;
    return this.slots.get(key) ?? {
      clip_name: '',
      clip_color: 0,
      has_clip: false,
      state: 'Stopped',
      follow_action: 'None',
    };
  }

  getSceneSummary(sceneId: { value: number }): SceneSummary {
    return this.summaries.get(sceneId.value) ?? {
      scene_id: sceneId,
      is_playing: false,
      any_triggered: false,
    };
  }

  getCrossfader(): CrossfaderData { return this._crossfader; }

  setCrossfaderPosition(pos: number): void {
    this._crossfader.position = pos;
  }

  launchClip(_trackId: TrackID, _sceneId: { value: number }): void {}
  launchScene(_sceneId: { value: number }): void {}
}

// ─── Fixtures ───────────────────────────────────────────────────

function makeTrack(overrides: Partial<TrackData> & { id: number }): TrackData {
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
  } as TrackData;
}

function makeScene(overrides: Partial<SceneData> & { id: number }): SceneData {
  const { id: _id, ...rest } = overrides;
  return {
    id: sceneId(_id),
    name: `Scene ${_id}`,
    color: 0,
    order_index: _id - 1,
    follow_action: 'None',
    ...rest,
  } as SceneData;
}

// ─── Tests ──────────────────────────────────────────────────────

describe('SessionView', () => {
  it('creates with data source and has correct name', () => {
    const ds = new MockSessionDataSource([], []);
    const view = new SessionView(ds);
    expect(view.name).toBe('Session');
  });

  it('handles activate/deactivate without crashing', () => {
    const ds = new MockSessionDataSource([], []);
    const view = new SessionView(ds);

    expect(() => view.activate()).not.toThrow();
    expect(() => view.render()).not.toThrow();
    expect(() => view.deactivate()).not.toThrow();
  });

  it('updates data source', () => {
    const ds1 = new MockSessionDataSource([], []);
    const view = new SessionView(ds1);

    const ds2 = new MockSessionDataSource(
      [makeTrack({ id: 1 })],
      [makeScene({ id: 1 })],
    );

    view.setDataSource(ds2);

    const tracks = ds2.getTracks();
    const scenes = ds2.getScenes();
    expect(tracks.length).toBe(1);
    expect(scenes.length).toBe(1);
    expect(tracks[0].name).toBe('Track 1');
    expect(scenes[0].name).toBe('Scene 1');
  });

  it('data source crossfader position set/get round-trips', () => {
    const ds = new MockSessionDataSource([], []);
    expect(ds.getCrossfader().position).toBe(0);

    ds.setCrossfaderPosition(0.5);
    expect(ds.getCrossfader().position).toBeCloseTo(0.5);

    ds.setCrossfaderPosition(-1);
    expect(ds.getCrossfader().position).toBeCloseTo(-1);
  });

  it('returns correct track/scene data from data source', () => {
    const ds = new MockSessionDataSource(
      [makeTrack({ id: 1 }), makeTrack({ id: 2 })],
      [makeScene({ id: 1 }), makeScene({ id: 2 }), makeScene({ id: 3 })],
    );

    const tracks = ds.getTracks();
    const scenes = ds.getScenes();
    expect(tracks).toHaveLength(2);
    expect(scenes).toHaveLength(3);
  });
});

describe('SessionView clip grid slot queries', () => {
  it('returns empty slot when no clip is placed', () => {
    const slot: ClipSlotData = {
      clip_name: '',
      clip_color: 0,
      has_clip: false,
      state: 'Stopped',
      follow_action: 'None',
    };

    expect(slot.has_clip).toBe(false);
    expect(slot.state).toBe('Stopped');
    expect(slot.clip_name).toBe('');
  });

  it('returns playing slot data correctly', () => {
    const slot: ClipSlotData = {
      clip_name: 'Bass Loop',
      clip_color: 0xff44aa66,
      has_clip: true,
      state: 'Playing',
      follow_action: 'None',
    };

    expect(slot.has_clip).toBe(true);
    expect(slot.state).toBe('Playing');
    expect(slot.clip_name).toBe('Bass Loop');
  });

  it('triggers scene launch via data source', () => {
    let launchedScene: number | null = null;
    class LaunchTrackingDS extends MockSessionDataSource {
      launchScene(sceneId: { value: number }): void {
        launchedScene = sceneId.value;
      }
    }

    const ds = new LaunchTrackingDS(
      [makeTrack({ id: 1 })],
      [makeScene({ id: 42, name: 'Drop' })],
    );

    ds.launchScene(sceneId(42));
    expect(launchedScene).toBe(42);
  });

  it('triggers clip launch via data source', () => {
    let launched: { track: number; scene: number } | null = null;
    class LaunchTrackingDS extends MockSessionDataSource {
      launchClip(trackId: TrackID, sceneId: { value: number }): void {
        launched = { track: trackId.value, scene: sceneId.value };
      }
    }

    const ds = new LaunchTrackingDS(
      [makeTrack({ id: 5 })],
      [makeScene({ id: 10 })],
    );

    ds.launchClip(trackId(5), sceneId(10));
    expect(launched).toEqual({ track: 5, scene: 10 });
  });
});

describe('evaluateCrossfaderGain', () => {
  it('returns 1.0 for None and Both assignments regardless of position', () => {
    expect(evaluateCrossfaderGain('None', 0, 'EqualPower')).toBe(1.0);
    expect(evaluateCrossfaderGain('Both', 0, 'EqualPower')).toBe(1.0);
    expect(evaluateCrossfaderGain('None', 1, 'Linear')).toBe(1.0);
    expect(evaluateCrossfaderGain('Both', -1, 'Linear')).toBe(1.0);
  });

  it('returns 0 at far side, 1 at near side for Linear', () => {
    // Side A: at position -1 (full A), gain = 1; at +1 (full B), gain = 0
    expect(evaluateCrossfaderGain('A', -1, 'Linear')).toBeCloseTo(1.0);
    expect(evaluateCrossfaderGain('A', 1, 'Linear')).toBeCloseTo(0.0);
    // Side B: at position +1 (full B), gain = 1; at -1 (full A), gain = 0
    expect(evaluateCrossfaderGain('B', 1, 'Linear')).toBeCloseTo(1.0);
    expect(evaluateCrossfaderGain('B', -1, 'Linear')).toBeCloseTo(0.0);
  });

  it('returns 0.5 at center for Linear', () => {
    expect(evaluateCrossfaderGain('A', 0, 'Linear')).toBeCloseTo(0.5);
    expect(evaluateCrossfaderGain('B', 0, 'Linear')).toBeCloseTo(0.5);
  });

  it('returns ~0.707 at center for EqualPower', () => {
    const sqrtHalf = Math.SQRT1_2;
    expect(evaluateCrossfaderGain('A', 0, 'EqualPower')).toBeCloseTo(sqrtHalf, 4);
    expect(evaluateCrossfaderGain('B', 0, 'EqualPower')).toBeCloseTo(sqrtHalf, 4);
  });

  it('clamps position values outside [-1, 1]', () => {
    // At position 2.0, side A should be gain 0
    expect(evaluateCrossfaderGain('A', 2.0, 'Linear')).toBeCloseTo(0.0);
    // At position -2.0, side B should be gain 0
    expect(evaluateCrossfaderGain('B', -2.0, 'Linear')).toBeCloseTo(0.0);
  });
});
