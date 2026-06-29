// ARIA DAW — SceneStrip Component
//
// Renders the left column with scene names and launch buttons.
// Clicking a scene name launches all clips in that scene.
// A green indicator shows when a scene has a playing clip.

import {
  SLOT_HEIGHT,
  COLUMN_HEADER_HEIGHT,
  SCENE_STRIP_WIDTH,
  SceneData,
  SceneSummary,
  SessionDataSource,
} from './types.js';

// ─── Default Theme ──────────────────────────────────────────────

const BG_COLOR = '#16162b';
const ALT_ROW = '#1a1a30';
const BORDER_COLOR = '#2a2a3f';
const LAUNCH_BTN_BG = '#2a2a50';
const SCENE_PLAYING_INDICATOR = '#44cc66';
const SCENE_TRIGGERED_INDICATOR = '#ccaa33';
const SCENE_NAME_COLOR = '#e0e0f0';
const ADD_SCENE_BTN_COLOR = '#4a6a8a';

// ─── SceneStrip ─────────────────────────────────────────────────

export interface SceneStripConfig {
  scrollY: number;
  viewHeight: number;
  /** Whether there are more scenes than fit in the viewport. */
  hasMoreScenes: boolean;
}

export class SceneStrip {
  // ─── Public API ──────────────────────────────────────────────

  render(
    ctx: CanvasRenderingContext2D,
    scenes: SceneData[],
    dataSource: SessionDataSource,
    config: SceneStripConfig,
  ): void {
    const { scrollY, viewHeight } = config;

    // Background
    ctx.fillStyle = BG_COLOR;
    ctx.fillRect(0, 0, SCENE_STRIP_WIDTH, viewHeight);

    // Right border
    ctx.strokeStyle = BORDER_COLOR;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(SCENE_STRIP_WIDTH - 0.5, 0);
    ctx.lineTo(SCENE_STRIP_WIDTH - 0.5, viewHeight);
    ctx.stroke();

    // Vertical scroll range
    const firstVisible = Math.max(0, Math.floor(scrollY / SLOT_HEIGHT));
    const lastVisible = Math.min(
      scenes.length,
      Math.ceil((scrollY + viewHeight) / SLOT_HEIGHT),
    );

    for (let i = firstVisible; i < lastVisible; i++) {
      const scene = scenes[i];
      if (!scene) continue;

      const y = COLUMN_HEADER_HEIGHT + i * SLOT_HEIGHT - scrollY;

      // Alternating row background
      if (i % 2 === 0) {
        ctx.fillStyle = BG_COLOR;
      } else {
        ctx.fillStyle = ALT_ROW;
      }
      ctx.fillRect(0, y, SCENE_STRIP_WIDTH, SLOT_HEIGHT);

      // Bottom border
      ctx.strokeStyle = BORDER_COLOR;
      ctx.lineWidth = 0.5;
      ctx.beginPath();
      ctx.moveTo(0, y + SLOT_HEIGHT - 0.5);
      ctx.lineTo(SCENE_STRIP_WIDTH, y + SLOT_HEIGHT - 0.5);
      ctx.stroke();

      // Scene playing indicator
      const summary = dataSource.getSceneSummary(scene.id);
      this.drawPlayingIndicator(ctx, summary, y);

      // Scene name
      this.drawSceneName(ctx, scene, y);

      // Launch button
      this.drawLaunchButton(ctx, y, summary);
    }

    // ── Add scene button (at bottom of the strip) ──
    this.drawAddSceneButton(ctx, scenes, config);
  }

  /**
   * Hit-test: return the scene ID if a click hit a scene launch button
   * or scene name area.
   */
  hitTestScene(
    px: number,
    py: number,
    scenes: SceneData[],
    scrollY: number,
  ): SceneData | null {
    if (px < 0 || px > SCENE_STRIP_WIDTH) return null;
    if (py < COLUMN_HEADER_HEIGHT) return null;

    const rowIndex = Math.floor((py + scrollY - COLUMN_HEADER_HEIGHT) / SLOT_HEIGHT);
    if (rowIndex < 0 || rowIndex >= scenes.length) return null;

    return scenes[rowIndex];
  }

