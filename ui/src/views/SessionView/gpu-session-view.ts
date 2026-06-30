// ── GPUSessionView — GPU-Backed Session View ───────────────────────────
//
// Port of the Canvas 2D SessionView to the GPU Component tree.
// Renders clip slot grid (tracks × scenes), scene launch buttons,
// crossfader, and follow action indicators entirely via VNode tree
// → Bridge.serialize. Zero Canvas 2D calls.
//
// Design:
//   - Extends Component; viewport state (scroll) + crossfader state live
//     in component state
//   - Virtual culling: only cells in the visible viewport produce VNodes
//   - Scene launch buttons dispatch to dataSource.launchScene()
//   - Crossfader drag dispatches to dataSource.setCrossfaderPosition()
//   - Follow action indicators shown as icon text (▶, ↻, ■, etc.)
//   - Playing cells show green indicator; triggered cells show amber
//
// State transitions:
//   setState({ scrollY }) on wheel → dirty-mark → re-render at new position
//   crossfader drag → setState({ crossfader }) → dirty-mark → thumb moves

import { Component } from '../../framework/component.js';
import type { VNode } from '../../ffi/bridge.js';
import type { InputEvent } from '../../ffi/types.js';
import {
  SLOT_HEIGHT,
  COLUMN_HEADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  CROSSFADER_HEIGHT,
  MIN_SLOT_WIDTH,
  MAX_SLOT_WIDTH,
  SessionDataSource,
  SceneData,
  CrossfaderData,
  SceneID,
} from './types.js';

import type { TrackData } from '../ArrangementView/types.js';

// ── Theme (mirrors Canvas 2D colors in 0..1 float) ─────────────────────

const BG = { r: 0.10, g: 0.10, b: 0.18, a: 1 };               // #1a1a2e
const ALT_ROW = { r: 0.12, g: 0.12, b: 0.20, a: 1 };          // #1e1e34
const CELL_BORDER = { r: 0.16, g: 0.16, b: 0.25, a: 1 };      // #2a2a3f
const CLIP_FILL = { r: 0.16, g: 0.33, b: 0.25, a: 1 };        // #2a5540
const CLIP_PLAYING = { r: 0.23, g: 0.60, b: 0.33, a: 1 };     // #3a9955
const CLIP_TRIGGERED = { r: 0.60, g: 0.48, b: 0.20, a: 1 };   // #997a33
const CLIP_TEXT = { r: 0.75, g: 0.82, b: 0.88, a: 1 };        // #c0d0e0
const CLIP_SHADOW = { r: 0, g: 0, b: 0, a: 0.4 };
const HEADER_BG = { r: 0.086, g: 0.086, b: 0.169, a: 1 };     // #16162b
const HEADER_TEXT = { r: 0.53, g: 0.53, b: 0.67, a: 1 };      // #8888aa
const HEADER_BORDER = { r: 0.20, g: 0.20, b: 0.35, a: 1 };    // #32325a
const SCENE_INDICATOR_GREEN = { r: 0.27, g: 0.80, b: 0.40, a: 1 }; // #44cc66
const SCENE_INDICATOR_AMBER = { r: 0.80, g: 0.67, b: 0.20, a: 1 }; // #ccaa33
const SCENE_NAME = { r: 0.88, g: 0.88, b: 0.94, a: 1 };       // #e0e0f0
const LAUNCH_BTN_BG = { r: 0.16, g: 0.16, b: 0.31, a: 1 };    // #2a2a50
const ADD_SCENE_COLOR = { r: 0.29, g: 0.42, b: 0.54, a: 1 };  // #4a6a8a

