// ── GPUArrangementView — GPU-Backed Arrangement View ───────────────────
//
// Port of the Canvas 2D ArrangementView to the GPU Component tree.
// Renders track headers, clip area (audio waveform + MIDI + automation),
// playhead, grid lines, and zoom/pan transforms entirely via VNode tree
// → Bridge.serialize. Zero Canvas 2D calls.
//
// Design:
//   - Extends Component; viewport state (scroll/zoom) lives in state
//   - Data source in props provides tracks, playhead, zoom
//   - Playhead polling via setInterval on mount
//   - Clip culling: clips entirely outside viewport produce zero VNodes
//   - Grid density adapts to pixelsPerPPQN via getGridLevel()
//   - Track headers use rect + cornerRadius for circles (mute/solo)
//
// State transitions:
//   setState({ scrollY }) on wheel → dirty-mark → re-render at new position
//   playhead interval → setState({ playheadPPQN }) → dirty-mark → playhead moves

import { Component } from '../../framework/component.js';
import type { VNode } from '../../ffi/bridge.js';
import {
  TRACK_HEIGHT,
  TRACK_LIST_WIDTH,
  RULER_HEIGHT,
  BEAT_PPQN,
  SIXTEENTH_PPQN,
  DEFAULT_PIXELS_PER_PPQN,
  ArrangementDataSource,
  AnyTrackData,
  AnyClipData,
  AudioClipData,
  MidiClipData,
  isAudioClip,
  isMidiClip,
  isAutomationClip,
  isSessionCaptureClip,
  isGroupTrack,
  getGridLevel,
} from './types.js';

import type { SelectionState } from './types.js';
import type { InputEvent } from '../../ffi/types.js';

// ── Theme (mirrors Canvas 2D colors in 0..1 float) ─────────────────────

const BG = { r: 0.071, g: 0.071, b: 0.118, a: 1 };           // #12121e
const GRID_BEAT = { r: 0.118, g: 0.118, b: 0.196, a: 1 };    // #1e1e32
const GRID_BAR = { r: 0.133, g: 0.133, b: 0.220, a: 1 };     // #222238
const CLIP_BORDER = { r: 0.29, g: 0.42, b: 0.54, a: 1 };     // #4a6a8a
const CLIP_AUDIO = { r: 0.16, g: 0.33, b: 0.25, a: 1 };      // #2a5540
const CLIP_MIDI = { r: 0.16, g: 0.25, b: 0.33, a: 1 };       // #2a4055
const CLIP_AUTO = { r: 0.33, g: 0.23, b: 0.16, a: 1 };       // #553a2a
const CLIP_TEXT = { r: 0.75, g: 0.82, b: 0.88, a: 1 };       // #c0d0e0
const MUTED_OVERLAY = { r: 0, g: 0, b: 0, a: 0.4 };
const WAVEFORM = { r: 0.29, g: 0.67, b: 0.42, a: 1 };        // #4aaa6a
const WAVEFORM_LOW = { r: 0.16, g: 0.42, b: 0.25, a: 1 };    // #2a6a40
const PLAYHEAD = { r: 1, g: 0.27, b: 0.27, a: 1 };           // #ff4444
const RULER_BG = { r: 0.10, g: 0.10, b: 0.18, a: 1 };        // #1a1a2e
const RULER_STRONG = { r: 0.23, g: 0.23, b: 0.36, a: 1 };    // #3a3a5c
const RULER_WEAK = { r: 0.16, g: 0.16, b: 0.29, a: 1 };      // #2a2a4a
const BAR_NUM = { r: 0.54, g: 0.54, b: 0.67, a: 1 };         // #8a8aaa
const TRACK_BG = { r: 0.086, g: 0.086, b: 0.169, a: 1 };     // #16162b
const TRACK_ALT = { r: 0.10, g: 0.10, b: 0.19, a: 1 };       // #1a1a30
const TRACK_BORDER = { r: 0.16, g: 0.16, b: 0.25, a: 1 };    // #2a2a3f
const TRACK_TEXT = { r: 0.75, g: 0.75, b: 0.82, a: 1 };      // #c0c0d0
const TRACK_MUTED_TEXT = { r: 0.33, g: 0.33, b: 0.40, a: 1 };
const MUTE_ON = { r: 0.8, g: 0.2, b: 0.2, a: 1 };
const MUTE_OFF = { r: 0.23, g: 0.23, b: 0.35, a: 1 };
const SOLO_ON = { r: 0.87, g: 0.8, b: 0, a: 1 };
const SOLO_OFF = { r: 0.23, g: 0.23, b: 0.35, a: 1 };
const FOLD_COLOR = { r: 0.42, g: 0.42, b: 0.67, a: 1 };
const FOLD_ACTIVE = { r: 0.54, g: 0.54, b: 0.80, a: 1 };