  /**
   * Hit-test: return the scene ID if a click hit the launch button
   * (the small circle on the left).
   */
  hitTestLaunchButton(
    px: number,
    py: number,
    scenes: SceneData[],
    scrollY: number,
  ): SceneData | null {
    const scene = this.hitTestScene(px, py, scenes, scrollY);
    if (!scene) return null;

    const rowIndex = scenes.indexOf(scene);
    const y = COLUMN_HEADER_HEIGHT + rowIndex * SLOT_HEIGHT - scrollY;
    const btnCx = 16;
    const btnCy = y + SLOT_HEIGHT / 2;
    const btnR = 7;

    const dx = px - btnCx;
    const dy = py - btnCy;
    if (dx * dx + dy * dy <= btnR * btnR + 4) {
      return scene;
    }
    return null;
  }

  /**
   * Hit-test: return true if the click was on the "+" add scene button.
   */
  hitTestAddButton(
    px: number,
    py: number,
    _scrollY: number,
    viewHeight: number,
  ): boolean {
    if (px < 0 || px > SCENE_STRIP_WIDTH) return false;
    const btnY = viewHeight - 40;
    return py >= btnY && py <= btnY + 32;
  }

  // ─── Private Rendering Methods ───────────────────────────────

  private drawPlayingIndicator(
    ctx: CanvasRenderingContext2D,
    summary: SceneSummary,
    y: number,
  ): void {
    const cx = 16;
    const cy = y + SLOT_HEIGHT / 2;

    if (summary.is_playing) {
      ctx.fillStyle = SCENE_PLAYING_INDICATOR;
    } else if (summary.any_triggered) {
      ctx.fillStyle = SCENE_TRIGGERED_INDICATOR;
    } else {
      ctx.fillStyle = LAUNCH_BTN_BG;
    }

    ctx.beginPath();
    ctx.arc(cx, cy, 7, 0, Math.PI * 2);
    ctx.fill();
  }

  private drawSceneName(
    ctx: CanvasRenderingContext2D,
    scene: SceneData,
    y: number,
  ): void {
    ctx.fillStyle = SCENE_NAME_COLOR;
    ctx.font = '13px monospace';
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'left';

    const nameX = 32;
    const maxChars = Math.max(1, Math.floor((SCENE_STRIP_WIDTH - nameX - 10) / 8));
    const displayName = scene.name.length > 0
      ? scene.name.slice(0, maxChars)
      : `Scene ${scene.order_index + 1}`;

    ctx.fillText(displayName, nameX, y + SLOT_HEIGHT / 2);
  }

  private drawLaunchButton(
    ctx: CanvasRenderingContext2D,
    y: number,
    _summary: SceneSummary,
  ): void {
    const cx = 16;
    const cy = y + SLOT_HEIGHT / 2;
    const r = 7;

    // Outer ring
    ctx.strokeStyle = '#4a4a7a';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.stroke();
  }

  private drawAddSceneButton(
    ctx: CanvasRenderingContext2D,
    _scenes: SceneData[],
    config: SceneStripConfig,
  ): void {
    const { viewHeight, scrollY: _scrollY } = config;
    const btnY = viewHeight - 40;
    const btnH = 32;

    // Button background
    ctx.fillStyle = '#1e1e38';
    ctx.fillRect(0, btnY, SCENE_STRIP_WIDTH, btnH);

    // Top border
    ctx.strokeStyle = BORDER_COLOR;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, btnY - 0.5);
    ctx.lineTo(SCENE_STRIP_WIDTH, btnY - 0.5);
    ctx.stroke();

    // "+" icon
    ctx.fillStyle = ADD_SCENE_BTN_COLOR;
    ctx.font = '18px monospace';
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'center';
    ctx.fillText('+', 24, btnY + btnH / 2);

    // Label
    ctx.fillStyle = '#6a8aaa';
    ctx.font = '11px monospace';
    ctx.textAlign = 'left';
    ctx.fillText('Add Scene', 36, btnY + btnH / 2);
  }
}
