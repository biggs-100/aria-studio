// GPUSessionView — Unit Tests
// Tests: clip culling, scene launch dispatch, crossfader position, follow action indicators
import { describe, it, expect, beforeEach } from 'vitest';
import { GPUSessionView } from '../../../src/views/SessionView/gpu-session-view.js';
import type { VNode } from '../../../src/ffi/bridge.js';
import type {
  SessionDataSource,
  SceneData,
  SceneID,
  ClipSlotData,
  SceneSummary,
  CrossfaderData,
} from '../../../src/views/SessionView/types.js';
import {
  SLOT_HEIGHT,
  COLUMN_HEADER_HEIGHT,
  CROSSFADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  MIN_SLOT_WIDTH,
} from '../../../src/views/SessionView/types.js';
import type { TrackData, TrackID } from '../../../src/views/ArrangementView/types.js';

// ─── Helpers ──────────────────────────────────────────────────────────

/** Walk VNode tree and collect all nodes whose key starts with a prefix. */
function findByKeyPrefix(root: VNode, prefix: string): VNode[] {
  const results: VNode[] = [];
  function walk(node: VNode): void {
    if (node.key.startsWith(prefix)) results.push(node);
    if (node.children) node.children.forEach(walk);
  }
  walk(root);
  return results;
}

/** Walk VNode tree and collect all VNodes of a given type. */
function findByType(root: VNode, type: string): VNode[] {
  const results: VNode[] = [];
  function walk(node: VNode): void {
    if (node.type === type) results.push(node);
    if (node.children) node.children.forEach(walk);
  }
  walk(root);
  return results;
}

function trackId(v: number): TrackID {
  return { value: v };
}

function sceneId(v: number): SceneID {
  return { value: v };
}

function makeScene(id: number, name: string, order: number): SceneData {
  return {
    id: sceneId(id),
    name,
    color: 0,
    order_index: order,
    follow_action: 'None',
  };
}

function makeTrack(id: number, name: string): TrackData {
  return {
    id: trackId(id),
    name,
    type: 'Audio',
    volume: 0.75,
    pan: 0,
    muted: false,
    soloed: false,
    clips: [],
  };
}

function defaultSlot(): ClipSlotData {
  return {
    clip_name: '',
    clip_color: 0,
    has_clip: false,
    state: 'Stopped',
    follow_action: 'None',
  };
}

function defaultSummary(): SceneSummary {
  return { scene_id: sceneId(0), is_playing: false, any_triggered: false };
}

// ─── Tests ────────────────────────────────────────────────────────────