const TYPE_BADGE_BG: Record<string, { r: number; g: number; b: number; a: number }> = {
  Audio:  { r: 0.16, g: 0.33, b: 0.23, a: 1 },
  MIDI:   { r: 0.16, g: 0.23, b: 0.33, a: 1 },
  Group:  { r: 0.33, g: 0.23, b: 0.16, a: 1 },
  VCA:    { r: 0.29, g: 0.16, b: 0.33, a: 1 },
  Return: { r: 0.16, g: 0.33, b: 0.31, a: 1 },
  Master: { r: 0.33, g: 0.16, b: 0.16, a: 1 },
};

// ─── Polling ──────────────────────────────────────────────────────────

/** Playhead polling interval in ms. */
const PLAYHEAD_POLL_MS = 50;

// ─── Props & State ────────────────────────────────────────────────────

export interface GPUArrangementViewProps {
  /** Data source for tracks, playhead, zoom. */
  dataSource: ArrangementDataSource;
}

export interface GPUArrangementViewState {
  /** Horizontal scroll offset in pixels. */
  scrollX: number;
  /** Vertical scroll offset in pixels. */
  scrollY: number;
  /** Pixels per PPQN at current zoom. */
  pixelsPerPPQN: number;
  /** Current playhead position in PPQN (synced from data source). */
  playheadPPQN: number;
  /** Viewport width in logical pixels. */
  viewWidth: number;
  /** Viewport height in logical pixels. */
  viewHeight: number;
}

// ─── GPUArrangementView ───────────────────────────────────────────────

export class GPUArrangementView extends Component<GPUArrangementViewProps, GPUArrangementViewState> {
  /** Interval handle for playhead polling. */
  private _playheadTimer: ReturnType<typeof setInterval> | null = null;

  constructor(props: GPUArrangementViewProps) {
    super(props);
    this.state = {
      scrollX: 0,
      scrollY: 0,
      pixelsPerPPQN: DEFAULT_PIXELS_PER_PPQN,
      playheadPPQN: 0,
      viewWidth: 800,
      viewHeight: 600,
    };
  }

  // ── Lifecycle ──────────────────────────────────────────────────────

  onMount(): void {
    super.onMount();
    // Initial data load
    this._syncPlayhead();
    // Start playhead polling
    this._playheadTimer = setInterval(() => {
      this._pollPlayhead();
    }, PLAYHEAD_POLL_MS);
  }

  onUnmount(): void {
    super.onUnmount();
    if (this._playheadTimer !== null) {
      clearInterval(this._playheadTimer);
      this._playheadTimer = null;
    }
  }

  // ── Data read ──────────────────────────────────────────────────────

  /**
   * Get current visible tracks from the data source.
   * Model data lives in the data source — state only tracks viewport.
   */
  private _getTracks(): AnyTrackData[] {
    return this.props.dataSource.getVisibleTracks();
  }

  /** Sync playhead PPQN from data source into state. */
  private _syncPlayhead(): void {
    this.setState({
      playheadPPQN: this.props.dataSource.getPlayheadPPQN(),
    } as Partial<GPUArrangementViewState>);
  }

