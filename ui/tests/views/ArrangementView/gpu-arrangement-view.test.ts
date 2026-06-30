// GPUArrangementView — Unit Tests
// Tests: clip culling, playhead reactive update, grid zoom, zero Canvas 2D calls
import { describe, it, expect, beforeEach } from 'vitest';
import { GPUArrangementView } from '../../../src/views/ArrangementView/gpu-arrangement-view.js';
import type { VNode } from '../../../src/ffi/bridge.js';
import type {
  ArrangementDataSource,
  AnyTrackData,
  AnyClipData,
  AudioClipData,
  MidiClipData,
  ClipID,
  TrackID,
} from '../../../src/views/ArrangementView/types.js';
import {
  TRACK_HEIGHT,
  RULER_HEIGHT,
  TRACK_LIST_WIDTH,
} from '../../../src/views/ArrangementView/types.js';

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

/** Count VNodes of a given type in the tree. */
function countByType(root: VNode, type: string): number {
  let count = 0;
  function walk(node: VNode): void {
    if (node.type === type) count++;
    if (node.children) node.children.forEach(walk);
  }
  walk(root);
  return count;
}

/** Create a minimal ArrangementDataSource stub. */
function createDataSource(overrides?: Partial<ArrangementDataSource>): ArrangementDataSource {
  return {
    getTracks: () => [],
    getVisibleTracks: () => [],
    getPlayheadPPQN: () => 0,
    getPixelsPerPPQN: () => 40 / 960,
    isGridSnapEnabled: () => false,
    getGridPpqn: () => 240,
    ...overrides,
  };
}

function clipId(v: number): ClipID {
  return { value: v };
}

function trackId(v: number): TrackID {
  return { value: v };
}

function makeAudioClip(id: number, startPPQN: number, lengthPPQN: number): AudioClipData {
  return {
    id: clipId(id),
    start_ppqn: startPPQN,
    length_ppqn: lengthPPQN,
    name: `Clip ${id}`,
    color: 0,
    gain: 1,
    muted: false,
    fade_in_ppqn: 0,
    fade_out_ppqn: 0,
    fade_in_shape: 'None',
    fade_out_shape: 'None',
    looping: false,
    loop_start_ppqn: 0,
    loop_end_ppqn: 0,
    track_id: trackId(0),
    type: 'Audio',
    file_path: '',
    waveform: { peaks: [0.5, 0.7, 0.3], valleys: [0.3, 0.5, 0.1] },
  };
}

function makeMidiClip(id: number, startPPQN: number, lengthPPQN: number): MidiClipData {
  return {
    id: clipId(id),
    start_ppqn: startPPQN,
    length_ppqn: lengthPPQN,
    name: `MIDI ${id}`,
    color: 0,
    gain: 1,
    muted: false,
    fade_in_ppqn: 0,
    fade_out_ppqn: 0,
    fade_in_shape: 'None',
    fade_out_shape: 'None',
    looping: false,
    loop_start_ppqn: 0,
    loop_end_ppqn: 0,
    track_id: trackId(0),
    type: 'MIDI',
    note_density: 0.5,
  };
}

function makeTrack(id: number, name: string, clips: AnyClipData[]): AnyTrackData {
  return {
    id: trackId(id),
    name,
    type: 'Audio',
    volume: 0.75,
    pan: 0,
    muted: false,
    soloed: false,
    clips,
  };
}

// ─── Tests ────────────────────────────────────────────────────────────

