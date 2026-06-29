// ARIA DAW — ClipGrid Component
//
// Renders the 2D session grid (tracks × scenes). Each cell shows the
// clip name and color. Implements virtual culling: only cells visible
// in the current viewport are rendered. Click on a cell triggers a
// clip launch via the data source.

import {
  SLOT_HEIGHT,
  COLUMN_HEADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  SceneData,
  ClipSlotData,
  SessionDataSource,
  SessionViewport,
} from './types.js';

import type { TrackData } from '../ArrangementView/types.js';

// ─── Default Theme ──────────────────────────────────────────────

const CELL_BORDER = '#2a2a3f';
const CELL_BG = '#1a1a2e';
const CELL_ALT_ROW = '#1e1e34';
const CLIP_FILL = '#2a5540';
const CLIP_PLAYING_FILL = '#3a9955';
const CLIP_TRIGGERED_FILL = '#997a33';
const CLIP_TEXT = '#c0d0e0';
const CLIP_NAME_SHADOW = 'rgba(0,0,0,0.4)';
const HEADER_BG = '#16162b';
const HEADER_TEXT = '#8888aa';
const HEADER_BORDER = '#32325a';
const SCENE_RUNNING_INDICATOR = '#44cc66';

// ─── ClipGrid ───────────────────────────────────────────────────

export interface ClipGridConfig {
  viewport: SessionViewport;
  /** Number of tracks = number of grid columns. */
  trackCount: number;
  /** Track column width in pixels. */
  columnWidth: number;
}

export class ClipGrid {
  // ─── Public API ──────────────────────────────────────────────

  render(
    ctx: CanvasRenderingContext2D,
    tracks: TrackData[],
    scenes: SceneData[],
    dataSource: SessionDataSource,
    config: ClipGridConfig,
  ): void {
    const { viewport, trackCount, columnWidth } = config;
    const { scrollX, scrollY, viewWidth, viewHeight } = viewport;

    const gridLeft = SCENE_STRIP_WIDTH;
    const gridWidth = trackCount * columnWidth;

    // ── Column header background ──
    ctx.fillStyle = HEADER_BG;
    ctx.fillRect(gridLeft, 0, gridWidth, COLUMN_HEADER_HEIGHT);

    // ── Draw column headers (track names) ──
    this.drawColumnHeaders(ctx, tracks, scrollX, scrollY, columnWidth);

    // ── Draw grid cells ──
    this.drawGridCells(
      ctx, tracks, scenes, dataSource,
      scrollX, scrollY, viewWidth, viewHeight,
      columnWidth,
    );

    // ── Grid cell borders (drawn after cells so they overlay) ──
    this.drawGridLines(
      ctx, scenes.length, trackCount,
      scrollX, scrollY, viewWidth, viewHeight,
      columnWidth,
    );
  }

  /**
   * Hit-test: return the track and scene IDs for a clip slot at a pixel
   * position, or null if no slot was hit.
   */
  hitTestSlot(
    px: number,
    py: number,
    scenes: SceneData[],
    scrollX: number,
    scrollY: number,
    columnWidth: number,
  ): { trackIndex: number; sceneId: number } | null {
    if (px < SCENE_STRIP_WIDTH) return null;
    if (py < COLUMN_HEADER_HEIGHT) return null;

    const col = Math.floor((px + scrollX - SCENE_STRIP_WIDTH) / columnWidth);
    const row = Math.floor((py + scrollY - COLUMN_HEADER_HEIGHT) / SLOT_HEIGHT);

    if (col < 0 || row < 0) return null;
    if (row >= scenes.length) return null;

    const scene = scenes[row];
    if (!scene) return null;

    return { trackIndex: col, sceneId: scene.id.value };
  }

  // ─── Private Rendering Methods ───────────────────────────────

