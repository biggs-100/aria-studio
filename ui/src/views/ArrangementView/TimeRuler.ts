// ARIA DAW — TimeRuler Component
//
// Renders the horizontal time-axis: bar numbers, beat markers,
// sixteenth grid lines, and the playhead cursor.

import {
  BEAT_PPQN,
  BAR_PPQN,
  SIXTEENTH_PPQN,
  RULER_HEIGHT,
  GridLevel,
  getGridLevel,
} from './types.js';

// ─── Default Theme ──────────────────────────────────────────────

const RULER_BG = '#1a1a2e';
const GRID_LINE_STRONG = '#3a3a5c';
const GRID_LINE_WEAK = '#2a2a4a';
const BAR_NUMBER_TEXT = '#8a8aaa';
const PLAYHEAD_COLOR = '#ff4444';
const BEAT_MARKER_FILL = '#4a4a6a';
const SIXTEENTH_MARKER_FILL = '#2e2e4e';

// ─── TimeRuler ──────────────────────────────────────────────────

export interface TimeRulerConfig {
  /** Visible scroll offset (pixels) */
  scrollX: number;
  /** Visible viewport width (pixels) */
  viewWidth: number;
  /** Pixels per PPQN at current zoom */
  pixelsPerPPQN: number;
  /** Track list column width (ruler starts after this) */
  trackListWidth: number;
}

export class TimeRuler {
  render(
    ctx: CanvasRenderingContext2D,
    config: TimeRulerConfig,
    playheadPPQN: number,
  ): void {
    const { scrollX, viewWidth, pixelsPerPPQN, trackListWidth } = config;
    const rulerLeft = trackListWidth;
    const rulerWidth = viewWidth - trackListWidth;
    const rulerTop = 0;

    // Background
    ctx.fillStyle = RULER_BG;
    ctx.fillRect(rulerLeft, rulerTop, rulerWidth, RULER_HEIGHT);

    // Bottom border
    ctx.strokeStyle = GRID_LINE_STRONG;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(rulerLeft, RULER_HEIGHT - 0.5);
    ctx.lineTo(viewWidth, RULER_HEIGHT - 0.5);
    ctx.stroke();

    // Determine visible PPQN range
    const startPixel = rulerLeft;
    const endPixel = viewWidth;
    const startPPQN = Math.max(0, Math.floor((startPixel + scrollX) / pixelsPerPPQN));
    const endPPQN = Math.ceil((endPixel + scrollX) / pixelsPerPPQN);

    // Resolve grid level
    const gridLevel = getGridLevel(pixelsPerPPQN);

    // Draw grid lines from left to right
    this.drawGridLines(
      ctx, gridLevel, pixelsPerPPQN,
      startPPQN, endPPQN,
      rulerLeft, scrollX, rulerWidth,
    );

    // Draw playhead
    this.drawPlayhead(ctx, playheadPPQN, pixelsPerPPQN, rulerLeft, scrollX);
  }

  private drawGridLines(
    ctx: CanvasRenderingContext2D,
    level: GridLevel,
    pixelsPerPPQN: number,
    startPPQN: number,
    endPPQN: number,
    rulerLeft: number,
    scrollX: number,
    rulerWidth: number,
  ): void {
    const stepPPQN = level === GridLevel.Sixteenths
      ? SIXTEENTH_PPQN
      : level === GridLevel.Beats
        ? BEAT_PPQN
        : BAR_PPQN;

    // Align start to step boundary
    const aligned = Math.floor(startPPQN / stepPPQN) * stepPPQN;

    for (let ppqn = aligned; ppqn <= endPPQN; ppqn += stepPPQN) {
      const x = ppqn * pixelsPerPPQN - scrollX + rulerLeft;

      if (x < rulerLeft || x > rulerLeft + rulerWidth) continue;

      if (level === GridLevel.Bars) {
        // Strong line for bars
        ctx.strokeStyle = GRID_LINE_STRONG;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(x, RULER_HEIGHT);
        ctx.lineTo(x, RULER_HEIGHT + 8);
        ctx.stroke();

        // Bar number
        const barNum = Math.floor(ppqn / BAR_PPQN) + 1;
        ctx.fillStyle = BAR_NUMBER_TEXT;
        ctx.font = '11px monospace';
        ctx.textBaseline = 'top';
        ctx.textAlign = 'center';
        ctx.fillText(String(barNum), x, 4);
      } else if (level === GridLevel.Beats) {
        // Medium line for beats
        ctx.strokeStyle = GRID_LINE_WEAK;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(x, RULER_HEIGHT);
        ctx.lineTo(x, RULER_HEIGHT + 5);
        ctx.stroke();

        ctx.fillStyle = BEAT_MARKER_FILL;
        ctx.beginPath();
        ctx.arc(x, RULER_HEIGHT + 10, 2, 0, Math.PI * 2);
        ctx.fill();
      } else {
        // Weak line for sixteenths
        ctx.strokeStyle = GRID_LINE_WEAK;
        ctx.lineWidth = 0.5;
        ctx.beginPath();
        ctx.moveTo(x, RULER_HEIGHT);
        ctx.lineTo(x, RULER_HEIGHT + 3);
        ctx.stroke();

        ctx.fillStyle = SIXTEENTH_MARKER_FILL;
        ctx.beginPath();
        ctx.arc(x, RULER_HEIGHT + 10, 1.5, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }

  /** Draw the playhead cursor as a vertical line. */
  private drawPlayhead(
    ctx: CanvasRenderingContext2D,
    playheadPPQN: number,
    pixelsPerPPQN: number,
    rulerLeft: number,
    scrollX: number,
  ): void {
    const x = playheadPPQN * pixelsPerPPQN - scrollX + rulerLeft;

    ctx.strokeStyle = PLAYHEAD_COLOR;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, RULER_HEIGHT);
    ctx.stroke();
  }
}