  /** Poll playhead position from the data source. */
  private _pollPlayhead(): void {
    const newPos = this.props.dataSource.getPlayheadPPQN();
    if (newPos !== this.state.playheadPPQN) {
      this.setState({ playheadPPQN: newPos } as Partial<GPUArrangementViewState>);
    }
  }

  // ── Viewport mutations ─────────────────────────────────────────────

  /** Set scroll position. Triggers re-render via setState. */
  setScroll(scrollX: number, scrollY: number): void {
    this.setState({
      scrollX: Math.max(0, scrollX),
      scrollY: Math.max(0, scrollY),
    } as Partial<GPUArrangementViewState>);
  }

  /** Set zoom level (pixelsPerPPQN). Triggers re-render. */
  setZoom(pixelsPerPPQN: number): void {
    this.setState({ pixelsPerPPQN } as Partial<GPUArrangementViewState>);
  }

  /** Handle wheel event for scroll/zoom. */
  onWheel(event: InputEvent): void {
    // Simple vertical scroll (deltaY from event)
    // In production this would differentiate scroll vs zoom (Ctrl+wheel)
    const newScrollY = Math.max(0, this.state.scrollY - event.y * 0.5);
    this.setState({ scrollY: newScrollY } as Partial<GPUArrangementViewState>);
  }

  // ── Selection (placeholder — routes to selection manager in Canvas 2D) ─

  getSelection(): SelectionState {
    return { selectedClipIds: new Set(), hoveredClipId: null };
  }

  // ── Rendering ──────────────────────────────────────────────────────