// Crossfader theme
const XF_BG = { r: 0.086, g: 0.086, b: 0.169, a: 1 };
const XF_TRACK = { r: 0.12, g: 0.12, b: 0.22, a: 1 };
const XF_FILL_A = { r: 0.23, g: 0.42, b: 0.33, a: 1 };
const XF_FILL_B = { r: 0.23, g: 0.33, b: 0.42, a: 1 };
const XF_CENTER = { r: 0.16, g: 0.16, b: 0.29, a: 1 };
const XF_HANDLE = { r: 0.53, g: 0.67, b: 0.80, a: 1 };
const XF_HANDLE_BORDER = { r: 0.67, g: 0.80, b: 0.93, a: 1 };
const XF_HANDLE_SHADOW = { r: 0, g: 0, b: 0, a: 0.3 };
const XF_LABEL = { r: 0.53, g: 0.53, b: 0.67, a: 1 };
const XF_LABEL_A = { r: 0.33, g: 0.67, b: 0.47, a: 1 };
const XF_LABEL_B = { r: 0.33, g: 0.47, b: 0.67, a: 1 };

// Follow action icon map
const FOLLOW_ICON: Record<string, string> = {
  Stop: '\u25A0',          // ■
  PlayNext: '\u25B6',      // ▶
  PlayRandom: '\u21BB',    // ↻
  ContinueAsLoop: '\u21BA', // ↺
};

// ─── Layout constants ──────────────────────────────────────────────────

const HANDLE_RADIUS = 10;
const TRACK_H = 6;
const TRACK_WIDTH_PCT = 0.7;
const LABEL_PADDING = 8;

// ── Props & State ────────────────────────────────────────────────────

export interface GPUSessionViewProps {
  /** Data source for tracks, scenes, slots, crossfader. */
  dataSource: SessionDataSource;
}

export interface GPUSessionViewState {
  scrollX: number;
  scrollY: number;
  viewWidth: number;
  viewHeight: number;
}

// ─── GPUSessionView ──────────────────────────────────────────────────

export class GPUSessionView extends Component<GPUSessionViewProps, GPUSessionViewState> {
  /** Whether the crossfader thumb is being dragged. */
  private _crossfaderDragging = false;

  constructor(props: GPUSessionViewProps) {
    super(props);
    this.state = {
      scrollX: 0,
      scrollY: 0,
      viewWidth: 800,
      viewHeight: 600,
    };
  }

  // ── Data read ──────────────────────────────────────────────────────

  /** Get current tracks from the data source. */
  private _getTracks(): TrackData[] {
    return this.props.dataSource.getTracks();
  }

  /** Get current scenes from the data source. */
  private _getScenes(): SceneData[] {
    return this.props.dataSource.getScenes();
  }

  /** Get current crossfader data from the data source. */
  private _getCrossfader(): CrossfaderData {
    return this.props.dataSource.getCrossfader();
  }

  // ── Viewport mutations ─────────────────────────────────────────────

  /** Set scroll position. */
  setScroll(scrollX: number, scrollY: number): void {
    this.setState({
      scrollX: Math.max(0, scrollX),
      scrollY: Math.max(0, scrollY),
    } as Partial<GPUSessionViewState>);
  }

  /** Handle wheel event for scroll. */
  onWheel(event: InputEvent): void {
    const newScrollY = Math.max(0, this.state.scrollY - event.y * 0.5);
    this.setState({ scrollY: newScrollY } as Partial<GPUSessionViewState>);
  }

  // ── Crossfader interaction ─────────────────────────────────────────

  /** Handle mouse down on crossfader track or thumb. */
  onCrossfaderDown(event: InputEvent): void {
    const { viewWidth } = this.state;
    const trackLeft = viewWidth * (1 - TRACK_WIDTH_PCT) / 2;
    const trackWidth = viewWidth * TRACK_WIDTH_PCT;
    const trackCenterY = CROSSFADER_HEIGHT / 2;

    // Check thumb hit
    const handleX = this._crossfaderHandleX(trackLeft, trackWidth);
    const dx = event.x - handleX;
    const dy = event.y - trackCenterY;
    const hitRadius = HANDLE_RADIUS + 6;

    if (dx * dx + dy * dy <= hitRadius * hitRadius) {
      this._crossfaderDragging = true;
      return;
    }

    // Check track hit
    if (event.y >= 0 && event.y <= CROSSFADER_HEIGHT &&
        event.x >= trackLeft && event.x <= trackLeft + trackWidth) {
      this._crossfaderDragging = true;
    }
  }

