// ARIA DAW — ClipCanvas Component
//
// Renders clip rectangles with waveform thumbnails, MIDI note density,
// and automation curve overlays on the main timeline area.

import {
  TRACK_HEIGHT,
  RULER_HEIGHT,
  PPQN,
  AnyClipData,
  AnyTrackData,
  AudioClipData,
  MidiClipData,
  AutomationClipData,
  isAudioClip,
  isMidiClip,
  isAutomationClip,
  isSessionCaptureClip,
  SelectionState,
} from './types.js';

// ─── Default Theme ──────────────────────────────────────────────

const BG_COLOR = '#12121e';
const GRID_BEAT = '#1e1e32';
const GRID_BAR = '#222238';
const CLIP_BORDER = '#4a6a8a';
const CLIP_SELECTED_BORDER = '#88bbee';
const CLIP_TEXT = '#c0d0e0';
const CLIP_AUDIO_FILL = '#2a5540';
const CLIP_MIDI_FILL = '#2a4055';
const CLIP_AUTO_FILL = '#553a2a';
const WAVEFORM_HIGH = '#4aaa6a';
const WAVEFORM_LOW = '#2a6a40';
const MUTED_OVERLAY = 'rgba(0,0,0,0.4)';
const SELECTION_OVERLAY = 'rgba(100,150,220,0.15)';

// ─── ClipCanvas ─────────────────────────────────────────────────

export interface ClipCanvasConfig {
  scrollX: number;
  scrollY: number;
  viewWidth: number;
  viewHeight: number;
  pixelsPerPPQN: number;
  trackListWidth: number;
}

export class ClipCanvas {
  render(
    ctx: CanvasRenderingContext2D,
    tracks: AnyTrackData[],
    config: ClipCanvasConfig,
    selection: SelectionState,
  ): void {
    const {
      scrollX, scrollY, viewWidth, viewHeight,
      pixelsPerPPQN, trackListWidth,
    } = config;

    const canvasLeft = trackListWidth;
    const canvasWidth = viewWidth - trackListWidth;
    const rulerBottom = RULER_HEIGHT;

    // Background
    ctx.fillStyle = BG_COLOR;
    ctx.fillRect(canvasLeft, rulerBottom, canvasWidth, viewHeight - rulerBottom);

    // Draw grid
    this.drawGrid(ctx, pixelsPerPPQN, scrollX, scrollY, canvasLeft, canvasWidth, viewHeight, rulerBottom);

    // Determine visible track range
    const firstTrack = Math.max(0, Math.floor(scrollY / TRACK_HEIGHT));
    const lastTrack = Math.min(
      tracks.length - 1,
      Math.ceil((scrollY + viewHeight - rulerBottom) / TRACK_HEIGHT),
    );

    for (let i = firstTrack; i <= lastTrack; i++) {
      const track = tracks[i];
      if (!track) continue;

      const trackY = rulerBottom + i * TRACK_HEIGHT - scrollY;

      // Track row background (alternating)
      if (i % 2 === 1) {
        ctx.fillStyle = '#161626';
        ctx.fillRect(canvasLeft, trackY, canvasWidth, TRACK_HEIGHT);
      }

      // Render each clip on this track
      for (const clip of track.clips) {
        this.renderClip(ctx, clip, trackY, pixelsPerPPQN, scrollX, canvasLeft, selection);
      }

      // Drop highlight for selected clips being dragged
      if (selection.selectedClipIds.size > 0) {
        // Light highlight on track row
        ctx.fillStyle = SELECTION_OVERLAY;
        ctx.fillRect(canvasLeft, trackY, canvasWidth, TRACK_HEIGHT);
      }
    }
  }

