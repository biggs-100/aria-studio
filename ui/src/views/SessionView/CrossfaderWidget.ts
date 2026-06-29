// ARIA DAW — CrossfaderWidget Component
//
// Renders a horizontal crossfader slider with A/B assignment indicator.
// Supports mouse drag interaction to change the crossfader position.
// Value range: -1.0 (full A) to +1.0 (full B).

import {
  CROSSFADER_HEIGHT,
  CrossfaderData,
} from './types.js';

// ─── Default Theme ──────────────────────────────────────────────

const BG_COLOR = '#16162b';
const TRACK_BG = '#1e1e38';
const TRACK_FILL_A = '#3a6a55';
const TRACK_FILL_B = '#3a556a';
const TRACK_FILL_CENTER = '#2a2a4a';
const HANDLE_COLOR = '#88aacc';
const HANDLE_BORDER = '#aaccee';
const HANDLE_SHADOW = 'rgba(0,0,0,0.3)';
const LABEL_TEXT = '#8888aa';
const LABEL_A = '#55aa77';
const LABEL_B = '#5577aa';
const CURVE_LABEL = '#666688';

// ─── Layout Constants ───────────────────────────────────────────

const HANDLE_RADIUS = 10;
const TRACK_HEIGHT = 6;
const TRACK_WIDTH_PCT = 0.7; // 70% of available widget width
const LABEL_PADDING = 8;
const BORDER_COLOR = '#2a2a3f';

// ─── CrossfaderWidget ───────────────────────────────────────────

export interface CrossfaderWidgetConfig {
  /** Full viewport width (for centering the widget). */
  viewWidth: number;
}

export class CrossfaderWidget {
  /** Whether the user is currently dragging. */
  private _isDragging = false;

  /** Cached handle x position for hit-test during drag. */
  private _handleX = 0;

  // ─── Public API ──────────────────────────────────────────────

  get isDragging(): boolean {
    return this._isDragging;
  }

  render(
    ctx: CanvasRenderingContext2D,
    crossfader: CrossfaderData,
    config: CrossfaderWidgetConfig,
  ): void {
    const { viewWidth } = config;
    const widgetWidth = viewWidth;
    const widgetY = 0;

    // Background
    ctx.fillStyle = BG_COLOR;
    ctx.fillRect(0, widgetY, widgetWidth, CROSSFADER_HEIGHT);

    // Top border
    ctx.strokeStyle = BORDER_COLOR;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, CROSSFADER_HEIGHT - 0.5);
    ctx.lineTo(widgetWidth, CROSSFADER_HEIGHT - 0.5);
    ctx.stroke();

    // Track area dimensions
    const trackLeft = widgetWidth * (1 - TRACK_WIDTH_PCT) / 2;
    const trackWidth = widgetWidth * TRACK_WIDTH_PCT;
    const trackCenterY = CROSSFADER_HEIGHT / 2;
    const trackTop = trackCenterY - TRACK_HEIGHT / 2;

    // ── Background track ──
    ctx.fillStyle = TRACK_BG;
    ctx.beginPath();
    this.roundTrackRect(ctx, trackLeft, trackTop, trackWidth, TRACK_HEIGHT, 3);
    ctx.fill();

    // ── A side fill (left of handle) ──
    const handleX = this.handlePosition(crossfader.position, trackLeft, trackWidth);
    this._handleX = handleX;

    if (handleX > trackLeft) {
      ctx.fillStyle = TRACK_FILL_A;
      ctx.beginPath();
      this.roundTrackRect(ctx, trackLeft, trackTop, handleX - trackLeft, TRACK_HEIGHT, 0);
      ctx.fill();
    }

    // ── B side fill (right of handle) ──
    const rightEnd = trackLeft + trackWidth;
    if (handleX < rightEnd) {
      ctx.fillStyle = TRACK_FILL_B;
      ctx.beginPath();
      this.roundTrackRect(ctx, handleX, trackTop, rightEnd - handleX, TRACK_HEIGHT, 0);
      ctx.fill();
    }

    // ── Center marker ──
    const centerX = trackLeft + trackWidth / 2;
    ctx.strokeStyle = TRACK_FILL_CENTER;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(centerX, trackTop + 1);
    ctx.lineTo(centerX, trackTop + TRACK_HEIGHT - 1);
    ctx.stroke();