  /** Handle mouse move for crossfader drag. */
  onCrossfaderMove(event: InputEvent): void {
    if (!this._crossfaderDragging) return;

    const { viewWidth } = this.state;
    const trackLeft = viewWidth * (1 - TRACK_WIDTH_PCT) / 2;
    const trackWidth = viewWidth * TRACK_WIDTH_PCT;

    const t = (event.x - trackLeft) / trackWidth;
    const clamped = Math.max(0, Math.min(1, t));
    const position = clamped * 2 - 1; // [0,1] → [-1,+1]

    // Dispatch to data source
    this.props.dataSource.setCrossfaderPosition(position);

    // Re-render via dummy setState to show new crossfader position
    this.setState({} as Partial<GPUSessionViewState>);
  }

  /** Handle mouse up to end crossfader drag. */
  onCrossfaderUp(): void {
    this._crossfaderDragging = false;
  }

  // ── Scene / Clip launch ────────────────────────────────────────────

  /** Handle scene launch button click. */
  private _launchScene(sceneId: SceneID): void {
    this.props.dataSource.launchScene(sceneId);
  }

  /** Handle clip slot click. */
  private _launchSlot(trackIndex: number, sceneId: number): void {
    const tracks = this._getTracks();
    const track = tracks[trackIndex];
    if (track) {
      this.props.dataSource.launchClip(track.id, { value: sceneId });
    }
  }

  // ── Helpers ─────────────────────────────────────────────────────────

  private _computeColumnWidth(trackCount: number): number {
    if (trackCount <= 0) return MIN_SLOT_WIDTH;
    const availableWidth = this.state.viewWidth - SCENE_STRIP_WIDTH;
    const autoWidth = Math.floor(availableWidth / trackCount);
    return Math.max(MIN_SLOT_WIDTH, Math.min(MAX_SLOT_WIDTH, autoWidth));
  }

  private _crossfaderHandleX(trackLeft: number, trackWidth: number): number {
    const position = this._getCrossfader().position;
    const t = (position + 1) / 2;
    return trackLeft + t * trackWidth;
  }

  // ── Rendering ──────────────────────────────────────────────────────