  private renderClip(
    ctx: CanvasRenderingContext2D,
    clip: AnyClipData,
    trackY: number,
    pixelsPerPPQN: number,
    scrollX: number,
    canvasLeft: number,
    selection: SelectionState,
  ): void {
    const x = clip.start_ppqn * pixelsPerPPQN - scrollX + canvasLeft;
    const w = clip.length_ppqn * pixelsPerPPQN;
    const h = TRACK_HEIGHT - 2;
    const y = trackY + 1;

    // Off-screen skip (use a large default since window may not exist in tests)
    const viewWidth = typeof window !== 'undefined' ? window.innerWidth : 4000;
    if (x + w < canvasLeft || x > canvasLeft + viewWidth) return;

    const isSelected = selection.selectedClipIds.has(clip.id.value);

    // Clip background
    const clipBg = clip.muted ? MUTED_OVERLAY : this.clipTypeColor(clip);
    ctx.fillStyle = clipBg;
    ctx.beginPath();
    this.roundRect(ctx, x, y, w, h, 3);
    ctx.fill();

    // Clip border
    ctx.strokeStyle = isSelected ? CLIP_SELECTED_BORDER : CLIP_BORDER;
    ctx.lineWidth = isSelected ? 2 : 1;
    ctx.beginPath();
    this.roundRect(ctx, x, y, w, h, 3);
    ctx.stroke();

    // Clip name
    ctx.fillStyle = CLIP_TEXT;
    ctx.font = '11px monospace';
    ctx.textBaseline = 'top';
    ctx.textAlign = 'left';
    const name = clip.name.length > 0 ? clip.name : `Clip ${clip.id.value}`;
    ctx.fillText(name, x + 4, y + 3);

    // Waveform for AudioClips
    if (isAudioClip(clip) && clip.waveform && clip.waveform.peaks.length > 0) {
      this.renderWaveform(ctx, clip, x, y, w, h);
    }

    // Note density for MidiClips
    if (isMidiClip(clip) && clip.note_density !== undefined) {
      this.renderMidiDensity(ctx, clip, x, y, w, h);
    }

    // Automation curve
    if (isAutomationClip(clip) && clip.curve_points && clip.curve_points.length > 0) {
      this.renderAutomationCurve(ctx, clip, x, y, w, h);
    }

    // Muted overlay
    if (clip.muted) {
      ctx.fillStyle = 'rgba(0,0,0,0.25)';
      ctx.beginPath();
      this.roundRect(ctx, x, y, w, h, 3);
      ctx.fill();
    }

    // "S" badge for session-captured clips
    if (isSessionCaptureClip(clip)) {
      ctx.fillStyle = '#88dd88';
      ctx.font = 'bold 10px monospace';
      ctx.textBaseline = 'bottom';
      ctx.textAlign = 'right';
      ctx.fillText('S', x + w - 3, y + h - 3);
    }
  }

  private clipTypeColor(clip: AnyClipData): string {
    if (isAudioClip(clip)) return CLIP_AUDIO_FILL;
    if (isMidiClip(clip)) return CLIP_MIDI_FILL;
    return CLIP_AUTO_FILL;
  }

  /** Render a waveform peek as a filled polygon. */
  private renderWaveform(
    ctx: CanvasRenderingContext2D,
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

    const numSamples = Math.min(peaks.length, Math.floor(clipW / 2));
    if (numSamples < 2) return;

    ctx.fillStyle = WAVEFORM_HIGH;
    ctx.beginPath();
    ctx.moveTo(clipX, bandTop + (1 - peaks[0]) * waveformAreaH);

    for (let i = 1; i < numSamples; i++) {
      const x = clipX + (i / numSamples) * clipW;
      const yPeak = bandTop + (1 - peaks[i]) * waveformAreaH;
      ctx.lineTo(x, yPeak);
    }

    // Back along valley bottom
    for (let i = numSamples - 1; i >= 0; i--) {
      const x = clipX + (i / numSamples) * clipW;
      const yValley = bandTop + (1 - valleys[i]) * waveformAreaH;
      ctx.lineTo(x, yValley);
    }

    ctx.closePath();
    ctx.fill();

    // Center line
    ctx.strokeStyle = WAVEFORM_LOW;
    ctx.lineWidth = 0.5;
    ctx.beginPath();
    ctx.moveTo(clipX, midY);
    ctx.lineTo(clipX + clipW, midY);
    ctx.stroke();
  }

  /** Render MIDI note density as vertical bars. */
  private renderMidiDensity(
    ctx: CanvasRenderingContext2D,
    clip: MidiClipData,
    clipX: number,
    clipY: number,
    clipW: number,
    clipH: number,
  ): void {
    const density = clip.note_density ?? 0.5;
    const barCount = Math.max(4, Math.min(64, Math.floor(clipW / 8)));
    const barW = clipW / barCount;

    ctx.fillStyle = '#5599cc';
    for (let i = 0; i < barCount; i++) {
      // Vary density slightly per bar for visual interest
      const phase = Math.sin(i * 1.5 + 1) * 0.3 + 0.7;
      const h = Math.max(3, clipH * 0.7 * density * phase);
      const x = clipX + i * barW;
      ctx.fillRect(x + 1, clipY + clipH - h - 2, Math.max(1, barW - 2), h);
    }
  }