  private drawColumnHeaders(
    ctx: CanvasRenderingContext2D,
    tracks: TrackData[],
    scrollX: number,
    _scrollY: number,
    columnWidth: number,
  ): void {
    const startCol = Math.max(0, Math.floor(scrollX / columnWidth));
    const endCol = Math.min(
      tracks.length,
      Math.ceil((scrollX + 2000) / columnWidth),
    );

    for (let col = startCol; col < endCol; col++) {
      const track = tracks[col];
      if (!track) continue;

      const x = col * columnWidth - scrollX + SCENE_STRIP_WIDTH;

      ctx.fillStyle = HEADER_TEXT;
      ctx.font = '11px monospace';
      ctx.textBaseline = 'middle';
      ctx.textAlign = 'center';

      // Truncate long track names
      const maxLen = Math.max(3, Math.floor(columnWidth / 7));
      const name = track.name.length > maxLen
        ? track.name.slice(0, maxLen - 1) + '…'
        : track.name;

      ctx.fillText(name, x + columnWidth / 2, COLUMN_HEADER_HEIGHT / 2);

      // Column header divider
      ctx.strokeStyle = HEADER_BORDER;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(x + columnWidth - 0.5, 0);
      ctx.lineTo(x + columnWidth - 0.5, COLUMN_HEADER_HEIGHT);
      ctx.stroke();
    }
  }

  private drawGridCells(
    ctx: CanvasRenderingContext2D,
    tracks: TrackData[],
    scenes: SceneData[],
    dataSource: SessionDataSource,
    scrollX: number,
    scrollY: number,
    viewWidth: number,
    viewHeight: number,
    columnWidth: number,
  ): void {
    // ── Virtual culling: only render rows/cols in the viewport ──
    const startRow = Math.max(0, Math.floor(scrollY / SLOT_HEIGHT));
    const endRow = Math.min(
      scenes.length,
      Math.ceil((scrollY + viewHeight - COLUMN_HEADER_HEIGHT) / SLOT_HEIGHT) + 1,
    );

    const startCol = Math.max(0, Math.floor(scrollX / columnWidth));
    const endCol = Math.min(
      tracks.length,
      Math.ceil((scrollX + viewWidth - SCENE_STRIP_WIDTH) / columnWidth) + 1,
    );

    for (let row = startRow; row < endRow; row++) {
      const scene = scenes[row];
      if (!scene) continue;

      const cellY = COLUMN_HEADER_HEIGHT + row * SLOT_HEIGHT - scrollY;

      for (let col = startCol; col < endCol; col++) {
        const track = tracks[col];
        if (!track) continue;

        const cellX = col * columnWidth - scrollX + SCENE_STRIP_WIDTH;

        // Background
        ctx.fillStyle = row % 2 === 0 ? CELL_BG : CELL_ALT_ROW;
        ctx.fillRect(cellX, cellY, columnWidth, SLOT_HEIGHT);

        // Query slot data from the data source
        const slot = dataSource.getSlot(track.id, scene.id);

        if (slot.has_clip) {
          this.renderClipSlot(
            ctx, slot, cellX, cellY, columnWidth,
          );
        }
      }
    }
  }