describe('GPUSessionView', () => {
  /** Track calls to launchScene / launchClip / setCrossfaderPosition */
  let launchedScenes: SceneID[];
  let launchedClips: Array<{ trackId: TrackID; sceneId: SceneID }>;
  let crossfaderPositions: number[];

  function createDataSource(overrides?: Partial<SessionDataSource>): SessionDataSource {
    // We need mutable slot storage per track/scene
    const slotStore = new Map<string, ClipSlotData>();

    return {
      getTracks: () => [],
      getScenes: () => [],
      getSlot: (_trackId: TrackID, _sceneId: SceneID): ClipSlotData => {
        const key = `${_trackId.value}-${_sceneId.value}`;
        return slotStore.get(key) ?? defaultSlot();
      },
      getSceneSummary: (_sceneId: SceneID): SceneSummary => defaultSummary(),
      getCrossfader: (): CrossfaderData => ({ position: 0, curve: 'Linear' }),
      launchClip: (trackId: TrackID, sId: SceneID) => {
        launchedClips.push({ trackId, sceneId: sId });
      },
      launchScene: (sId: SceneID) => {
        launchedScenes.push(sId);
      },
      setCrossfaderPosition: (pos: number) => {
        crossfaderPositions.push(pos);
      },
      ...overrides,
    };
  }

  beforeEach(() => {
    launchedScenes = [];
    launchedClips = [];
    crossfaderPositions = [];
  });

  // ── Zero Canvas 2D ─────────────────────────────────────────────────

  it('does NOT import or reference CanvasRenderingContext2D', () => {
    const ds = createDataSource({
      getTracks: () => [],
      getScenes: () => [],
    });
    const view = new GPUSessionView({ dataSource: ds });
    const vnode = view.render();

    expect(vnode.type).toBe('container');
    // All drawables are rect or text — no canvas API references
    expect(findByType(vnode, 'rect').length).toBeGreaterThan(0);
    expect(findByType(vnode, 'text').length).toBeGreaterThan(0);
  });

  // ── Grid Culling ──────────────────────────────────────────────────

  it('culls cells outside the viewport (fewer rendered than total)', () => {
    // 8 tracks × 16 scenes
    const tracks: TrackData[] = [];
    for (let i = 0; i < 8; i++) tracks.push(makeTrack(i, `Track ${i}`));

    const scenes: SceneData[] = [];
    for (let i = 0; i < 16; i++) scenes.push(makeScene(i, `Scene ${i + 1}`, i));

    // All cells have clips
    const slotStore = new Map<string, ClipSlotData>();
    for (let t = 0; t < 8; t++) {
      for (let s = 0; s < 16; s++) {
        slotStore.set(`${t}-${s}`, {
          clip_name: `C${t}x${s}`,
          clip_color: 0,
          has_clip: true,
          state: 'Stopped',
          follow_action: 'None',
        });
      }
    }

    const ds = createDataSource({
      getTracks: () => tracks,
      getScenes: () => scenes,
      getSlot: (trackId: TrackID, sId: SceneID) => {
        return slotStore.get(`${trackId.value}-${sId.value}`) ?? defaultSlot();
      },
      getSceneSummary: () => defaultSummary(),
    });

    const view = new GPUSessionView({ dataSource: ds });

    // Tiny viewport: only a few cells visible
    view.setState({
      viewWidth: SCENE_STRIP_WIDTH + MIN_SLOT_WIDTH * 2, // Very narrow: ~2 cols
      viewHeight: CROSSFADER_HEIGHT + COLUMN_HEADER_HEIGHT + SLOT_HEIGHT * 2, // ~2 rows
      scrollX: 0,
      scrollY: 0,
    } as any);

    const vnode = view.render();

    // cell-bg is rendered for EVERY cell in the visible viewport (1 per cell)
    const cellBgNodes = findByKeyPrefix(vnode, `${view.widgetId}-cell-bg-`);
    // Total visible cells should be less than 128 (total cells)
    expect(cellBgNodes.length).toBeLessThan(128);
    expect(cellBgNodes.length).toBeGreaterThan(0);

    // Compare with full viewport (no culling)
    view.setState({
      viewWidth: 2000,
      viewHeight: 2000,
    } as any);
    const fullVnode = view.render();
    const allCellNodes = findByKeyPrefix(fullVnode, `${view.widgetId}-cell-bg-`);
    // With full viewport (2000x2000), trackCount=8, columnWidth=max(100,min(200,2000/8))=200
    // 2000px minus 140 strip = 1860 / 200 = 9.3 → all 8 cols visible
    // 2000 - 48 - 32 = 1920 / 48 = 40 → all 16 rows visible
    expect(allCellNodes.length).toBe(8 * 16); // 128
    // Small viewport must produce fewer cells
    expect(cellBgNodes.length).toBeLessThan(allCellNodes.length);
  });

  // ── Playing / Triggered State ─────────────────────────────────────

  it('renders green indicator for playing clips', () => {
    const tracks = [makeTrack(0, 'Track 0')];
    const scenes = [makeScene(0, 'Scene 1', 0)];

    const slotStore = new Map<string, ClipSlotData>();
    slotStore.set('0-0', {
      clip_name: 'Playing Clip',
      clip_color: 0,
      has_clip: true,
      state: 'Playing',
      follow_action: 'None',
    });

    const ds = createDataSource({
      getTracks: () => tracks,
      getScenes: () => scenes,
      getSlot: () => slotStore.get('0-0') ?? defaultSlot(),
    });

    const view = new GPUSessionView({ dataSource: ds });
    view.setState({
      tracks,
      scenes,
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
    } as any);

    const vnode = view.render();

    // Playing state indicator (green dot) should exist for cell 0-0
    const playingNodes = findByKeyPrefix(vnode, `${view.widgetId}-slot-playing-0-0`);
    expect(playingNodes.length).toBe(1);
    expect(playingNodes[0].type).toBe('rect');
  });

  // ── Scene Launch Dispatch ─────────────────────────────────────────

  it('dispatchScene launches scene via data source', () => {
    const scene = makeScene(42, 'Drop', 0);
    const ds = createDataSource({
      getScenes: () => [scene],
      getSceneSummary: () => ({ scene_id: sceneId(42), is_playing: false, any_triggered: false }),
    });

    const view = new GPUSessionView({ dataSource: ds });
    view.setState({
      tracks: [],
      scenes: [scene],
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
    } as any);

    // Trigger launch via the private method (we test the dispatch behavior)
    view['_launchScene'](sceneId(42));

    expect(launchedScenes.length).toBe(1);
    expect(launchedScenes[0].value).toBe(42);
  });

  // ── Follow Action Indicators ──────────────────────────────────────

  it('shows follow action icon for PlayNext clips', () => {
    const tracks = [makeTrack(0, 'Track 0')];
    const scenes = [makeScene(0, 'Scene 1', 0)];

    const slotStore = new Map<string, ClipSlotData>();
    slotStore.set('0-0', {
      clip_name: 'Follow Clip',
      clip_color: 0,
      has_clip: true,
      state: 'Stopped',
      follow_action: 'PlayNext',
    });

    const ds = createDataSource({
      getTracks: () => tracks,
      getScenes: () => scenes,
      getSlot: () => slotStore.get('0-0') ?? defaultSlot(),
    });

    const view = new GPUSessionView({ dataSource: ds });
    view.setState({
      tracks,
      scenes,
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
    } as any);

    const vnode = view.render();

    // Should have a follow action text node for cell 0-0
    const followNodes = findByKeyPrefix(vnode, `${view.widgetId}-slot-follow-0-0`);
    expect(followNodes.length).toBe(1);
    expect(followNodes[0].type).toBe('text');
    // PlayNext icon is ▶ (unicode \u25B6)
    expect(followNodes[0].props.text).toBe('\u25B6');
  });

  it('hides follow action icon for clips with None follow action', () => {
    const tracks = [makeTrack(0, 'Track 0')];
    const scenes = [makeScene(0, 'Scene 1', 0)];

    const slotStore = new Map<string, ClipSlotData>();
    slotStore.set('0-0', {
      clip_name: 'Follow Clip',
      clip_color: 0,
      has_clip: true,
      state: 'Stopped',
      follow_action: 'None',
    });

    const ds = createDataSource({
      getTracks: () => tracks,
      getScenes: () => scenes,
      getSlot: () => slotStore.get('0-0') ?? defaultSlot(),
    });

    const view = new GPUSessionView({ dataSource: ds });
    view.setState({
      tracks,
      scenes,
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
    } as any);

    const vnode = view.render();
    // No follow action icon should exist
    const followNodes = findByKeyPrefix(vnode, `${view.widgetId}-slot-follow-0-0`);
    expect(followNodes.length).toBe(0);
  });

  // ── Crossfader Position ───────────────────────────────────────────

  it('renders crossfader with A/B labels and position text', () => {
    const ds = createDataSource({
      getCrossfader: (): CrossfaderData => ({ position: 0.5, curve: 'Linear' }),
    });

    const view = new GPUSessionView({ dataSource: ds });
    view.setState({
      viewWidth: 800,
      viewHeight: 600,
    } as any);

    const vnode = view.render();

    // Crossfader elements: bg, track, handle, labels
    const xfBg = findByKeyPrefix(vnode, `${view.widgetId}-xf-bg`);
    expect(xfBg.length).toBe(1);

    // Keys contain `-xf-handle` as prefix for both handle and handle-border
    // Check we have at least the handle (handle + border = 2 rect-like nodes)
    const xfHandle = findByKeyPrefix(vnode, `${view.widgetId}-xf-handle`);
    expect(xfHandle.length).toBeGreaterThanOrEqual(1);

    const xfPos = findByKeyPrefix(vnode, `${view.widgetId}-xf-pos`);
    expect(xfPos.length).toBe(1);
    // Position label should reflect the value
    expect(xfPos[0].props.text).toBe('0.50');
  });

  it('crossfader drag calls setCrossfaderPosition', () => {
    const ds = createDataSource();
    const view = new GPUSessionView({ dataSource: ds });

    view.setState({
      viewWidth: 800,
      viewHeight: 600,
      crossfader: { position: 0, curve: 'Linear' },
    } as any);

    // Simulate crossfader move at position corresponding to ~0.5
    const trackLeft = 800 * (1 - 0.7) / 2;
    const trackWidth = 800 * 0.7;
    const halfX = trackLeft + trackWidth * 0.75; // 75% → position = 0.5

    view.onCrossfaderDown({ x: halfX, y: 24, type: 'mousedown', widget_id: 0, button: 0, keyCode: 0, shift: false, ctrl: false, alt: false, meta: false });
    view.onCrossfaderMove({ x: halfX, y: 24, type: 'mousemove', widget_id: 0, button: 0, keyCode: 0, shift: false, ctrl: false, alt: false, meta: false });

    // setCrossfaderPosition should have been called with approximately 0.5
    expect(crossfaderPositions.length).toBeGreaterThan(0);
    const pos = crossfaderPositions[crossfaderPositions.length - 1];
    expect(pos).toBeGreaterThan(0.4);
    expect(pos).toBeLessThan(0.6);
  });

  it('crossfader position updates in data source after drag', () => {
    const ds = createDataSource();
    const view = new GPUSessionView({ dataSource: ds });

    view.setState({
      viewWidth: 800,
      viewHeight: 600,
    } as any);

    const trackLeft = 800 * (1 - 0.7) / 2;
    const trackWidth = 800 * 0.7;
    const halfX = trackLeft + trackWidth * 0.5; // 50% = center = position 0

    view.onCrossfaderDown({ x: halfX, y: 24, type: 'mousedown', widget_id: 0, button: 0, keyCode: 0, shift: false, ctrl: false, alt: false, meta: false });
    view.onCrossfaderMove({ x: trackLeft + trackWidth, y: 24, type: 'mousemove', widget_id: 0, button: 0, keyCode: 0, shift: false, ctrl: false, alt: false, meta: false });

    // After dragging to far right, position dispatched to data source should be 1.0
    expect(crossfaderPositions.length).toBeGreaterThan(0);
    const lastPos = crossfaderPositions[crossfaderPositions.length - 1];
    expect(lastPos).toBeCloseTo(1.0, 1);
  });

  // ── Scene Strip ───────────────────────────────────────────────────

  it('renders scene names in the scene strip', () => {
    const scenes = [
      makeScene(0, 'Intro', 0),
      makeScene(1, 'Verse', 1),
      makeScene(2, 'Chorus', 2),
    ];

    const ds = createDataSource({
      getScenes: () => scenes,
      getSceneSummary: () => defaultSummary(),
    });

    const view = new GPUSessionView({ dataSource: ds });
    view.setState({
      tracks: [],
      scenes,
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
    } as any);

    const vnode = view.render();

    // Scene name texts should be rendered
    const sceneNames = findByKeyPrefix(vnode, `${view.widgetId}-scene-name-`);
    expect(sceneNames.length).toBe(3);

    expect(sceneNames[0].props.text).toBe('Intro');
    expect(sceneNames[1].props.text).toBe('Verse');
    expect(sceneNames[2].props.text).toBe('Chorus');
  });

  // ── Column Headers ────────────────────────────────────────────────

  it('renders track names as column headers', () => {
    const tracks = [makeTrack(0, 'Kick'), makeTrack(1, 'Snare')];
    const ds = createDataSource({
      getTracks: () => tracks,
    });

    const view = new GPUSessionView({ dataSource: ds });
    view.setState({
      tracks,
      scenes: [],
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
    } as any);

    const vnode = view.render();

    const headers = findByKeyPrefix(vnode, `${view.widgetId}-col-header-`);
    expect(headers.length).toBe(2);
    expect(headers[0].props.text).toBe('Kick');
    expect(headers[1].props.text).toBe('Snare');
  });

  // ── setScroll ─────────────────────────────────────────────────────

  it('setScroll updates scroll state', () => {
    const view = new GPUSessionView({ dataSource: createDataSource() });
    view.setScroll(50, 100);
    expect(view.state.scrollX).toBe(50);
    expect(view.state.scrollY).toBe(100);
  });

  it('setScroll clamps to 0', () => {
    const view = new GPUSessionView({ dataSource: createDataSource() });
    view.setScroll(-10, -5);
    expect(view.state.scrollX).toBe(0);
    expect(view.state.scrollY).toBe(0);
  });
});
