// ARIA DAW — ArrangementView
//
// Main arrangement timeline view: coordinates TimeRuler, TrackList,
// ClipCanvas, SelectionManager, and FadeOverlay. Owns the
// requestAnimationFrame render loop and mouse event dispatch.

import {
  TRACK_LIST_WIDTH,
  DEFAULT_PIXELS_PER_PPQN,
  ArrangementDataSource,
  SelectionState,
  AnyClipData,
  TrackID,
  SessionCaptureClipData,
  clipId,
} from './types.js';

import { TimeRuler, TimeRulerConfig } from './TimeRuler.js';
import { TrackList, TrackListConfig } from './TrackList.js';
import { ClipCanvas, ClipCanvasConfig } from './ClipCanvas.js';
import { SelectionManager } from './SelectionManager.js';
import { FadeOverlay, FadeOverlayConfig } from './FadeOverlay.js';

import type { View } from '../index.js';

// ─── ArrangementView ────────────────────────────────────────────

export class ArrangementView implements View {
  readonly name = 'Arrangement';

  // Sub-components
  private timeRuler = new TimeRuler();
  private trackList = new TrackList();
  private clipCanvas = new ClipCanvas();
  private selectionMgr = new SelectionManager();
  private fadeOverlay = new FadeOverlay();

  // Data source (provided by host app or C++ bridge)
  private dataSource: ArrangementDataSource;

  // Canvas
  private canvasEl: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;

  // Viewport state
  private scrollX = 0;
  private scrollY = 0;
  private viewWidth = 800;
  private viewHeight = 600;

  // Animation frame handle
  private rafId: number | null = null;

  // ─── Session Capture Callback ───────────────────────────────
  /** Callback invoked when a session clip is captured into the arrangement.
   *  The host app should handle dispatching to the C++ model.
   *  Parameters: the created SessionCaptureClipData, target track ID, and
   *  target start PPQN position. */
  onCaptureFromSession: ((
    clip: AnyClipData,
    targetTrack: TrackID,
    targetStartPPQN: number,
  ) => void) | null = null;

  // Mouse event handlers (bound so we can remove them)
  private boundMouseDown: (e: MouseEvent) => void;
  private boundMouseMove: (e: MouseEvent) => void;
  private boundMouseUp: (e: MouseEvent) => void;
  private boundKeyDown: (e: KeyboardEvent) => void;
  private boundResize: () => void;

  constructor(dataSource: ArrangementDataSource) {
    this.dataSource = dataSource;

    // Bind event handlers
    this.boundMouseDown = this.onMouseDown.bind(this);
    this.boundMouseMove = this.onMouseMove.bind(this);
    this.boundMouseUp = this.onMouseUp.bind(this);
    this.boundKeyDown = this.onKeyDown.bind(this);
    this.boundResize = this.onResize.bind(this);
  }

  // ─── View Interface ─────────────────────────────────────────

  activate(): void {
    // Event listeners are attached in initialize() when canvas is provided
    // This is called when the view becomes active
  }

  deactivate(): void {
    this.stopRenderLoop();
    this.removeEventListeners();
    this.canvasEl = null;
    this.ctx = null;
  }

  render(): void {
    // Render is driven by rAF loop, but this provides a manual trigger
    if (this.ctx && this.canvasEl) {
      this.drawFrame();
    }
  }

  // ─── Canvas Setup ───────────────────────────────────────────

  /** Attach to a canvas element and start the render loop. */
  attach(canvas: HTMLCanvasElement): void {
    this.canvasEl = canvas;
    this.ctx = canvas.getContext('2d');

    if (!this.ctx) {
      console.error('[ArrangementView] Could not get 2D context');
      return;
    }

    this.updateViewSize();
    this.addEventListeners();
    this.startRenderLoop();
  }

  /** Detach from canvas and stop the render loop. */
  detach(): void {
    this.stopRenderLoop();
    this.removeEventListeners();
    this.canvasEl = null;
    this.ctx = null;
  }

  // ─── Data Source ────────────────────────────────────────────

  /** Update the data source (e.g., after project load). */
  setDataSource(ds: ArrangementDataSource): void {
    this.dataSource = ds;
  }

  /** Get current selection state (for external query). */
  getSelection(): SelectionState {
    return this.selectionMgr.state;
  }

  // ─── Session Capture ──────────────────────────────────────