  /** Render automation curve as a filled envelope. */
  private renderAutomationCurve(
    ctx: CanvasRenderingContext2D,
    clip: AutomationClipData,
    clipX: number,
    clipY: number,
    clipW: number,
    clipH: number,
  ): void {
    const points = clip.curve_points!;
    if (points.length < 2) return;

    ctx.strokeStyle = '#cc8844';
    ctx.lineWidth = 1.5;
    ctx.beginPath();

    const first = points[0];
    const lastX = clipX + ((first.ppqn - clip.start_ppqn) / clip.length_ppqn) * clipW;
    const lastY = clipY + clipH - first.value * clipH;
    ctx.moveTo(lastX, lastY);

    for (let i = 1; i < points.length; i++) {
      const normX = (points[i].ppqn - clip.start_ppqn) / clip.length_ppqn;
      const px = clipX + normX * clipW;
      const py = clipY + clipH - points[i].value * clipH;
      ctx.lineTo(px, py);
    }

    ctx.stroke();
  }

  /** Draw background grid lines. */
  private drawGrid(
    ctx: CanvasRenderingContext2D,
    pixelsPerPPQN: number,
    scrollX: number,
    _scrollY: number,
    canvasLeft: number,
    canvasWidth: number,
    viewHeight: number,
    rulerBottom: number,
  ): void {
    const stepPPQN = PPQN; // Beat grid
    const startPPQN = Math.max(0, Math.floor(scrollX / pixelsPerPPQN / stepPPQN) * stepPPQN);
    const endPPQN = Math.ceil((scrollX + canvasWidth) / pixelsPerPPQN / stepPPQN) * stepPPQN;

    for (let ppqn = startPPQN; ppqn <= endPPQN; ppqn += stepPPQN) {
      const x = ppqn * pixelsPerPPQN - scrollX + canvasLeft;

      // Bar lines (stronger)
      if (ppqn % (stepPPQN * 4) === 0) {
        ctx.strokeStyle = GRID_BAR;
        ctx.lineWidth = 1;
      } else {
        ctx.strokeStyle = GRID_BEAT;
        ctx.lineWidth = 0.5;
      }

      ctx.beginPath();
      ctx.moveTo(x, rulerBottom);
      ctx.lineTo(x, viewHeight);
      ctx.stroke();
    }
  }

  /** Round-rect helper. */
  private roundRect(
    ctx: CanvasRenderingContext2D,
    x: number, y: number, w: number, h: number, r: number,
  ): void {
    if (w < 0) return;
    if (h < 0) return;
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

  /** Hit-test: find the clip at pixel (px, py) within the canvas area. */
  hitTestClip(
    tracks: AnyTrackData[],
    scrollX: number,
    scrollY: number,
    pixelsPerPPQN: number,
    trackListWidth: number,
    px: number,
    py: number,
  ): AnyClipData | null {
    const canvasLeft = trackListWidth;
    if (px < canvasLeft) return null;

    const trackIndex = Math.floor((py - RULER_HEIGHT + scrollY) / TRACK_HEIGHT);
    if (trackIndex < 0 || trackIndex >= tracks.length) return null;

    const pixelPPQN = pixelsPerPPQN || 1;
    const clickPPQN = (px + scrollX - canvasLeft) / pixelPPQN;
    const track = tracks[trackIndex];

    for (const clip of track.clips) {
      const clipStart = clip.start_ppqn;
      const clipEnd = clipStart + clip.length_ppqn;
      if (clickPPQN >= clipStart && clickPPQN <= clipEnd) {
        return clip;
      }
    }

    return null;
  }

  /** Compute the pixel X position of a clip's left edge. */
  clipPixelX(
    clip: AnyClipData,
    scrollX: number,
    pixelsPerPPQN: number,
    trackListWidth: number,
  ): number {
    return clip.start_ppqn * pixelsPerPPQN - scrollX + trackListWidth;
  }

  /** Compute the pixel Y position of a track row. */
  trackPixelY(
    trackIndex: number,
    scrollY: number,
  ): number {
    return RULER_HEIGHT + trackIndex * TRACK_HEIGHT - scrollY;
  }
}
