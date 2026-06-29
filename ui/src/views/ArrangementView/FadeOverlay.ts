// ARIA DAW — FadeOverlay Component
//
// Renders fade-in/out overlays on clip edges and crossfade regions
// between overlapping clips on the same track.

import {
  TRACK_HEIGHT,
  RULER_HEIGHT,
  AnyClipData,
  AnyTrackData,
} from './types.js';

// ─── Default Theme ──────────────────────────────────────────────

const FADE_FILL = 'rgba(60, 180, 120, 0.35)';
const FADE_BORDER = 'rgba(60, 180, 120, 0.7)';
const CROSSFADE_FILL = 'rgba(180, 120, 60, 0.3)';
const CROSSFADE_BORDER = 'rgba(180, 120, 60, 0.6)';
const FADE_HANDLE_COLOR = '#3cb478';
const CROSSFADE_HANDLE_COLOR = '#b4783c';

// ─── FadeOverlay ─────────────────────────────────────────────────

export interface FadeOverlayConfig {
  scrollX: number;
  scrollY: number;
  viewWidth: number;
  viewHeight: number;
  pixelsPerPPQN: number;
  trackListWidth: number;
}

export class FadeOverlay {
  render(
    ctx: CanvasRenderingContext2D,
    tracks: AnyTrackData[],
    config: FadeOverlayConfig,
    selectedClipIds: Set<number>,
  ): void {
    const {
      scrollX, scrollY, viewHeight, pixelsPerPPQN, trackListWidth,
    } = config;

    const canvasLeft = trackListWidth;

    // Determine visible track range
    const firstTrack = Math.max(0, Math.floor(scrollY / TRACK_HEIGHT));
    const lastTrack = Math.min(
      tracks.length - 1,
      Math.ceil((scrollY + viewHeight) / TRACK_HEIGHT),
    );

    for (let i = firstTrack; i <= lastTrack; i++) {
      const track = tracks[i];
      if (!track) continue;

      // Render fades for each clip
      for (const clip of track.clips) {
        this.renderClipFades(
          ctx, clip, i, scrollX, scrollY,
          pixelsPerPPQN, canvasLeft, selectedClipIds,
        );
      }

      // Crossfades: find overlapping clip pairs on the same track
      this.renderCrossfades(
        ctx, track.clips, i, scrollX, scrollY,
        pixelsPerPPQN, canvasLeft,
      );
    }
  }