    // ── Handle shadow ──
    ctx.fillStyle = HANDLE_SHADOW;
    ctx.beginPath();
    ctx.arc(handleX + 1, trackCenterY + 1, HANDLE_RADIUS, 0, Math.PI * 2);
    ctx.fill();

    // ── Handle ──
    ctx.fillStyle = HANDLE_COLOR;
    ctx.beginPath();
    ctx.arc(handleX, trackCenterY, HANDLE_RADIUS, 0, Math.PI * 2);
    ctx.fill();

    ctx.strokeStyle = HANDLE_BORDER;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(handleX, trackCenterY, HANDLE_RADIUS, 0, Math.PI * 2);
    ctx.stroke();

    // ── Labels ──
    ctx.font = '10px monospace';
    ctx.textBaseline = 'middle';

    // "A" label (left)
    ctx.fillStyle = LABEL_A;
    ctx.textAlign = 'left';
    ctx.fillText('A', LABEL_PADDING, trackCenterY);

    // "B" label (right)
    ctx.fillStyle = LABEL_B;
    ctx.textAlign = 'right';
    ctx.fillText('B', widgetWidth - LABEL_PADDING, trackCenterY);

    // Position value (centered below track)
    ctx.fillStyle = LABEL_TEXT;
    ctx.textAlign = 'center';
    const posLabel = crossfader.position.toFixed(2);
    ctx.fillText(posLabel, widgetWidth / 2, CROSSFADER_HEIGHT - 10);

    // Curve label
    ctx.fillStyle = CURVE_LABEL;
    ctx.textAlign = 'right';
    ctx.font = '9px monospace';
    ctx.fillText(crossfader.curve, widgetWidth - LABEL_PADDING, 10);
  }

  /**
   * Handle a mouse down event. Returns true if the handle was hit.
   */
  onMouseDown(px: number, py: number, config: CrossfaderWidgetConfig): boolean {
    const { viewWidth } = config;
    const trackLeft = viewWidth * (1 - TRACK_WIDTH_PCT) / 2;
    const trackWidth = viewWidth * TRACK_WIDTH_PCT;
    const handleX = this._handleX;
    const trackCenterY = CROSSFADER_HEIGHT / 2;

    // Check if the click is near the handle
    const dx = px - handleX;
    const dy = py - trackCenterY;
    const hitRadius = HANDLE_RADIUS + 6;

    if (dx * dx + dy * dy <= hitRadius * hitRadius) {
      this._isDragging = true;
      return true;
    }

    // Also allow clicking anywhere on the track
    if (py >= 0 && py <= CROSSFADER_HEIGHT) {
      if (px >= trackLeft && px <= trackLeft + trackWidth) {
        this._isDragging = true;
        return true;
      }
    }

    return false;
  }

  /**
   * Handle a mouse move event during drag. Returns the new position, or
   * null if not dragging.
   */
  onMouseMove(
    px: number,
    config: CrossfaderWidgetConfig,
  ): number | null {
    if (!this._isDragging) return null;

    const { viewWidth } = config;
    const trackLeft = viewWidth * (1 - TRACK_WIDTH_PCT) / 2;
    const trackWidth = viewWidth * TRACK_WIDTH_PCT;

    // Map pixel position to normalized [-1, +1]
    const t = (px - trackLeft) / trackWidth;
    const clamped = Math.max(0, Math.min(1, t));
    const position = clamped * 2 - 1; // [0,1] → [-1,+1]

    return position;
  }

  /**
   * Handle a mouse up event. Ends drag and returns the final position.
   */
  onMouseUp(): void {
    this._isDragging = false;
  }

  // ─── Private Helpers ─────────────────────────────────────────

  /**
   * Draw a rounded rectangle for the track background.
   */
  private roundTrackRect(
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

  /**
   * Convert a crossfader position (-1..+1) to a pixel x-coordinate
   * within the track area.
   */
  private handlePosition(
    position: number,
    trackLeft: number,
    trackWidth: number,
  ): number {
    const t = (position + 1) / 2; // [-1,+1] → [0,1]
    return trackLeft + t * trackWidth;
  }
}
