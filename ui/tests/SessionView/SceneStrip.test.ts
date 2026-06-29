// SceneStrip — Unit Tests
import { describe, it, expect } from 'vitest';
import { SceneStrip } from '../../src/views/SessionView/SceneStrip.js';
import {
  SLOT_HEIGHT,
  COLUMN_HEADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  sceneId,
  SessionDataSource,
  SceneData,
  ClipSlotData,
  SceneSummary,
  CrossfaderData,
} from '../../src/views/SessionView/types.js';

import { TrackData, TrackID } from '../../src/views/ArrangementView/types.js';

// ─── Mock ────────────────────────────────────────────────────────

class MockStripDS implements SessionDataSource {
  private summaries = new Map<number, SceneSummary>();

  constructor(scenes: SceneData[]) {
    for (const s of scenes) {
      this.summaries.set(s.id.value, {
        scene_id: s.id, is_playing: false, any_triggered: false,
      });
    }
  }

  getTracks(): TrackData[] { return []; }
  getScenes(): SceneData[] { return []; }
  getSlot(): ClipSlotData {
    return { clip_name: '', clip_color: 0, has_clip: false, state: 'Stopped', follow_action: 'None' };
  }
  getSceneSummary(sid: { value: number }): SceneSummary {
    return this.summaries.get(sid.value) ?? {
      scene_id: sid, is_playing: false, any_triggered: false,
    };
  }
  getCrossfader(): CrossfaderData { return { position: 0, curve: 'EqualPower' }; }
  setCrossfaderPosition(): void {}
  launchClip(): void {}
  launchScene(): void {}
}

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

function makeScene(overrides: Partial<SceneData> & { id: number }): SceneData {
  const { id: _id, ...rest } = overrides;
  return {
    id: sceneId(_id), name: `Scene ${_id}`,
    color: 0, order_index: _id - 1, follow_action: 'None',
    ...rest,
  } as SceneData;
}

describe('SceneStrip', () => {
  it('renders without crashing with empty scenes', () => {
    const strip = new SceneStrip();
    const ctx = mockCtx();
    const ds = new MockStripDS([]);

    expect(() => strip.render(ctx, [], ds, {
      scrollY: 0, viewHeight: 600, hasMoreScenes: false,
    })).not.toThrow();
  });

  it('renders with multiple scenes', () => {
    const strip = new SceneStrip();
    const ctx = mockCtx();
    const scenes = [1, 2, 3].map(i => makeScene({ id: i }));
    const ds = new MockStripDS(scenes);

    expect(() => strip.render(ctx, scenes, ds, {
      scrollY: 0, viewHeight: 600, hasMoreScenes: false,
    })).not.toThrow();
  });

  it('renders with scrolled offset', () => {
    const strip = new SceneStrip();
    const ctx = mockCtx();
    const scenes = [1, 2, 3, 4, 5].map(i => makeScene({ id: i }));
    const ds = new MockStripDS(scenes);

    expect(() => strip.render(ctx, scenes, ds, {
      scrollY: SLOT_HEIGHT * 2, viewHeight: 600, hasMoreScenes: true,
    })).not.toThrow();
  });

  it('hitTestScene returns correct scene for click position', () => {
    const strip = new SceneStrip();
    const scenes = [10, 20, 30].map(i => makeScene({ id: i }));

    // Click on scene 20 (row 1)
    const px = SCENE_STRIP_WIDTH / 2;
    const py = COLUMN_HEADER_HEIGHT + SLOT_HEIGHT * 1 + SLOT_HEIGHT / 2;

    const hit = strip.hitTestScene(px, py, scenes, 0);
    expect(hit).not.toBeNull();
    expect(hit!.id.value).toBe(20);

    // Click beyond last scene
    const miss = strip.hitTestScene(
      px, COLUMN_HEADER_HEIGHT + SLOT_HEIGHT * 5, scenes, 0,
    );
    expect(miss).toBeNull();
  });

  it('hitTestScene returns null for clicks outside scene strip width', () => {
    const strip = new SceneStrip();
    const scenes = [makeScene({ id: 1 })];

    const hit = strip.hitTestScene(
      SCENE_STRIP_WIDTH + 10,
      COLUMN_HEADER_HEIGHT + SLOT_HEIGHT / 2,
      scenes, 0,
    );
    expect(hit).toBeNull();
  });

  it('hitTestScene returns null for clicks in column header area', () => {
    const strip = new SceneStrip();
    const scenes = [makeScene({ id: 1 })];

    const hit = strip.hitTestScene(10, 5, scenes, 0);
    expect(hit).toBeNull();
  });

  it('hitTestLaunchButton detects button clicks', () => {
    const strip = new SceneStrip();
    const scenes = [10, 20, 30].map(i => makeScene({ id: i }));

    // Click on the launch button of scene 20 (row 1)
    const px = 16; // button center x
    const py = COLUMN_HEADER_HEIGHT + SLOT_HEIGHT * 1 + SLOT_HEIGHT / 2;

    const hit = strip.hitTestLaunchButton(px, py, scenes, 0);
    expect(hit).not.toBeNull();
    expect(hit!.id.value).toBe(20);
  });

  it('hitTestLaunchButton returns null outside button radius', () => {
    const strip = new SceneStrip();
    const scenes = [makeScene({ id: 1 })];

    // Far from the button center
    const hit = strip.hitTestLaunchButton(
      60, COLUMN_HEADER_HEIGHT + SLOT_HEIGHT / 2, scenes, 0,
    );
    expect(hit).toBeNull();
  });
});