  private renderClipFades(
    ctx: CanvasRenderingContext2D,
    clip: AnyClipData,
    trackIndex: number,
    scrollX: number,
    scrollY: number,
    pixelsPerPPQN: number,
    canvasLeft: number,
    selectedClipIds: Set<number>,
  ): void {
    const trackY = RULER_HEIGHT + trackIndex * TRACK_HEIGHT - scrollY;
    const clipX = clip.start_ppqn * pixelsPerPPQN - scrollX + canvasLeft;
    const clipW = clip.length_ppqn * pixelsPerPPQN;
    const clipH = TRACK_HEIGHT - 2;
    const clipY = trackY + 1;

    // Off-screen
    if (clipX + clipW < canvasLeft || clipX > canvasLeft + 4000) return;

    const isSelected = selectedClipIds.has(clip.id.value);
    const handleSize = isSelected ? 6 : 0;

    // ─── Fade-in overlay ───────────────────────────────────────
    if (clip.fade_in_ppqn > 0) {
      const fadeW = clip.fade_in_ppqn * pixelsPerPPQN;

      ctx.fillStyle = FADE_FILL;
      ctx.beginPath();
      ctx.moveTo(clipX, clipY + clipH);
      ctx.lineTo(clipX, clipY);
      ctx.lineTo(clipX + fadeW, clipY + clipH);
      ctx.closePath();
      ctx.fill();

      if (isSelected) {
        ctx.strokeStyle = FADE_BORDER;
        ctx.lineWidth = 0.5;
        ctx.beginPath();
        ctx.moveTo(clipX, clipY);
        ctx.lineTo(clipX + fadeW, clipY + clipH);
        ctx.stroke();
      }

      // Fade handle dot
      if (isSelected) {
        ctx.fillStyle = FADE_HANDLE_COLOR;
        ctx.beginPath();
        ctx.arc(clipX + fadeW, clipY + clipH, handleSize, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    // ─── Fade-out overlay ──────────────────────────────────────
    if (clip.fade_out_ppqn > 0) {
      const fadeW = clip.fade_out_ppqn * pixelsPerPPQN;
      const fadeX = clipX + clipW - fadeW;

      ctx.fillStyle = FADE_FILL;
      ctx.beginPath();
      ctx.moveTo(clipX + clipW, clipY + clipH);
      ctx.lineTo(clipX + clipW, clipY);
      ctx.lineTo(fadeX, clipY + clipH);
      ctx.closePath();
      ctx.fill();

      if (isSelected) {
        ctx.strokeStyle = FADE_BORDER;
        ctx.lineWidth = 0.5;
        ctx.beginPath();
        ctx.moveTo(clipX + clipW, clipY);
        ctx.lineTo(fadeX, clipY + clipH);
        ctx.stroke();

        // Fade handle dot
        ctx.fillStyle = FADE_HANDLE_COLOR;
        ctx.beginPath();
        ctx.arc(fadeX, clipY + clipH, handleSize, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }

  /** Render crossfade between overlapping clips on the same track. */
  private renderCrossfades(
    ctx: CanvasRenderingContext2D,
    clips: AnyClipData[],
    trackIndex: number,
    scrollX: number,
    scrollY: number,
    pixelsPerPPQN: number,
    canvasLeft: number,
  ): void {
    // Sort clips by start position
    const sorted = [...clips].sort((a, b) => a.start_ppqn - b.start_ppqn);

    for (let i = 0; i < sorted.length - 1; i++) {
      const a = sorted[i];
      const b = sorted[i + 1];
      const aEnd = a.start_ppqn + a.length_ppqn;
      const bStart = b.start_ppqn;

      // Overlap detected
      if (bStart < aEnd) {
        const overlapPpqn = aEnd - bStart;
        if (overlapPpqn <= 0) continue;

        const trackY = RULER_HEIGHT + trackIndex * TRACK_HEIGHT - scrollY;
        const clipY = trackY + 1;
        const clipH = TRACK_HEIGHT - 2;

        const aX = a.start_ppqn * pixelsPerPPQN - scrollX + canvasLeft;
        const aW = a.length_ppqn * pixelsPerPPQN;

        const overlapX = aX + aW - overlapPpqn * pixelsPerPPQN;
        const overlapW = overlapPpqn * pixelsPerPPQN;

        // Crossfade region fill
        ctx.fillStyle = CROSSFADE_FILL;
        ctx.beginPath();
        ctx.moveTo(overlapX, clipY + clipH);
        ctx.lineTo(overlapX, clipY);
        ctx.lineTo(overlapX + overlapW, clipY + clipH);
        ctx.closePath();
        ctx.fill();

        // Crossfade border
        ctx.strokeStyle = CROSSFADE_BORDER;
        ctx.lineWidth = 0.5;
        ctx.beginPath();
        ctx.moveTo(overlapX, clipY + clipH);
        ctx.lineTo(overlapX + overlapW, clipY);
        ctx.stroke();

        // Crossfade handle at midpoint
        ctx.fillStyle = CROSSFADE_HANDLE_COLOR;
        ctx.beginPath();
        ctx.arc(overlapX + overlapW / 2, clipY + clipH, 4, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }

  /** Hit-test: return the clip and side ('in' or 'out') for a fade
   *  handle hit, or null. */
  hitTestFadeHandle(
    tracks: AnyTrackData[],
    scrollX: number,
    scrollY: number,
    pixelsPerPPQN: number,
    trackListWidth: number,
    px: number,
    py: number,
  ): { clip: AnyClipData; side: 'in' | 'out' } | null {
    const canvasLeft = trackListWidth;
    if (px < canvasLeft) return null;

    const trackIndex = Math.floor((py + scrollY - RULER_HEIGHT) / TRACK_HEIGHT);
    if (trackIndex < 0 || trackIndex >= tracks.length) return null;

    const track = tracks[trackIndex];
    const trackY = RULER_HEIGHT + trackIndex * TRACK_HEIGHT - scrollY;
    const clipY = trackY + 1;
    const clipH = TRACK_HEIGHT - 2;
    const hitRadius = 8;

    for (const clip of track.clips) {
      const clipX = clip.start_ppqn * pixelsPerPPQN - scrollX + canvasLeft;
      const clipW = clip.length_ppqn * pixelsPerPPQN;

      // Fade-in handle (bottom-left of fade triangle)
      if (clip.fade_in_ppqn > 0) {
        const fadeW = clip.fade_in_ppqn * pixelsPerPPQN;
        const handleX = clipX + fadeW;
        const handleY = clipY + clipH;
        const dx = px - handleX;
        const dy = py - handleY;
        if (dx * dx + dy * dy <= hitRadius * hitRadius) {
          return { clip, side: 'in' };
        }
      }

      // Fade-out handle (bottom-right of fade triangle)
      if (clip.fade_out_ppqn > 0) {
        const fadeW = clip.fade_out_ppqn * pixelsPerPPQN;
        const handleX = clipX + clipW - fadeW;
        const handleY = clipY + clipH;
        const dx = px - handleX;
        const dy = py - handleY;
        if (dx * dx + dy * dy <= hitRadius * hitRadius) {
          return { clip, side: 'out' };
        }
      }
    }

    return null;
  }

  /** Compute the desired fade PPQN from a drag position. */
  computeFadePPQN(
    clip: AnyClipData,
    side: 'in' | 'out',
    px: number,
    scrollX: number,
    pixelsPerPPQN: number,
    trackListWidth: number,
  ): number {
    const canvasLeft = trackListWidth;
    const clickPPQN = Math.max(0, Math.round(
      (px + scrollX - canvasLeft) / (pixelsPerPPQN || 1),
    ));

    if (side === 'in') {
      // Fade-in: measured from clip start
      return Math.min(clickPPQN - clip.start_ppqn, clip.length_ppqn);
    } else {
      // Fade-out: measured from clip end
      return Math.min(clip.start_ppqn + clip.length_ppqn - clickPPQN, clip.length_ppqn);
    }
  }
}
