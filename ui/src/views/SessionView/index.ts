// ARIA DAW — SessionView
//
// Main session view: coordinates ClipGrid, SceneStrip, and
// CrossfaderWidget. Owns the requestAnimationFrame render loop
// and dispatches mouse events to the appropriate sub-component.
//
// Mirrors the ArrangementView pattern (ArrangementView/index.ts).

import {
  COLUMN_HEADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  CROSSFADER_HEIGHT,
  MIN_SLOT_WIDTH,
  MAX_SLOT_WIDTH,
  SessionDataSource,
  SessionViewport,
} from './types.js';

import { ClipGrid, ClipGridConfig } from './ClipGrid.js';
import { SceneStrip, SceneStripConfig } from './SceneStrip.js';
import { CrossfaderWidget, CrossfaderWidgetConfig } from './CrossfaderWidget.js';

import type { View } from '../index.js';

// ─── SessionView ─────────────────────────────────────────────────

export class SessionView implements View {
  readonly name = 'Session';

  // Sub-components
  private clipGrid = new ClipGrid();
  private sceneStrip = new SceneStrip();
  private crossfaderWidget = new CrossfaderWidget();

  // Data source (provided by host app or C++ bridge)
  private dataSource: SessionDataSource;

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

  // Mouse event handlers (bound)
  private boundMouseDown: (e: MouseEvent) => void;
  private boundMouseMove: (e: MouseEvent) => void;
  private boundMouseUp: (e: MouseEvent) => void;
  private boundWheel: (e: WheelEvent) => void;
  private boundResize: () => void;

  constructor(dataSource: SessionDataSource) {
    this.dataSource = dataSource;

    // Bind event handlers
    this.boundMouseDown = this.onMouseDown.bind(this);
    this.boundMouseMove = this.onMouseMove.bind(this);
    this.boundMouseUp = this.onMouseUp.bind(this);
    this.boundWheel = this.onWheel.bind(this);
    this.boundResize = this.onResize.bind(this);
  }

  // ─── View Interface ─────────────────────────────────────────