  /**
   * Capture a clip from the Session view into the Arrangement.
   *
   * Creates a SessionCaptureClipData from the source clip and emits it
   * via the onCaptureFromSession callback for the host app to dispatch.
   *
   * @param sourceTrackId     The track ID the clip came from in Session.
   * @param sourceSceneId     The scene ID the clip came from in Session.
   * @param sourceClip        The original clip data from the Session.
   * @param targetTrackId     The track to place the captured clip on.
   * @param targetStartPPQN   The PPQN position to place the captured clip.
   */
  captureFromSession(
    sourceTrackId: TrackID,
    sourceSceneId: { value: number },
    sourceClip: AnyClipData,
    targetTrackId: TrackID,
    targetStartPPQN: number,
  ): void {
    const capturedClip: SessionCaptureClipData = {
      id: clipId(-(sourceClip.id.value)), // negative ID = captured from session
      start_ppqn: targetStartPPQN,
      length_ppqn: sourceClip.length_ppqn,
      name: `Session: ${sourceClip.name || `Clip ${sourceClip.id.value}`}`,
      color: sourceClip.color,
      gain: sourceClip.gain,
      muted: sourceClip.muted,
      fade_in_ppqn: sourceClip.fade_in_ppqn,
      fade_out_ppqn: sourceClip.fade_out_ppqn,
      fade_in_shape: sourceClip.fade_in_shape,
      fade_out_shape: sourceClip.fade_out_shape,
      looping: sourceClip.looping,
      loop_start_ppqn: sourceClip.loop_start_ppqn,
      loop_end_ppqn: sourceClip.loop_end_ppqn,
      track_id: targetTrackId,
      type: 'SessionCapture',
      capture_source_track_id: sourceTrackId,
      capture_source_scene_id: sourceSceneId,
    };

    if (this.onCaptureFromSession) {
      this.onCaptureFromSession(capturedClip, targetTrackId, targetStartPPQN);
    }
  }

  // ─── Rendering ──────────────────────────────────────────────

  private startRenderLoop(): void {
    this.stopRenderLoop();
    const loop = () => {
      this.drawFrame();
      this.rafId = requestAnimationFrame(loop);
    };
    this.rafId = requestAnimationFrame(loop);
  }

  private stopRenderLoop(): void {
    if (this.rafId !== null) {
      cancelAnimationFrame(this.rafId);
      this.rafId = null;
    }
  }

  private drawFrame(): void {
    const ctx = this.ctx;
    const canvas = this.canvasEl;
    if (!ctx || !canvas) return;

    const pixelsPerPPQN = this.dataSource.getPixelsPerPPQN() || DEFAULT_PIXELS_PER_PPQN;
    const playheadPPQN = this.dataSource.getPlayheadPPQN();
    const visibleTracks = this.dataSource.getVisibleTracks();

    // ── Clear ──
    ctx.clearRect(0, 0, this.viewWidth, this.viewHeight);

    // ── 1. Time Ruler ──
    const rulerConfig: TimeRulerConfig = {
      scrollX: this.scrollX,
      viewWidth: this.viewWidth,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    };
    this.timeRuler.render(ctx, rulerConfig, playheadPPQN);

    // ── 2. Track List ──
    const trackListConfig: TrackListConfig = {
      scrollY: this.scrollY,
      viewHeight: this.viewHeight,
      showEmpty: true,
    };
    this.trackList.render(ctx, visibleTracks, trackListConfig);

    // ── 3. Clip Canvas ──
    const clipConfig: ClipCanvasConfig = {
      scrollX: this.scrollX,
      scrollY: this.scrollY,
      viewWidth: this.viewWidth,
      viewHeight: this.viewHeight,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    };
    this.clipCanvas.render(
      ctx, visibleTracks, clipConfig,
      this.selectionMgr.state,
    );

    // ── 4. Fade Overlay ──
    const fadeConfig: FadeOverlayConfig = {
      scrollX: this.scrollX,
      scrollY: this.scrollY,
      viewWidth: this.viewWidth,
      viewHeight: this.viewHeight,
      pixelsPerPPQN,
      trackListWidth: TRACK_LIST_WIDTH,
    };
    this.fadeOverlay.render(
      ctx, visibleTracks, fadeConfig,
      this.selectionMgr.state.selectedClipIds,
    );
  }

  // ─── Event Handlers ─────────────────────────────────────────

  private addEventListeners(): void {
    const canvas = this.canvasEl;
    if (!canvas) return;

    canvas.addEventListener('mousedown', this.boundMouseDown);
    canvas.addEventListener('mousemove', this.boundMouseMove);
    canvas.addEventListener('mouseup', this.boundMouseUp);
    canvas.addEventListener('keydown', this.boundKeyDown);
    canvas.setAttribute('tabindex', '0'); // Enable keyboard events

    window.addEventListener('resize', this.boundResize);
  }

  private removeEventListeners(): void {
    const canvas = this.canvasEl;
    if (canvas) {
      canvas.removeEventListener('mousedown', this.boundMouseDown);
      canvas.removeEventListener('mousemove', this.boundMouseMove);
      canvas.removeEventListener('mouseup', this.boundMouseUp);
      canvas.removeEventListener('keydown', this.boundKeyDown);
    }

    window.removeEventListener('resize', this.boundResize);
  }