  private renderClipSlot(
    ctx: CanvasRenderingContext2D,
    slot: ClipSlotData,
    cellX: number,
    cellY: number,
    columnWidth: number,
  ): void {
    const padX = 2;
    const padY = 2;
    const clipW = columnWidth - padX * 2;
    const clipH = SLOT_HEIGHT - padY * 2;

    // Clip background
    let fillColor = CLIP_FILL;
    if (slot.state === 'Playing') {
      fillColor = CLIP_PLAYING_FILL;
    } else if (slot.state === 'Triggered') {
      fillColor = CLIP_TRIGGERED_FILL;
    }

    ctx.fillStyle = fillColor;
    ctx.beginPath();
    this.roundRect(ctx, cellX + padX, cellY + padY, clipW, clipH, 3);
    ctx.fill();

    // Clip border
    ctx.strokeStyle = CELL_BORDER;
    ctx.lineWidth = 1;
    ctx.beginPath();
    this.roundRect(ctx, cellX + padX, cellY + padY, clipW, clipH, 3);
    ctx.stroke();

    // Clip name
    ctx.fillStyle = CLIP_TEXT;
    ctx.font = '11px monospace';
    ctx.textBaseline = 'top';
    ctx.textAlign = 'left';

    const maxChars = Math.max(1, Math.floor(clipW / 7));
    const displayName = slot.clip_name.length > 0
      ? slot.clip_name.slice(0, maxChars)
      : 'Clip';

    // Text shadow for readability
    ctx.fillStyle = CLIP_NAME_SHADOW;
    ctx.fillText(displayName, cellX + padX + 5, cellY + padY + 3);
    ctx.fillStyle = CLIP_TEXT;
    ctx.fillText(displayName, cellX + padX + 4, cellY + padY + 2);

    // Playing state indicator (green dot)
    if (slot.state === 'Playing') {
      ctx.fillStyle = SCENE_RUNNING_INDICATOR;
      ctx.beginPath();
      ctx.arc(
        cellX + clipW - 8,
        cellY + padY + 8,
        3, 0, Math.PI * 2,
      );
      ctx.fill();
    }
  }

  private drawGridLines(
    ctx: CanvasRenderingContext2D,
    sceneCount: number,
    trackCount: number,
    scrollX: number,
    scrollY: number,
    viewWidth: number,
    viewHeight: number,
    columnWidth: number,
  ): void {
    ctx.strokeStyle = CELL_BORDER;
    ctx.lineWidth = 0.5;

    // ── Horizontal grid lines (scene row separators) ──
    const startRow = Math.max(0, Math.floor(scrollY / SLOT_HEIGHT));
    const endRow = Math.min(
      sceneCount,
      Math.ceil((scrollY + viewHeight) / SLOT_HEIGHT) + 1,
    );

    for (let row = startRow; row <= endRow; row++) {
      const y = COLUMN_HEADER_HEIGHT + row * SLOT_HEIGHT - scrollY;
      if (y < 0 || y > viewHeight) continue;

      ctx.beginPath();
      ctx.moveTo(SCENE_STRIP_WIDTH, y);
      ctx.lineTo(SCENE_STRIP_WIDTH + trackCount * columnWidth, y);
      ctx.stroke();
    }

    // ── Vertical grid lines (track column separators) ──
    const startCol = Math.max(0, Math.floor(scrollX / columnWidth));
    const endCol = Math.min(
      trackCount,
      Math.ceil((scrollX + viewWidth - SCENE_STRIP_WIDTH) / columnWidth) + 1,
    );

    for (let col = startCol; col <= endCol; col++) {
      const x = col * columnWidth - scrollX + SCENE_STRIP_WIDTH;
      if (x < SCENE_STRIP_WIDTH || x > viewWidth) continue;

      ctx.beginPath();
      ctx.moveTo(x, COLUMN_HEADER_HEIGHT);
      ctx.lineTo(x, viewHeight);
      ctx.stroke();
    }
  }

  // ─── Helpers ─────────────────────────────────────────────────

  private roundRect(
    ctx: CanvasRenderingContext2D,
    x: number, y: number, w: number, h: number, r: number,
  ): void {
    if (w < 0 || h < 0) return;
    if (w < 2 * r) r = w / 2;
    if (h < 2 * r) r = h / 2;

    ctx.moveTo(x + r, y);
    ctx.lineTo(x + w - r, y);
    ctx.quadraticCurveTo(x + w, y, x + w, y + r);
    ctx.lineTo(x + w, y + h - r);
    ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
    ctx.lineTo(x + r, y + h);
    ctx.quadraticCurveTo(x, y + h, x, y + h - r);
    ctx.lineTo(x, y + r);
    ctx.quadraticCurveTo(x, y, x + r, y);
  }
}