  render(): VNode {
    const tracks = this._getTracks();
    const scenes = this._getScenes();
    const crossfader = this._getCrossfader();
    const { scrollX, scrollY, viewWidth, viewHeight } = this.state;
    const children: VNode[] = [];
    const columnWidth = this._computeColumnWidth(tracks.length);

    // ── 1. Crossfader Widget (top bar) ──
    const xfChildren = this._renderCrossfader(viewWidth, crossfader);
    for (let i = 0; i < xfChildren.length; i++) {
      children.push(xfChildren[i]);
    }

    // ── 2. Scene Strip (left column) ──
    const stripChildren = this._renderSceneStrip(scenes, scrollY, viewHeight);
    for (let i = 0; i < stripChildren.length; i++) {
      children.push(stripChildren[i]);
    }

    // ── 3. Clip Grid (main area) ──
    const gridChildren = this._renderClipGrid(tracks, scenes, scrollX, scrollY, viewWidth, viewHeight, columnWidth);
    for (let i = 0; i < gridChildren.length; i++) {
      children.push(gridChildren[i]);
    }

    // ── 4. Scene strip right border ──
    children.push({
      type: 'rect',
      key: `${this.widgetId}-strip-border`,
      props: {
        x: SCENE_STRIP_WIDTH - 1, y: CROSSFADER_HEIGHT, w: 1,
        h: viewHeight - CROSSFADER_HEIGHT,
        fillR: CELL_BORDER.r, fillG: CELL_BORDER.g, fillB: CELL_BORDER.b, fillA: CELL_BORDER.a,
      },
    });

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x: 0, y: 0, w: viewWidth, h: viewHeight },
      children,
      eventHandlers: {
        mousemove: (e: InputEvent) => this.onCrossfaderMove(e),
        mouseup: () => this.onCrossfaderUp(),
        mouseleave: () => this.onCrossfaderUp(),
      },
    };
  }

  // ── Crossfader ─────────────────────────────────────────────────────

  private _renderCrossfader(viewWidth: number, crossfader: CrossfaderData): VNode[] {
    const nodes: VNode[] = [];
    const widgetY = 0;
    const trackLeft = viewWidth * (1 - TRACK_WIDTH_PCT) / 2;
    const trackWidth = viewWidth * TRACK_WIDTH_PCT;
    const trackCenterY = CROSSFADER_HEIGHT / 2;
    const trackTop = trackCenterY - TRACK_H / 2;

    // Background
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-xf-bg`,
      props: {
        x: 0, y: widgetY, w: viewWidth, h: CROSSFADER_HEIGHT,
        fillR: XF_BG.r, fillG: XF_BG.g, fillB: XF_BG.b, fillA: XF_BG.a,
      },
    });

    // Bottom border
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-xf-border`,
      props: {
        x: 0, y: CROSSFADER_HEIGHT - 1, w: viewWidth, h: 1,
        fillR: CELL_BORDER.r, fillG: CELL_BORDER.g, fillB: CELL_BORDER.b, fillA: CELL_BORDER.a,
      },
    });

    // Track background
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-xf-track`,
      props: {
        x: trackLeft, y: trackTop, w: trackWidth, h: TRACK_H,
        fillR: XF_TRACK.r, fillG: XF_TRACK.g, fillB: XF_TRACK.b, fillA: XF_TRACK.a,
        cornerRadius: 3,
      },
    });

    // Handle X position
    const handleX = this._crossfaderHandleX(trackLeft, trackWidth);
    const rightEnd = trackLeft + trackWidth;

    // A side fill (left of handle)
    if (handleX > trackLeft) {
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-xf-fill-a`,
        props: {
          x: trackLeft, y: trackTop,
          w: handleX - trackLeft, h: TRACK_H,
          fillR: XF_FILL_A.r, fillG: XF_FILL_A.g, fillB: XF_FILL_A.b, fillA: XF_FILL_A.a,
        },
      });
    }

    // B side fill (right of handle)
    if (handleX < rightEnd) {
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-xf-fill-b`,
        props: {
          x: handleX, y: trackTop,
          w: rightEnd - handleX, h: TRACK_H,
          fillR: XF_FILL_B.r, fillG: XF_FILL_B.g, fillB: XF_FILL_B.b, fillA: XF_FILL_B.a,
        },
      });
    }

    // Center marker
    const centerX = trackLeft + trackWidth / 2;
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-xf-center`,
      props: {
        x: centerX, y: trackTop + 1, w: 1, h: TRACK_H - 2,
        fillR: XF_CENTER.r, fillG: XF_CENTER.g, fillB: XF_CENTER.b, fillA: XF_CENTER.a,
      },
    });

    // Handle shadow
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-xf-shadow`,
      props: {
        x: handleX - HANDLE_RADIUS + 1, y: trackCenterY - HANDLE_RADIUS + 1,
        w: HANDLE_RADIUS * 2, h: HANDLE_RADIUS * 2,
        fillR: XF_HANDLE_SHADOW.r, fillG: XF_HANDLE_SHADOW.g,
        fillB: XF_HANDLE_SHADOW.b, fillA: XF_HANDLE_SHADOW.a,
        cornerRadius: HANDLE_RADIUS,
      },
    });

    // Handle
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-xf-handle`,
      props: {
        x: handleX - HANDLE_RADIUS, y: trackCenterY - HANDLE_RADIUS,
        w: HANDLE_RADIUS * 2, h: HANDLE_RADIUS * 2,
        fillR: XF_HANDLE.r, fillG: XF_HANDLE.g, fillB: XF_HANDLE.b, fillA: XF_HANDLE.a,
        cornerRadius: HANDLE_RADIUS,
      },
      eventHandlers: {
        mousedown: (e: InputEvent) => this.onCrossfaderDown(e),
      },
    });

    // Handle border
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-xf-handle-border`,
      props: {
        x: handleX - HANDLE_RADIUS, y: trackCenterY - HANDLE_RADIUS,
        w: HANDLE_RADIUS * 2, h: HANDLE_RADIUS * 2,
        fillR: XF_HANDLE_BORDER.r, fillG: XF_HANDLE_BORDER.g,
        fillB: XF_HANDLE_BORDER.b, fillA: XF_HANDLE_BORDER.a,
        cornerRadius: HANDLE_RADIUS,
        strokeWidth: 1.5,
      },
    });

    // Labels
    nodes.push({ type: 'text', key: `${this.widgetId}-xf-label-a`, props: { text: 'A', x: LABEL_PADDING, y: trackCenterY + 4, r: XF_LABEL_A.r, g: XF_LABEL_A.g, b: XF_LABEL_A.b, a: 1, fontSize: 10 } });
    nodes.push({ type: 'text', key: `${this.widgetId}-xf-label-b`, props: { text: 'B', x: viewWidth - LABEL_PADDING, y: trackCenterY + 4, r: XF_LABEL_B.r, g: XF_LABEL_B.g, b: XF_LABEL_B.b, a: 1, fontSize: 10 } });

    // Position value
    const posLabel = crossfader.position.toFixed(2);
    nodes.push({ type: 'text', key: `${this.widgetId}-xf-pos`, props: { text: posLabel, x: viewWidth / 2, y: CROSSFADER_HEIGHT - 10, r: XF_LABEL.r, g: XF_LABEL.g, b: XF_LABEL.b, a: 1, fontSize: 10 } });

    // Curve label
    nodes.push({ type: 'text', key: `${this.widgetId}-xf-curve`, props: { text: crossfader.curve, x: viewWidth - LABEL_PADDING, y: 10, r: XF_LABEL.r, g: XF_LABEL.g, b: XF_LABEL.b, a: 1, fontSize: 9 } });

    return nodes;
  }

  // ── Scene Strip ────────────────────────────────────────────────────

  private _renderSceneStrip(scenes: SceneData[], scrollY: number, viewHeight: number): VNode[] {
    const nodes: VNode[] = [];
    const stripLeft = 0;
    const firstVisible = Math.max(0, Math.floor(scrollY / SLOT_HEIGHT));
    const lastVisible = Math.min(
      scenes.length,
      Math.ceil((scrollY + viewHeight - CROSSFADER_HEIGHT) / SLOT_HEIGHT),
    );

    // Background
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-strip-bg`,
      props: {
        x: stripLeft, y: CROSSFADER_HEIGHT, w: SCENE_STRIP_WIDTH,
        h: viewHeight - CROSSFADER_HEIGHT,
        fillR: BG.r, fillG: BG.g, fillB: BG.b, fillA: BG.a,
      },
    });

    for (let i = firstVisible; i < lastVisible; i++) {
      const scene = scenes[i];
      if (!scene) continue;

      const y = CROSSFADER_HEIGHT + i * SLOT_HEIGHT - scrollY;

      // Alternating row background
      if (i % 2 === 0) {
        // Use default background (already drawn)
      } else {
        nodes.push({
          type: 'rect',
          key: `${this.widgetId}-strip-row-${i}`,
          props: {
            x: 0, y, w: SCENE_STRIP_WIDTH, h: SLOT_HEIGHT,
            fillR: ALT_ROW.r, fillG: ALT_ROW.g, fillB: ALT_ROW.b, fillA: ALT_ROW.a,
          },
        });
      }

      // Bottom border
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-strip-border-${i}`,
        props: {
          x: 0, y: y + SLOT_HEIGHT - 1, w: SCENE_STRIP_WIDTH, h: 1,
          fillR: CELL_BORDER.r, fillG: CELL_BORDER.g, fillB: CELL_BORDER.b, fillA: CELL_BORDER.a,
        },
      });

      // Scene playing state from data source
      const summary = this.props.dataSource.getSceneSummary(scene.id);

      // Launch button (circle)
      const btnCx = 16;
      const btnCy = y + SLOT_HEIGHT / 2;
      const btnR = 7;

      let dotColor = LAUNCH_BTN_BG;
      if (summary.is_playing) {
        dotColor = SCENE_INDICATOR_GREEN;
      } else if (summary.any_triggered) {
        dotColor = SCENE_INDICATOR_AMBER;
      }

      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-launch-btn-${i}`,
        props: {
          x: btnCx - btnR, y: btnCy - btnR, w: btnR * 2, h: btnR * 2,
          fillR: dotColor.r, fillG: dotColor.g, fillB: dotColor.b, fillA: dotColor.a,
          cornerRadius: btnR,
        },
        eventHandlers: {
          click: () => this._launchScene(scene.id),
        },
      });

      // Launch button outer ring
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-launch-ring-${i}`,
        props: {
          x: btnCx - btnR, y: btnCy - btnR, w: btnR * 2, h: btnR * 2,
          fillR: 0, fillG: 0, fillB: 0, fillA: 0,
          sr: 0.29, sg: 0.29, sb: 0.48, sa: 1,
          strokeWidth: 1,
          cornerRadius: btnR,
        },
      });

      // Scene name
      const maxChars = Math.max(1, Math.floor((SCENE_STRIP_WIDTH - 32 - 10) / 8));
      const displayName = scene.name.length > 0
        ? scene.name.slice(0, maxChars)
        : `Scene ${scene.order_index + 1}`;

      nodes.push({
        type: 'text',
        key: `${this.widgetId}-scene-name-${i}`,
        props: {
          text: displayName,
          x: 32, y: y + SLOT_HEIGHT / 2 + 4,
          r: SCENE_NAME.r, g: SCENE_NAME.g, b: SCENE_NAME.b, a: SCENE_NAME.a,
          fontSize: 13,
        },
      });
    }

    // ── Add Scene button (at bottom of strip) ──
    const addY = viewHeight - 40;
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-add-btn-bg`,
      props: {
        x: 0, y: addY, w: SCENE_STRIP_WIDTH, h: 32,
        fillR: 0.12, fillG: 0.12, fillB: 0.22, fillA: 1,
      },
    });
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-add-btn-border`,
      props: {
        x: 0, y: addY - 1, w: SCENE_STRIP_WIDTH, h: 1,
        fillR: CELL_BORDER.r, fillG: CELL_BORDER.g, fillB: CELL_BORDER.b, fillA: CELL_BORDER.a,
      },
    });
    nodes.push({
      type: 'text',
      key: `${this.widgetId}-add-icon`,
      props: {
        text: '+',
        x: 20, y: addY + 20,
        r: ADD_SCENE_COLOR.r, g: ADD_SCENE_COLOR.g, b: ADD_SCENE_COLOR.b, a: ADD_SCENE_COLOR.a,
        fontSize: 18,
      },
    });
    nodes.push({
      type: 'text',
      key: `${this.widgetId}-add-label`,
      props: {
        text: 'Add Scene',
        x: 36, y: addY + 18,
        r: 0.42, g: 0.54, b: 0.67, a: 1,
        fontSize: 11,
      },
    });

    return nodes;
  }

  // ── Clip Grid ──────────────────────────────────────────────────────

  private _renderClipGrid(
    tracks: TrackData[],
    scenes: SceneData[],
    scrollX: number,
    scrollY: number,
    viewWidth: number,
    viewHeight: number,
    columnWidth: number,
  ): VNode[] {
    const nodes: VNode[] = [];
    const gridLeft = SCENE_STRIP_WIDTH;
    const gridWidth = tracks.length * columnWidth;

    // Column header background
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-grid-header-bg`,
      props: {
        x: gridLeft, y: CROSSFADER_HEIGHT,
        w: gridWidth, h: COLUMN_HEADER_HEIGHT,
        fillR: HEADER_BG.r, fillG: HEADER_BG.g, fillB: HEADER_BG.b, fillA: HEADER_BG.a,
      },
    });

    // Column headers (track names)
    const startCol = Math.max(0, Math.floor(scrollX / columnWidth));
    const endCol = Math.min(
      tracks.length,
      Math.ceil((scrollX + viewWidth - SCENE_STRIP_WIDTH) / columnWidth),
    );

    for (let col = startCol; col < endCol; col++) {
      const track = tracks[col];
      if (!track) continue;

      const hx = col * columnWidth - scrollX + gridLeft;

      // Truncated track name
      const maxLen = Math.max(3, Math.floor(columnWidth / 7));
      const name = track.name.length > maxLen
        ? track.name.slice(0, maxLen - 1) + '\u2026'
        : track.name;

      nodes.push({
        type: 'text',
        key: `${this.widgetId}-col-header-${col}`,
        props: {
          text: name,
          x: hx + columnWidth / 2, y: CROSSFADER_HEIGHT + COLUMN_HEADER_HEIGHT / 2 + 4,
          r: HEADER_TEXT.r, g: HEADER_TEXT.g, b: HEADER_TEXT.b, a: HEADER_TEXT.a,
          fontSize: 11,
        },
      });

      // Column divider
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-col-divider-${col}`,
        props: {
          x: hx + columnWidth - 1, y: CROSSFADER_HEIGHT, w: 1, h: COLUMN_HEADER_HEIGHT,
          fillR: HEADER_BORDER.r, fillG: HEADER_BORDER.g, fillB: HEADER_BORDER.b, fillA: HEADER_BORDER.a,
        },
      });
    }

    // ── Grid cells (virtual culling) ──
    const startRow = Math.max(0, Math.floor(scrollY / SLOT_HEIGHT));
    const endRow = Math.min(
      scenes.length,
      Math.ceil((scrollY + viewHeight - CROSSFADER_HEIGHT - COLUMN_HEADER_HEIGHT) / SLOT_HEIGHT) + 1,
    );

    const startVisCol = Math.max(0, Math.floor(scrollX / columnWidth));
    const endVisCol = Math.min(
      tracks.length,
      Math.ceil((scrollX + viewWidth - SCENE_STRIP_WIDTH) / columnWidth) + 1,
    );

    for (let row = startRow; row < endRow; row++) {
      const scene = scenes[row];
      if (!scene) continue;

      const cellY = CROSSFADER_HEIGHT + COLUMN_HEADER_HEIGHT + row * SLOT_HEIGHT - scrollY;

      for (let col = startVisCol; col < endVisCol; col++) {
        const track = tracks[col];
        if (!track) continue;

        const cellX = col * columnWidth - scrollX + gridLeft;

        // Cell background (alternating)
        const cellBg = row % 2 === 0 ? BG : ALT_ROW;
        nodes.push({
          type: 'rect',
          key: `${this.widgetId}-cell-bg-${row}-${col}`,
          props: {
            x: cellX, y: cellY, w: columnWidth, h: SLOT_HEIGHT,
            fillR: cellBg.r, fillG: cellBg.g, fillB: cellBg.b, fillA: cellBg.a,
          },
        });

        // Query slot data
        const slot = this.props.dataSource.getSlot(track.id, scene.id);

        if (slot.has_clip) {
          this._renderClipSlot(nodes, slot, cellX, cellY, columnWidth, row, col);
        }
      }
    }

    // ── Grid lines (horizontal — scene row separators) ──
    for (let row = startRow; row <= endRow; row++) {
      const ly = CROSSFADER_HEIGHT + COLUMN_HEADER_HEIGHT + row * SLOT_HEIGHT - scrollY;
      if (ly < CROSSFADER_HEIGHT || ly > viewHeight) continue;

      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-grid-h-${row}`,
        props: {
          x: SCENE_STRIP_WIDTH, y: ly, w: tracks.length * columnWidth, h: 0.5,
          fillR: CELL_BORDER.r, fillG: CELL_BORDER.g, fillB: CELL_BORDER.b, fillA: CELL_BORDER.a,
        },
      });
    }

    // ── Grid lines (vertical — track column separators) ──
    for (let col = startCol; col <= endCol; col++) {
      const lx = col * columnWidth - scrollX + gridLeft;
      if (lx < SCENE_STRIP_WIDTH || lx > viewWidth) continue;

      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-grid-v-${col}`,
        props: {
          x: lx, y: CROSSFADER_HEIGHT + COLUMN_HEADER_HEIGHT, w: 0.5,
          h: viewHeight - CROSSFADER_HEIGHT - COLUMN_HEADER_HEIGHT,
          fillR: CELL_BORDER.r, fillG: CELL_BORDER.g, fillB: CELL_BORDER.b, fillA: CELL_BORDER.a,
        },
      });
    }

    return nodes;
  }

  private _renderClipSlot(
    out: VNode[],
    slot: {
      clip_name: string;
      clip_color: number;
      has_clip: boolean;
      state: string;
      follow_action: string;
    },
    cellX: number,
    cellY: number,
    columnWidth: number,
    row: number,
    col: number,
  ): void {
    const padX = 2;
    const padY = 2;
    const clipW = columnWidth - padX * 2;
    const clipH = SLOT_HEIGHT - padY * 2;

    // Determine fill based on state
    let fillColor = CLIP_FILL;
    if (slot.state === 'Playing') {
      fillColor = CLIP_PLAYING;
    } else if (slot.state === 'Triggered') {
      fillColor = CLIP_TRIGGERED;
    }

    // Clip background rect
    out.push({
      type: 'rect',
      key: `${this.widgetId}-slot-${row}-${col}`,
      props: {
        x: cellX + padX, y: cellY + padY,
        w: clipW, h: clipH,
        fillR: fillColor.r, fillG: fillColor.g, fillB: fillColor.b, fillA: fillColor.a,
        cornerRadius: 3,
      },
      eventHandlers: {
        click: () => this._launchSlot(col, row),
      },
    });

    // Clip border
    out.push({
      type: 'rect',
      key: `${this.widgetId}-slot-border-${row}-${col}`,
      props: {
        x: cellX + padX, y: cellY + padY,
        w: clipW, h: clipH,
        fillR: CELL_BORDER.r, fillG: CELL_BORDER.g, fillB: CELL_BORDER.b, fillA: CELL_BORDER.a,
        strokeWidth: 1,
        cornerRadius: 3,
      },
    });

    // Clip name (with shadow)
    const maxChars = Math.max(1, Math.floor(clipW / 7));
    const displayName = slot.clip_name.length > 0
      ? slot.clip_name.slice(0, maxChars)
      : 'Clip';

    // Shadow text
    out.push({
      type: 'text',
      key: `${this.widgetId}-slot-shadow-${row}-${col}`,
      props: {
        text: displayName,
        x: cellX + padX + 5,
        y: cellY + padY + 15,
        r: CLIP_SHADOW.r, g: CLIP_SHADOW.g, b: CLIP_SHADOW.b, a: 0.4,
        fontSize: 11,
      },
    });

    // Name text
    out.push({
      type: 'text',
      key: `${this.widgetId}-slot-name-${row}-${col}`,
      props: {
        text: displayName,
        x: cellX + padX + 4,
        y: cellY + padY + 14,
        r: CLIP_TEXT.r, g: CLIP_TEXT.g, b: CLIP_TEXT.b, a: CLIP_TEXT.a,
        fontSize: 11,
      },
    });

    // Playing state indicator (green dot at top-right)
    if (slot.state === 'Playing') {
      out.push({
        type: 'rect',
        key: `${this.widgetId}-slot-playing-${row}-${col}`,
        props: {
          x: cellX + clipW - 12,
          y: cellY + padY + 4,
          w: 6, h: 6,
          fillR: SCENE_INDICATOR_GREEN.r, fillG: SCENE_INDICATOR_GREEN.g,
          fillB: SCENE_INDICATOR_GREEN.b, fillA: SCENE_INDICATOR_GREEN.a,
          cornerRadius: 3,
        },
      });
    }

    // Follow action indicator (icon at bottom-right)
    if (slot.follow_action && slot.follow_action !== 'None') {
      const icon = FOLLOW_ICON[slot.follow_action] ?? '?';
      out.push({
        type: 'text',
        key: `${this.widgetId}-slot-follow-${row}-${col}`,
        props: {
          text: icon,
          x: cellX + clipW - 6,
          y: cellY + clipH - 2,
          r: 0.6, g: 0.6, b: 0.7, a: 0.8,
          fontSize: 8,
        },
      });
    }
  }
}
