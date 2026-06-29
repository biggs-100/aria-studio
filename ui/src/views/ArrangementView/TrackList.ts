// ARIA DAW — TrackList Component
//
// Renders the left track-name column with fold controls, mute/solo
// buttons, and track type indicators. Respects GroupTrack.folded
// to hide children.

import {
  TRACK_HEIGHT,
  TRACK_LIST_WIDTH,
  AnyTrackData,
  GroupTrackData,
  isGroupTrack,
  TrackID,
} from './types.js';

// ─── Default Theme ──────────────────────────────────────────────

const BG_COLOR = '#16162b';
const TEXT_COLOR = '#c0c0d0';
const MUTED_TEXT = '#555566';
const ALT_ROW = '#1a1a30';
const BORDER_COLOR = '#2a2a3f';
const FOLD_COLOR = '#6a6aaa';
const FOLD_ACTIVE = '#8a8acc';
const MUTE_ON = '#cc3333';
const MUTE_OFF = '#3a3a5a';
const SOLO_ON = '#ddcc00';
const SOLO_OFF = '#3a3a5a';
const TYPE_BADGE_BG: Record<string, string> = {
  Audio: '#2a553a',
  MIDI: '#2a3a55',
  Group: '#553a2a',
  VCA: '#4a2a55',
  Return: '#2a5550',
  Master: '#552a2a',
};

// ─── TrackList ──────────────────────────────────────────────────

export interface TrackListConfig {
  /** Vertical scroll offset (pixels) */
  scrollY: number;
  /** Viewport height (pixels) */
  viewHeight: number;
  /** Whether to show empty tracks */
  showEmpty: boolean;
}