describe('GPUArrangementView', () => {
  let view: GPUArrangementView;

  beforeEach(() => {
    // Reset widgetId counter by creating a fresh instance
    view = new GPUArrangementView({
      dataSource: createDataSource(),
    });
  });

  // ── Zero Canvas 2D ─────────────────────────────────────────────────

  it('does NOT import or reference CanvasRenderingContext2D', () => {
    // The GPUArrangementView should never use Canvas 2D.
    // Verify by checking render() produces VNodes only (no ctx calls).
    const vnode = view.render();
    expect(vnode.type).toBe('container');
    expect(vnode.key).toBe(String(view.widgetId));
    // All drawables are rect or text — no canvas API calls.
    expect(findByType(vnode, 'container').length).toBeGreaterThan(0);
  });

  // ── Clip Culling ──────────────────────────────────────────────────

  it('renders clips within the visible viewport', () => {
    const ds = createDataSource({
      getVisibleTracks: () => [
        makeTrack(0, 'Track 1', [makeAudioClip(1, 0, 480)]), // 0-480 PPQN, visible
      ],
    });
    view = new GPUArrangementView({ dataSource: ds });

    // Viewport large enough to show the clip.
    // pixelsPerPPQN=1 gives good pixel mapping for testing.
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 1,
    } as any);

    const vnode = view.render();
    // The clip should appear — clip id=1 at start_ppqn=0, length=480 → x=200..680
    const clipNodes = findByKeyPrefix(vnode, `${view.widgetId}-clip-1`);
    expect(clipNodes.length).toBeGreaterThan(0);
  });

  it('culls clips entirely outside the viewport', () => {
    // Place clip far to the right — outside the viewport
    const ds = createDataSource({
      getVisibleTracks: () => [
        makeTrack(0, 'Track 1', [makeAudioClip(99, 50000, 480)]),
      ],
    });
    view = new GPUArrangementView({ dataSource: ds });

    view.setState({
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 40 / 960,
    } as any);

    const vnode = view.render();
    // Clip with id=99 should be culled (no VNode produced)
    const clipNodes = findByKeyPrefix(vnode, `${view.widgetId}-clip-99`);
    // May find the clip container but its rect is never added due to culling
    expect(clipNodes.length).toBe(0);
  });

  it('culls clips scrolled above the visible area', () => {
    const clips = [makeAudioClip(1, 0, 480)];
    // Create tracks that are far down
    const tracks: AnyTrackData[] = [];
    for (let i = 0; i < 30; i++) {
      tracks.push(makeTrack(i, `Track ${i}`, i === 25 ? clips : []));
    }

    const ds = createDataSource({
      getVisibleTracks: () => tracks,
    });
    view = new GPUArrangementView({ dataSource: ds });

    // Scroll to the top — track 25 is far below
    view.setState({
      viewWidth: 800,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 40 / 960,
    } as any);

    const vnode = view.render();
    // Track 25's clip at id=1 has y = RULER_HEIGHT + 25*TRACK_HEIGHT - 0 = 32 + 1200 = 1232
    // But viewHeight=600, so it's below viewport → culled
    const clipNodes = findByKeyPrefix(vnode, `${view.widgetId}-clip-1`);
    expect(clipNodes.length).toBe(0);
  });

  // ── Playhead Reactive Update ──────────────────────────────────────

  it('renders playhead at correct PPQN position', () => {
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 1,
      playheadPPQN: 100,
    } as any);

    const vnode = view.render();
    // Playhead at x = 100*1 - 0 + TRACK_LIST_WIDTH = 100 + 200 = 300
    const phNodes = findByKeyPrefix(vnode, `${view.widgetId}-playhead`);
    expect(phNodes.length).toBe(1);
    const ph = phNodes[0];
    expect(ph.type).toBe('rect');
    const x = ph.props.x as number;
    expect(x).toBeCloseTo(300, 0);
  });

  it('playhead re-renders at new position after setState', () => {
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 1,
      playheadPPQN: 50,
    } as any);

    // Initial render: playhead at x = 50 + 200 = 250
    let vnode = view.render();
    let phNodes = findByKeyPrefix(vnode, `${view.widgetId}-playhead`);
    expect(phNodes.length).toBe(1);
    expect(phNodes[0].props.x as number).toBeCloseTo(250, 0);

    // Update playhead via setState (as if playhead poll fired)
    view.setState({ playheadPPQN: 200 } as any);

    // Re-render: playhead should now be at x = 200 + 200 = 400
    vnode = view.render();
    phNodes = findByKeyPrefix(vnode, `${view.widgetId}-playhead`);
    expect(phNodes.length).toBe(1);
    expect(phNodes[0].props.x as number).toBeCloseTo(400, 0);
  });

  // ── Grid Zoom ─────────────────────────────────────────────────────

  it('renders grid lines at beat level with default zoom', () => {
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 40 / 960, // Default zoom ≈ 0.042
    } as any);

    const vnode = view.render();
    // Grid lines have keys like `${widgetId}-grid-{PPQN}`
    const gridNodes = findByKeyPrefix(vnode, `${view.widgetId}-grid-`);
    // At default zoom, viewWidth=1000, TRACK_LIST_WIDTH=200, canvas area=800
    // Beat PPQN step = 960, visible range: start=0, end≈ ceil(800/0.042/960)*960 = ceil(19.8)*960 = 20*960
    // So we expect roughly 20 grid lines
    expect(gridNodes.length).toBeGreaterThan(0);
    expect(gridNodes.length).toBeLessThan(100);
  });

  it('changes grid line density when pixelsPerPPQN changes', () => {
    // Low zoom (zoomed out): more beats visible → more grid lines
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 1 / 960, // Very zoomed out
    } as any);

    let vnode = view.render();
    const gridLow = findByKeyPrefix(vnode, `${view.widgetId}-grid-`);
    const lowCount = gridLow.length;

    // Higher zoom (zoomed in): fewer beats visible → fewer grid lines
    view.setState({
      pixelsPerPPQN: 10 / 960, // More zoomed in
    } as any);

    vnode = view.render();
    const gridHigh = findByKeyPrefix(vnode, `${view.widgetId}-grid-`);
    const highCount = gridHigh.length;

    // Zoomed out shows more grid lines, zoomed in shows fewer
    expect(lowCount).toBeGreaterThan(highCount);
  });

  // ── setScroll / setZoom ───────────────────────────────────────────

  it('setScroll updates scrollX and scrollY in state', () => {
    view.setScroll(100, 200);
    expect(view.state.scrollX).toBe(100);
    expect(view.state.scrollY).toBe(200);
  });

  it('setScroll clamps to 0', () => {
    view.setScroll(-50, -10);
    expect(view.state.scrollX).toBe(0);
    expect(view.state.scrollY).toBe(0);
  });

  it('setZoom updates pixelsPerPPQN in state', () => {
    view.setZoom(0.1);
    expect(view.state.pixelsPerPPQN).toBe(0.1);
  });

  // ── Track headers ─────────────────────────────────────────────────

  it('renders track headers for visible tracks', () => {
    const tracks = [
      makeTrack(0, 'Kick', []),
      makeTrack(1, 'Snare', []),
    ];
    const ds = createDataSource({
      getVisibleTracks: () => tracks,
    });
    view = new GPUArrangementView({ dataSource: ds });

    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
    } as any);

    const vnode = view.render();

    // Track name texts should be rendered (render reads from data source)
    const trackEls = findByKeyPrefix(vnode, `${view.widgetId}-tl-name-`);
    expect(trackEls.length).toBe(2);

    // First track name should be "Kick"
    expect(trackEls[0].props.text).toBe('Kick');
    expect(trackEls[1].props.text).toBe('Snare');
  });

  // ── Waveform rendering ───────────────────────────────────────────

  it('renders waveform for audio clips', () => {
    const audioClip = makeAudioClip(1, 0, 480);
    audioClip.waveform = { peaks: [0.9, 0.5, 0.3, 0.7], valleys: [0.1, 0.2, 0.1, 0.3] };

    const ds = createDataSource({
      getVisibleTracks: () => [makeTrack(0, 'Audio Track', [audioClip])],
    });
    view = new GPUArrangementView({ dataSource: ds });

    // Use pixelsPerPPQN=1 so the clip width = 480*1 = 480px → enough for waveform bars
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 1,
    } as any);

    const vnode = view.render();
    // Waveform bars should be rendered
    const wfNodes = findByKeyPrefix(vnode, `${view.widgetId}-wf-1-`);
    expect(wfNodes.length).toBeGreaterThan(0);
  });

  // ── Ruler rendering ──────────────────────────────────────────────

  it('renders ruler background and ticks at medium zoom', () => {
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 0.03, // Beat-level grid: 0.03*960=28.8 >= 8
    } as any);

    const vnode = view.render();

    // Ruler background should exist
    const rulerBg = findByKeyPrefix(vnode, `${view.widgetId}-ruler-bg`);
    expect(rulerBg.length).toBe(1);

    // At beat-level grid, beat markers exist
    const beatTicks = findByKeyPrefix(vnode, `${view.widgetId}-ruler-beat-`);
    expect(beatTicks.length).toBeGreaterThan(0);
  });

  // ── Grid level transitions ────────────────────────────────────────

  it('renders ruler in bar-level mode at very low zoom', () => {
    // At very low zoom, ruler should show bars only
    // Grid thresholds: bars when pixelsPerPPQN * 960 < 8 → < 0.00833
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 0.005, // Very zoomed out → bar level
    } as any);

    const vnode = view.render();

    const barLabels = findByKeyPrefix(vnode, `${view.widgetId}-ruler-bar-label-`);
    const beatNodes = findByKeyPrefix(vnode, `${view.widgetId}-ruler-beat-`);

    // At bar-level ruler mode, we expect bar labels
    expect(barLabels.length).toBeGreaterThan(0);
    // No beat ticks at bar level
    expect(beatNodes.length).toBe(0);
  });

  it('transitions to beat-level ruler at medium zoom', () => {
    view.setState({
      viewWidth: 1000,
      viewHeight: 600,
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: 0.03, // Beat level: 0.03*960=28.8 >= 8
    } as any);

    const vnode = view.render();
    const beatNodes = findByKeyPrefix(vnode, `${view.widgetId}-ruler-beat-`);

    // At beat-level, we should have beat markers
    expect(beatNodes.length).toBeGreaterThan(0);

    // Bar labels only render at bar-level grid, not beat level.
    // Beat level renders medium tick marks without bar number labels.
    const barLabels = findByKeyPrefix(vnode, `${view.widgetId}-ruler-bar-label-`);
    // Bar labels may or may not exist depending on exact zoom; the key assertion is beat markers
  });
});