  private onMouseDown(e: MouseEvent): void {
    const rect = this.canvasEl?.getBoundingClientRect();
    if (!rect) return;

    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;

    const pixelsPerPPQN = this.dataSource.getPixelsPerPPQN() || DEFAULT_PIXELS_PER_PPQN;
    const gridPpqn = this.dataSource.getGridPpqn();
    const gridEnabled = this.dataSource.isGridSnapEnabled();
    const shiftHeld = e.shiftKey;
    const visibleTracks = this.dataSource.getVisibleTracks();

    // Check fade handle hit first (smaller target)
    const fadeHit = this.fadeOverlay.hitTestFadeHandle(
      visibleTracks, this.scrollX, this.scrollY, pixelsPerPPQN,
      TRACK_LIST_WIDTH, px, py,
    );

    if (fadeHit) {
      // Fade handle drag — handled by FadeOverlay
      // For now, we just note the hit. Full fade drag TODO.
      return;
    }

    // Check track list interactions
    if (px < TRACK_LIST_WIDTH) {
      // Fold toggle
      const foldHit = this.trackList.hitTestFoldArrow(
        visibleTracks, this.scrollY, px, py,
      );
      if (foldHit) {
        // Toggle fold — would dispatch to model
        // For now, just return
        return;
      }

      // Mute / Solo
      const muteHit = this.trackList.hitTestMute(
        visibleTracks, this.scrollY, px, py,
      );
      if (muteHit) return;

      const soloHit = this.trackList.hitTestSolo(
        visibleTracks, this.scrollY, px, py,
      );
      if (soloHit) return;

      return; // Click on track list background — no clip action
    }

    // Clip canvas interaction
    this.selectionMgr.onMouseDown(
      visibleTracks, this.scrollX, this.scrollY, pixelsPerPPQN,
      TRACK_LIST_WIDTH, gridEnabled, gridPpqn, shiftHeld, px, py,
    );
  }

  private onMouseMove(e: MouseEvent): void {
    const rect = this.canvasEl?.getBoundingClientRect();
    if (!rect) return;

    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;

    const pixelsPerPPQN = this.dataSource.getPixelsPerPPQN() || DEFAULT_PIXELS_PER_PPQN;
    const gridPpqn = this.dataSource.getGridPpqn();
    const gridEnabled = this.dataSource.isGridSnapEnabled();
    const visibleTracks = this.dataSource.getVisibleTracks();

    // Fade handle hover detection (would set cursor)
    const fadeHit = this.fadeOverlay.hitTestFadeHandle(
      visibleTracks, this.scrollX, this.scrollY, pixelsPerPPQN,
      TRACK_LIST_WIDTH, px, py,
    );

    if (this.canvasEl) {
      this.canvasEl.style.cursor = fadeHit ? 'ew-resize' : 'default';
    }

    // Update hovered clip
    const hovered = this.clipCanvas.hitTestClip(
      visibleTracks, this.scrollX, this.scrollY, pixelsPerPPQN,
      TRACK_LIST_WIDTH, px, py,
    );
    this.selectionMgr.setHovered(hovered);

    // Drag
    if (this.selectionMgr.isDragging) {
      this.selectionMgr.onMouseMove(
        visibleTracks, this.scrollX, this.scrollY, pixelsPerPPQN,
        TRACK_LIST_WIDTH, gridEnabled, gridPpqn, px, py,
      );
    }
  }

  private onMouseUp(_e: MouseEvent): void {
    const pixelsPerPPQN = this.dataSource.getPixelsPerPPQN() || DEFAULT_PIXELS_PER_PPQN;
    const gridEnabled = this.dataSource.isGridSnapEnabled();
    const gridPpqn = this.dataSource.getGridPpqn();
    const visibleTracks = this.dataSource.getVisibleTracks();

    const result = this.selectionMgr.onMouseUp(
      visibleTracks, pixelsPerPPQN, gridEnabled, gridPpqn,
    );

    if (result && result.size > 0) {
      // Apply the drag result — in production this would dispatch
      // to the C++ model. For now, we emit a console log.
      console.log('[ArrangementView] Clip drag completed:', result);
    }
  }

  private onKeyDown(e: KeyboardEvent): void {
    if (e.key === 'Escape') {
      this.selectionMgr.cancelDrag();
      this.selectionMgr.clearSelection();
      return;
    }

    if (e.key === 'Delete' || e.key === 'Backspace') {
      // Delete selected clips — dispatch to model
      // For now, just clear selection
      this.selectionMgr.clearSelection();
      return;
    }
  }

  private onResize(): void {
    this.updateViewSize();
  }

  private updateViewSize(): void {
    if (this.canvasEl) {
      this.viewWidth = this.canvasEl.width;
      this.viewHeight = this.canvasEl.height;
    }
  }
}

// Re-export types and sub-components for external use
export { TimeRuler } from './TimeRuler.js';
export { TrackList } from './TrackList.js';
export { ClipCanvas } from './ClipCanvas.js';
export { SelectionManager } from './SelectionManager.js';
export { FadeOverlay } from './FadeOverlay.js';
export * from './types.js';