export class TrackList {
  render(
    ctx: CanvasRenderingContext2D,
    tracks: AnyTrackData[],
    config: TrackListConfig,
  ): void {
    const { scrollY, viewHeight } = config;

    // Background
    ctx.fillStyle = BG_COLOR;
    ctx.fillRect(0, 0, TRACK_LIST_WIDTH, viewHeight);

    // Right border
    ctx.strokeStyle = BORDER_COLOR;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(TRACK_LIST_WIDTH - 0.5, 0);
    ctx.lineTo(TRACK_LIST_WIDTH - 0.5, viewHeight);
    ctx.stroke();

    // Render each visible track row
    const firstVisible = Math.max(0, Math.floor(scrollY / TRACK_HEIGHT));
    const lastVisible = Math.min(
      tracks.length - 1,
      Math.ceil((scrollY + viewHeight) / TRACK_HEIGHT),
    );

    for (let i = firstVisible; i <= lastVisible; i++) {
      const track = tracks[i];
      if (!track) continue;

      const y = i * TRACK_HEIGHT - scrollY;

      // Alternating row background
      if (i % 2 === 1) {
        ctx.fillStyle = ALT_ROW;
        ctx.fillRect(0, y, TRACK_LIST_WIDTH, TRACK_HEIGHT);
      }

      // Bottom border
      ctx.strokeStyle = BORDER_COLOR;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, y + TRACK_HEIGHT - 0.5);
      ctx.lineTo(TRACK_LIST_WIDTH, y + TRACK_HEIGHT - 0.5);
      ctx.stroke();

      // Fold control (GroupTrack only)
      if (isGroupTrack(track)) {
        this.drawFoldArrow(ctx, track, y);
      }

      // Type badge
      this.drawTypeBadge(ctx, track, y);

      // Track name
      const nameX = isGroupTrack(track) ? 32 : 14;
      ctx.fillStyle = track.muted ? MUTED_TEXT : TEXT_COLOR;
      ctx.font = '13px monospace';
      ctx.textBaseline = 'middle';
      ctx.textAlign = 'left';
      ctx.fillText(track.name, nameX, y + TRACK_HEIGHT / 2);

      // Mute button
      this.drawMuteButton(ctx, track, y);

      // Solo button
      this.drawSoloButton(ctx, track, y);
    }
  }

  private drawFoldArrow(
    ctx: CanvasRenderingContext2D,
    track: GroupTrackData,
    y: number,
  ): void {
    const cx = 12;
    const cy = y + TRACK_HEIGHT / 2;

    ctx.fillStyle = track.folded ? FOLD_ACTIVE : FOLD_COLOR;

    if (track.folded) {
      // Right-pointing triangle (collapsed)
      ctx.beginPath();
      ctx.moveTo(cx - 3, cy - 4);
      ctx.lineTo(cx + 4, cy);
      ctx.lineTo(cx - 3, cy + 4);
      ctx.closePath();
    } else {
      // Down-pointing triangle (expanded)
      ctx.beginPath();
      ctx.moveTo(cx - 4, cy - 3);
      ctx.lineTo(cx, cy + 4);
      ctx.lineTo(cx + 4, cy - 3);
      ctx.closePath();
    }
    ctx.fill();
  }

  private drawTypeBadge(
    ctx: CanvasRenderingContext2D,
    track: AnyTrackData,
    y: number,
  ): void {
    const label = track.type;
    const bg = TYPE_BADGE_BG[label] ?? '#333';

    ctx.fillStyle = bg;
    ctx.font = '9px monospace';
    ctx.textBaseline = 'bottom';
    ctx.textAlign = 'left';

    const x = isGroupTrack(track) ? 32 : 14;
    ctx.fillText(label.toUpperCase(), x, y + TRACK_HEIGHT - 2);
  }

  private drawMuteButton(
    ctx: CanvasRenderingContext2D,
    track: AnyTrackData,
    y: number,
  ): void {
    const x = TRACK_LIST_WIDTH - 50;
    const cy = y + TRACK_HEIGHT / 2;
    const r = 7;

    ctx.fillStyle = track.muted ? MUTE_ON : MUTE_OFF;
    ctx.beginPath();
    ctx.arc(x, cy, r, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#ddd';
    ctx.font = '7px monospace';
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'center';
    ctx.fillText('M', x, cy);
  }

  private drawSoloButton(
    ctx: CanvasRenderingContext2D,
    track: AnyTrackData,
    y: number,
  ): void {
    const x = TRACK_LIST_WIDTH - 28;
    const cy = y + TRACK_HEIGHT / 2;
    const r = 7;

    ctx.fillStyle = track.soloed ? SOLO_ON : SOLO_OFF;
    ctx.beginPath();
    ctx.arc(x, cy, r, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#222';
    ctx.font = '7px monospace';
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'center';
    ctx.fillText('S', x, cy);
  }

  /** Hit-test the fold arrow region. Returns true if (px, py) is on the
   *  fold arrow for the track at the given row index. */
  hitTestFoldArrow(
    tracks: AnyTrackData[],
    scrollY: number,
    px: number,
    py: number,
  ): TrackID | null {
    const rowIndex = Math.floor((py + scrollY) / TRACK_HEIGHT);
    const track = tracks[rowIndex];
    if (!track || !isGroupTrack(track)) return null;

    const y = rowIndex * TRACK_HEIGHT - scrollY;
    const cx = 12;
    const cy = y + TRACK_HEIGHT / 2;

    // 12x12 hit region around the arrow
    const dx = Math.abs(px - cx);
    const dy = Math.abs(py - cy);
    if (dx <= 8 && dy <= 8) {
      return track.id;
    }
    return null;
  }

  /** Hit-test the mute button region. */
  hitTestMute(
    tracks: AnyTrackData[],
    scrollY: number,
    px: number,
    py: number,
  ): TrackID | null {
    return this.hitTestCircleButton(tracks, scrollY, px, py, TRACK_LIST_WIDTH - 50, 7);
  }

  /** Hit-test the solo button region. */
  hitTestSolo(
    tracks: AnyTrackData[],
    scrollY: number,
    px: number,
    py: number,
  ): TrackID | null {
    return this.hitTestCircleButton(tracks, scrollY, px, py, TRACK_LIST_WIDTH - 28, 7);
  }

  private hitTestCircleButton(
    tracks: AnyTrackData[],
    scrollY: number,
    px: number,
    py: number,
    cx: number,
    r: number,
  ): TrackID | null {
    const rowIndex = Math.floor((py + scrollY) / TRACK_HEIGHT);
    const track = tracks[rowIndex];
    if (!track) return null;

    const y = rowIndex * TRACK_HEIGHT - scrollY;
    const cy = y + TRACK_HEIGHT / 2;

    const dx = px - cx;
    const dy = py - cy;
    if (dx * dx + dy * dy <= r * r + 2) {
      return track.id;
    }
    return null;
  }
}