  activate(): void {
    // Event listeners are attached in attach() when canvas is provided
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
      console.error('[SessionView] Could not get 2D context');
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
  setDataSource(ds: SessionDataSource): void {
    this.dataSource = ds;
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

    const tracks = this.dataSource.getTracks();
    const scenes = this.dataSource.getScenes();

    // Compute column width based on number of tracks
    const columnWidth = this.computeColumnWidth(tracks.length);

    // Clear entire canvas
    ctx.clearRect(0, 0, this.viewWidth, this.viewHeight);

    // ── 1. Crossfader Widget (top) ──
    const crossfaderConfig: CrossfaderWidgetConfig = {
      viewWidth: this.viewWidth,
    };
    const crossfaderData = this.dataSource.getCrossfader();
    this.crossfaderWidget.render(ctx, crossfaderData, crossfaderConfig);

    // ── 2. Scene Strip (left column) ──
    const sceneStripConfig: SceneStripConfig = {
      scrollY: this.scrollY,
      viewHeight: this.viewHeight,
      hasMoreScenes: false,
    };
    this.sceneStrip.render(ctx, scenes, this.dataSource, sceneStripConfig);

    // ── 3. Clip Grid (main area) ──
    const viewport: SessionViewport = {
      scrollX: this.scrollX,
      scrollY: this.scrollY,
      viewWidth: this.viewWidth,
      viewHeight: this.viewHeight,
    };
    const clipGridConfig: ClipGridConfig = {
      viewport,
      trackCount: tracks.length,
      columnWidth,
    };
    this.clipGrid.render(ctx, tracks, scenes, this.dataSource, clipGridConfig);
  }

  // ─── Event Handlers ─────────────────────────────────────────

  private addEventListeners(): void {
    const canvas = this.canvasEl;
    if (!canvas) return;

    canvas.addEventListener('mousedown', this.boundMouseDown);
    canvas.addEventListener('mousemove', this.boundMouseMove);
    canvas.addEventListener('mouseup', this.boundMouseUp);
    canvas.addEventListener('wheel', this.boundWheel, { passive: false });
    canvas.setAttribute('tabindex', '0');

    window.addEventListener('resize', this.boundResize);
  }

  private removeEventListeners(): void {
    const canvas = this.canvasEl;
    if (canvas) {
      canvas.removeEventListener('mousedown', this.boundMouseDown);
      canvas.removeEventListener('mousemove', this.boundMouseMove);
      canvas.removeEventListener('mouseup', this.boundMouseUp);
      canvas.removeEventListener('wheel', this.boundWheel);
    }

    window.removeEventListener('resize', this.boundResize);
  }

  private onMouseDown(e: MouseEvent): void {
    const rect = this.canvasEl?.getBoundingClientRect();
    if (!rect) return;

    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;

    // ── Check crossfader widget (top area) ──
    if (py < CROSSFADER_HEIGHT) {
      const hit = this.crossfaderWidget.onMouseDown(px, py, {
        viewWidth: this.viewWidth,
      });
      if (hit) {
        // Dispatch the click position as a manual drag
        const pos = this.crossfaderWidget.onMouseMove(px, {
          viewWidth: this.viewWidth,
        });
        if (pos !== null) {
          this.dataSource.setCrossfaderPosition(pos);
        }
      }
      return;
    }

    // ── Check scene strip (left column) ──
    if (px < SCENE_STRIP_WIDTH) {
      // Check add scene button
      if (this.sceneStrip.hitTestAddButton(px, py, this.scrollY, this.viewHeight)) {
        return; // Add button — would dispatch
      }

      // Check launch button hit
      const launchHit = this.sceneStrip.hitTestLaunchButton(
        px, py, this.dataSource.getScenes(), this.scrollY,
      );
      if (launchHit) {
        this.dataSource.launchScene(launchHit.id);
        return;
      }

      // Check scene name hit (launch scene)
      const sceneHit = this.sceneStrip.hitTestScene(
        px, py, this.dataSource.getScenes(), this.scrollY,
      );
      if (sceneHit) {
        this.dataSource.launchScene(sceneHit.id);
      }
      return;
    }

    // ── Check clip grid ──
    if (py >= COLUMN_HEADER_HEIGHT) {
      const columnWidth = this.computeColumnWidth(this.dataSource.getTracks().length);
      const hit = this.clipGrid.hitTestSlot(
        px, py, this.dataSource.getScenes(),
        this.scrollX, this.scrollY, columnWidth,
      );

      if (hit) {
        const tracks = this.dataSource.getTracks();
        const track = tracks[hit.trackIndex];
        if (track) {
          this.dataSource.launchClip(
            track.id,
            { value: hit.sceneId },
          );
        }
      }
    }
  }

  private onMouseMove(e: MouseEvent): void {
    // Handle crossfader drag
    if (this.crossfaderWidget.isDragging) {
      const rect = this.canvasEl?.getBoundingClientRect();
      if (!rect) return;

      const px = e.clientX - rect.left;
      const pos = this.crossfaderWidget.onMouseMove(px, {
        viewWidth: this.viewWidth,
      });
      if (pos !== null) {
        this.dataSource.setCrossfaderPosition(pos);
      }
    }
  }

  private onMouseUp(_e: MouseEvent): void {
    this.crossfaderWidget.onMouseUp();
  }

  private onWheel(e: WheelEvent): void {
    e.preventDefault();

    // Vertical scroll
    this.scrollY += e.deltaY;
    this.scrollY = Math.max(0, this.scrollY);

    // Horizontal scroll (SHIFT + wheel)
    if (e.shiftKey) {
      this.scrollX += e.deltaY;
      this.scrollX = Math.max(0, this.scrollX);
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

  // ─── Helpers ─────────────────────────────────────────────────

  /**
   * Compute column width based on the number of tracks.
   * Clamps between MIN_SLOT_WIDTH and MAX_SLOT_WIDTH.
   */
  private computeColumnWidth(trackCount: number): number {
    if (trackCount <= 0) return MIN_SLOT_WIDTH;

    const availableWidth = this.viewWidth - SCENE_STRIP_WIDTH;
    const autoWidth = Math.floor(availableWidth / trackCount);
    return Math.max(MIN_SLOT_WIDTH, Math.min(MAX_SLOT_WIDTH, autoWidth));
  }
}

// Re-export types and sub-components for external use
export { ClipGrid } from './ClipGrid.js';
export { SceneStrip } from './SceneStrip.js';
export { CrossfaderWidget } from './CrossfaderWidget.js';
export * from './types.js';