  render(): VNode {
    const tracks = this._getTracks();
    const { scrollX, scrollY, pixelsPerPPQN, playheadPPQN, viewWidth, viewHeight } = this.state;
    const children: VNode[] = [];

    // ── 1. Main background ──
    children.push({
      type: 'rect',
      key: `${this.widgetId}-bg`,
      props: {
        x: 0, y: 0, w: viewWidth, h: viewHeight,
        fillR: BG.r, fillG: BG.g, fillB: BG.b, fillA: BG.a,
      },
    });

    // ── 2. Track list background (left column) ──
    children.push({
      type: 'rect',
      key: `${this.widgetId}-tl-bg`,
      props: {
        x: 0, y: 0, w: TRACK_LIST_WIDTH, h: viewHeight,
        fillR: TRACK_BG.r, fillG: TRACK_BG.g, fillB: TRACK_BG.b, fillA: TRACK_BG.a,
      },
    });

    // ── 3. Grid lines (beat/bar vertical lines in the clip area) ──
    const gridChildren = this._renderGridLines(pixelsPerPPQN, scrollX, viewWidth, viewHeight);
    for (let i = 0; i < gridChildren.length; i++) {
      children.push(gridChildren[i]);
    }

    // ── 4. Track header rows ──
    const headerChildren = this._renderTrackHeaders(tracks, scrollY, viewHeight);
    for (let i = 0; i < headerChildren.length; i++) {
      children.push(headerChildren[i]);
    }

    // ── 5. Clips on each visible track ──
    const clipChildren = this._renderClips(tracks, scrollX, scrollY, pixelsPerPPQN, viewWidth, viewHeight);
    for (let i = 0; i < clipChildren.length; i++) {
      children.push(clipChildren[i]);
    }

    // ── 6. Track list right border ──
    children.push({
      type: 'rect',
      key: `${this.widgetId}-tl-border`,
      props: {
        x: TRACK_LIST_WIDTH - 1, y: 0, w: 1, h: viewHeight,
        fillR: TRACK_BORDER.r, fillG: TRACK_BORDER.g, fillB: TRACK_BORDER.b, fillA: TRACK_BORDER.a,
      },
    });

    // ── 7. Time ruler (top strip) ──
    const rulerChildren = this._renderRuler(pixelsPerPPQN, scrollX, viewWidth, playheadPPQN);
    for (let i = 0; i < rulerChildren.length; i++) {
      children.push(rulerChildren[i]);
    }

    // ── 8. Playhead (vertical red line in clip area) ──
    const phX = playheadPPQN * pixelsPerPPQN - scrollX + TRACK_LIST_WIDTH;
    if (phX >= TRACK_LIST_WIDTH && phX <= viewWidth) {
      children.push({
        type: 'rect',
        key: `${this.widgetId}-playhead`,
        props: {
          x: phX, y: RULER_HEIGHT, w: 2, h: viewHeight - RULER_HEIGHT,
          fillR: PLAYHEAD.r, fillG: PLAYHEAD.g, fillB: PLAYHEAD.b, fillA: PLAYHEAD.a,
        },
      });
    }

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x: 0, y: 0, w: viewWidth, h: viewHeight },
      children,
    };
  }

  // ── Grid lines ─────────────────────────────────────────────────────

  private _renderGridLines(
    pixelsPerPPQN: number,
    scrollX: number,
    viewWidth: number,
    viewHeight: number,
  ): VNode[] {
    const lines: VNode[] = [];
    if (pixelsPerPPQN <= 0) return lines;

    const stepPPQN = BEAT_PPQN; // Beat grid
    const startPPQN = Math.max(0, Math.floor(scrollX / pixelsPerPPQN / stepPPQN) * stepPPQN);
    const endPPQN = Math.ceil((scrollX + viewWidth - TRACK_LIST_WIDTH) / pixelsPerPPQN / stepPPQN) * stepPPQN;

    for (let ppqn = startPPQN; ppqn <= endPPQN; ppqn += stepPPQN) {
      const x = ppqn * pixelsPerPPQN - scrollX + TRACK_LIST_WIDTH;
      if (x < TRACK_LIST_WIDTH || x > viewWidth) continue;

      const isBar = (ppqn % (BEAT_PPQN * 4) === 0);
      const color = isBar ? GRID_BAR : GRID_BEAT;
      lines.push({
        type: 'rect',
        key: `${this.widgetId}-grid-${ppqn}`,
        props: {
          x, y: RULER_HEIGHT, w: isBar ? 1 : 0.5,
          h: viewHeight - RULER_HEIGHT,
          fillR: color.r, fillG: color.g, fillB: color.b, fillA: color.a,
        },
      });
    }
    return lines;
  }

  // ── Track headers ─────────────────────────────────────────────────

  private _renderTrackHeaders(
    tracks: AnyTrackData[],
    scrollY: number,
    viewHeight: number,
  ): VNode[] {
    const nodes: VNode[] = [];
    const firstVisible = Math.max(0, Math.floor(scrollY / TRACK_HEIGHT));
    const lastVisible = Math.min(
      tracks.length - 1,
      Math.ceil((scrollY + viewHeight) / TRACK_HEIGHT),
    );

    for (let i = firstVisible; i <= lastVisible; i++) {
      const track = tracks[i];
      if (!track) continue;

      const y = i * TRACK_HEIGHT - scrollY;
      const isAlt = i % 2 === 1;

      // Row background
      if (isAlt) {
        nodes.push({
          type: 'rect',
          key: `${this.widgetId}-tl-row-${i}`,
          props: {
            x: 0, y, w: TRACK_LIST_WIDTH, h: TRACK_HEIGHT,
            fillR: TRACK_ALT.r, fillG: TRACK_ALT.g, fillB: TRACK_ALT.b, fillA: TRACK_ALT.a,
          },
        });
      }

      // Bottom border
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-tl-border-${i}`,
        props: {
          x: 0, y: y + TRACK_HEIGHT - 1, w: TRACK_LIST_WIDTH, h: 1,
          fillR: TRACK_BORDER.r, fillG: TRACK_BORDER.g, fillB: TRACK_BORDER.b, fillA: TRACK_BORDER.a,
        },
      });

      // Type badge
      const badgeBg = TYPE_BADGE_BG[track.type] ?? { r: 0.2, g: 0.2, b: 0.2, a: 1 };
      const badgeX = isGroupTrack(track) ? 32 : 14;
      nodes.push({
        type: 'text',
        key: `${this.widgetId}-tl-badge-${i}`,
        props: {
          text: track.type.toUpperCase(),
          x: badgeX, y: y + TRACK_HEIGHT - 4,
          r: badgeBg.r, g: badgeBg.g, b: badgeBg.b, a: 1,
          fontSize: 9,
        },
      });

      // Track name
      const nameX = isGroupTrack(track) ? 32 : 14;
      const nameColor = track.muted ? TRACK_MUTED_TEXT : TRACK_TEXT;
      nodes.push({
        type: 'text',
        key: `${this.widgetId}-tl-name-${i}`,
        props: {
          text: track.name,
          x: nameX, y: y + TRACK_HEIGHT / 2 + 4,
          r: nameColor.r, g: nameColor.g, b: nameColor.b, a: nameColor.a,
          fontSize: 13,
        },
      });

      // Fold arrow for GroupTrack
      if (isGroupTrack(track)) {
        const foldSymbol = track.folded ? '\u25B6' : '\u25BC'; // ▶ or ▼
        nodes.push({
          type: 'text',
          key: `${this.widgetId}-tl-fold-${i}`,
          props: {
            text: foldSymbol,
            x: 8, y: y + TRACK_HEIGHT / 2 + 4,
            r: track.folded ? FOLD_ACTIVE.r : FOLD_COLOR.r,
            g: track.folded ? FOLD_ACTIVE.g : FOLD_COLOR.g,
            b: track.folded ? FOLD_ACTIVE.b : FOLD_COLOR.b,
            a: 1,
            fontSize: 11,
          },
        });
      }

      // Mute button (circle via rect + cornerRadius)
      const muteX = TRACK_LIST_WIDTH - 50;
      const muteCy = y + TRACK_HEIGHT / 2;
      const btnR = 7;
      const muteColor = track.muted ? MUTE_ON : MUTE_OFF;
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-tl-mute-${i}`,
        props: {
          x: muteX - btnR, y: muteCy - btnR, w: btnR * 2, h: btnR * 2,
          fillR: muteColor.r, fillG: muteColor.g, fillB: muteColor.b, fillA: muteColor.a,
          cornerRadius: btnR,
        },
      });
      nodes.push({
        type: 'text',
        key: `${this.widgetId}-tl-mute-label-${i}`,
        props: {
          text: 'M',
          x: muteX - 3, y: muteCy + 3,
          r: 0.87, g: 0.87, b: 0.87, a: 1,
          fontSize: 7,
        },
      });

      // Solo button
      const soloX = TRACK_LIST_WIDTH - 28;
      const soloCy = y + TRACK_HEIGHT / 2;
      const soloColor = track.soloed ? SOLO_ON : SOLO_OFF;
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-tl-solo-${i}`,
        props: {
          x: soloX - btnR, y: soloCy - btnR, w: btnR * 2, h: btnR * 2,
          fillR: soloColor.r, fillG: soloColor.g, fillB: soloColor.b, fillA: soloColor.a,
          cornerRadius: btnR,
        },
      });
      nodes.push({
        type: 'text',
        key: `${this.widgetId}-tl-solo-label-${i}`,
        props: {
          text: 'S',
          x: soloX - 3, y: soloCy + 3,
          r: 0.13, g: 0.13, b: 0.13, a: 1,
          fontSize: 7,
        },
      });
    }
    return nodes;
  }

  // ── Clip rendering ────────────────────────────────────────────────

  private _renderClips(
    tracks: AnyTrackData[],
    scrollX: number,
    scrollY: number,
    pixelsPerPPQN: number,
    viewWidth: number,
    viewHeight: number,
  ): VNode[] {
    const nodes: VNode[] = [];
    if (pixelsPerPPQN <= 0) return nodes;

    const canvasLeft = TRACK_LIST_WIDTH;
    const canvasWidth = viewWidth - TRACK_LIST_WIDTH;
    const rulerBottom = RULER_HEIGHT;

    const firstTrack = Math.max(0, Math.floor(scrollY / TRACK_HEIGHT));
    const lastTrack = Math.min(
      tracks.length - 1,
      Math.ceil((scrollY + viewHeight - rulerBottom) / TRACK_HEIGHT),
    );

    for (let i = firstTrack; i <= lastTrack; i++) {
      const track = tracks[i];
      if (!track) continue;

      const trackY = rulerBottom + i * TRACK_HEIGHT - scrollY;

      // Alternating row background
      if (i % 2 === 1) {
        nodes.push({
          type: 'rect',
          key: `${this.widgetId}-clip-row-${i}`,
          props: {
            x: canvasLeft, y: trackY, w: canvasWidth, h: TRACK_HEIGHT,
            fillR: TRACK_ALT.r, fillG: TRACK_ALT.g, fillB: TRACK_ALT.b, fillA: 0.5,
          },
        });
      }

      // Render each clip on this track
      for (const clip of track.clips) {
        this._renderClip(nodes, clip, trackY, pixelsPerPPQN, scrollX, canvasLeft, viewWidth);
      }
    }
    return nodes;
  }

  private _renderClip(
    out: VNode[],
    clip: AnyClipData,
    trackY: number,
    pixelsPerPPQN: number,
    scrollX: number,
    canvasLeft: number,
    viewWidth: number,
  ): void {
    const x = clip.start_ppqn * pixelsPerPPQN - scrollX + canvasLeft;
    const w = clip.length_ppqn * pixelsPerPPQN;
    const h = TRACK_HEIGHT - 2;
    const y = trackY + 1;

    // ── Clip culling: skip if entirely outside viewport ──
    if (x + w < canvasLeft || x > canvasLeft + viewWidth) return;

    // Determine clip background color
    let fillColor = CLIP_AUDIO;
    if (isMidiClip(clip)) fillColor = CLIP_MIDI;
    else if (isAutomationClip(clip)) fillColor = CLIP_AUTO;

    const isMuted = clip.muted;

    // Clip background rect
    if (isMuted) {
      out.push({
        type: 'rect',
        key: `${this.widgetId}-clip-${clip.id.value}`,
        props: {
          x, y, w, h,
          fillR: MUTED_OVERLAY.r, fillG: MUTED_OVERLAY.g, fillB: MUTED_OVERLAY.b, fillA: MUTED_OVERLAY.a,
          cornerRadius: 3,
        },
      });
    } else {
      out.push({
        type: 'rect',
        key: `${this.widgetId}-clip-${clip.id.value}`,
        props: {
          x, y, w, h,
          fillR: fillColor.r, fillG: fillColor.g, fillB: fillColor.b, fillA: fillColor.a,
          cornerRadius: 3,
        },
      });
    }

    // Clip border
    out.push({
      type: 'rect',
      key: `${this.widgetId}-clip-border-${clip.id.value}`,
      props: {
        x, y, w, h,
        fillR: CLIP_BORDER.r, fillG: CLIP_BORDER.g, fillB: CLIP_BORDER.b, fillA: CLIP_BORDER.a,
        strokeWidth: 1,
        cornerRadius: 3,
      },
    });

    // Clip name
    const displayName = clip.name.length > 0 ? clip.name : `Clip ${clip.id.value}`;
    out.push({
      type: 'text',
      key: `${this.widgetId}-clip-name-${clip.id.value}`,
      props: {
        text: displayName,
        x: x + 4, y: y + 14,
        r: CLIP_TEXT.r, g: CLIP_TEXT.g, b: CLIP_TEXT.b, a: CLIP_TEXT.a,
        fontSize: 11,
      },
    });

    // ── Waveform for AudioClips ──
    if (isAudioClip(clip) && clip.waveform && clip.waveform.peaks.length > 0) {
      this._renderWaveform(out, clip, x, y, w, h);
    }

    // ── MIDI density for MidiClips ──
    if (isMidiClip(clip) && clip.note_density !== undefined) {
      this._renderMidiDensity(out, clip, x, y, w, h);
    }

    // ── Muted overlay (semi-transparent on top) ──
    if (isMuted) {
      out.push({
        type: 'rect',
        key: `${this.widgetId}-clip-mute-${clip.id.value}`,
        props: {
          x, y, w, h,
          fillR: 0, fillG: 0, fillB: 0, fillA: 0.25,
          cornerRadius: 3,
        },
      });
    }

    // ── Session capture badge ──
    if (isSessionCaptureClip(clip)) {
      out.push({
        type: 'text',
        key: `${this.widgetId}-clip-session-${clip.id.value}`,
        props: {
          text: 'S',
          x: x + w - 10, y: y + h - 4,
          r: 0.53, g: 0.87, b: 0.53, a: 1,
          fontSize: 10,
        },
      });
    }
  }

  /** Render waveform as a series of vertical bars (approximating polygon fill). */
  private _renderWaveform(
    out: VNode[],
    clip: AudioClipData,
    clipX: number,
    clipY: number,
    clipW: number,
    clipH: number,
  ): void {
    const { peaks, valleys } = clip.waveform!;
    const midY = clipY + clipH / 2;
    const waveformAreaH = clipH * 0.6;
    const bandTop = midY - waveformAreaH / 2;

    // Use 1 bar per 2px of clip width to keep VNode count reasonable
    const barWidth = 2;
    const numSamples = Math.min(peaks.length, Math.max(1, Math.floor(clipW / barWidth)));
    if (numSamples < 2) return;

    for (let i = 0; i < numSamples; i++) {
      const sampleIdx = Math.floor((i / numSamples) * peaks.length);
      const bx = clipX + (i / numSamples) * clipW;
      const peak = peaks[sampleIdx] ?? 0.5;
      const valley = valleys[sampleIdx] ?? 0.5;

      const topY = bandTop + (1 - peak) * waveformAreaH;
      const bottomY = bandTop + (1 - valley) * waveformAreaH;
      const barH = bottomY - topY;

      if (barH > 0.5) {
        out.push({
          type: 'rect',
          key: `${this.widgetId}-wf-${clip.id.value}-${i}`,
          props: {
            x: bx, y: topY, w: Math.max(1, barWidth - 0.5), h: barH,
            fillR: WAVEFORM.r, fillG: WAVEFORM.g, fillB: WAVEFORM.b, fillA: WAVEFORM.a,
          },
        });
      }
    }

    // Center line
    out.push({
      type: 'rect',
      key: `${this.widgetId}-wf-center-${clip.id.value}`,
      props: {
        x: clipX, y: midY, w: clipW, h: 0.5,
        fillR: WAVEFORM_LOW.r, fillG: WAVEFORM_LOW.g, fillB: WAVEFORM_LOW.b, fillA: WAVEFORM_LOW.a,
      },
    });
  }

  /** Render MIDI note density as vertical bars. */
  private _renderMidiDensity(
    out: VNode[],
    clip: MidiClipData,
    clipX: number,
    clipY: number,
    clipW: number,
    clipH: number,
  ): void {
    const density = clip.note_density ?? 0.5;
    const barCount = Math.max(4, Math.min(64, Math.floor(clipW / 8)));
    const barW = clipW / barCount;

    for (let i = 0; i < barCount; i++) {
      const phase = Math.sin(i * 1.5 + 1) * 0.3 + 0.7;
      const h = Math.max(3, clipH * 0.7 * density * phase);
      const bx = clipX + i * barW;
      out.push({
        type: 'rect',
        key: `${this.widgetId}-midi-${clip.id.value}-${i}`,
        props: {
          x: bx + 1, y: clipY + clipH - h - 2,
          w: Math.max(1, barW - 2), h,
          fillR: 0.33, fillG: 0.6, fillB: 0.8, fillA: 1,
        },
      });
    }
  }

  // ── Time Ruler ────────────────────────────────────────────────────

  private _renderRuler(
    pixelsPerPPQN: number,
    scrollX: number,
    viewWidth: number,
    playheadPPQN: number,
  ): VNode[] {
    const nodes: VNode[] = [];
    const rulerLeft = TRACK_LIST_WIDTH;
    const rulerWidth = viewWidth - TRACK_LIST_WIDTH;

    // Ruler background
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-ruler-bg`,
      props: {
        x: rulerLeft, y: 0, w: rulerWidth, h: RULER_HEIGHT,
        fillR: RULER_BG.r, fillG: RULER_BG.g, fillB: RULER_BG.b, fillA: RULER_BG.a,
      },
    });

    // Ruler bottom border
    nodes.push({
      type: 'rect',
      key: `${this.widgetId}-ruler-border`,
      props: {
        x: rulerLeft, y: RULER_HEIGHT - 1, w: rulerWidth, h: 1,
        fillR: RULER_STRONG.r, fillG: RULER_STRONG.g, fillB: RULER_STRONG.b, fillA: RULER_STRONG.a,
      },
    });

    if (pixelsPerPPQN <= 0) return nodes;

    // Grid level
    const gridLevel = getGridLevel(pixelsPerPPQN);

    // Visible PPQN range
    const startPPQN = Math.max(0, Math.floor((rulerLeft + scrollX) / pixelsPerPPQN));
    const endPPQN = Math.ceil((viewWidth + scrollX) / pixelsPerPPQN);

    const stepPPQN = gridLevel === 0 /* Sixteenths */
      ? SIXTEENTH_PPQN
      : gridLevel === 1 /* Beats */
        ? 960 /* PPQN */
        : 960 * 4; /* BAR_PPQN */

    const aligned = Math.floor(startPPQN / stepPPQN) * stepPPQN;

    for (let ppqn = aligned; ppqn <= endPPQN; ppqn += stepPPQN) {
      const x = ppqn * pixelsPerPPQN - scrollX + rulerLeft;
      if (x < rulerLeft || x > rulerLeft + rulerWidth) continue;

      if (gridLevel === 0) {
        // Sixteenths — weak tick
        nodes.push({
          type: 'rect',
          key: `${this.widgetId}-ruler-tick-${ppqn}`,
          props: {
            x, y: RULER_HEIGHT - 3, w: 0.5, h: 3,
            fillR: RULER_WEAK.r, fillG: RULER_WEAK.g, fillB: RULER_WEAK.b, fillA: RULER_WEAK.a,
          },
        });
      } else if (gridLevel === 1) {
        // Beats — medium tick + dot
        nodes.push({
          type: 'rect',
          key: `${this.widgetId}-ruler-beat-${ppqn}`,
          props: {
            x, y: RULER_HEIGHT - 5, w: 1, h: 5,
            fillR: RULER_WEAK.r, fillG: RULER_WEAK.g, fillB: RULER_WEAK.b, fillA: RULER_WEAK.a,
          },
        });
      } else {
        // Bars — strong tick + number
        nodes.push({
          type: 'rect',
          key: `${this.widgetId}-ruler-bar-${ppqn}`,
          props: {
            x, y: 0, w: 1, h: 8,
            fillR: RULER_STRONG.r, fillG: RULER_STRONG.g, fillB: RULER_STRONG.b, fillA: RULER_STRONG.a,
          },
        });
        const barNum = Math.floor(ppqn / (960 * 4)) + 1;
        nodes.push({
          type: 'text',
          key: `${this.widgetId}-ruler-bar-label-${ppqn}`,
          props: {
            text: String(barNum),
            x, y: 14,
            r: BAR_NUM.r, g: BAR_NUM.g, b: BAR_NUM.b, a: BAR_NUM.a,
            fontSize: 11,
          },
        });
      }
    }

    // Playhead in ruler (extends full ruler height)
    const phX = playheadPPQN * pixelsPerPPQN - scrollX + rulerLeft;
    if (phX >= rulerLeft && phX <= rulerLeft + rulerWidth) {
      nodes.push({
        type: 'rect',
        key: `${this.widgetId}-ruler-playhead`,
        props: {
          x: phX, y: 0, w: 2, h: RULER_HEIGHT,
          fillR: PLAYHEAD.r, fillG: PLAYHEAD.g, fillB: PLAYHEAD.b, fillA: PLAYHEAD.a,
        },
      });
    }

    return nodes;
  }
}
