// ARIA DAW — Arrangement View Types & Constants
//
// Mirrors the C++ data model (model/types.h, model/clip.h, model/track.h,
// model/track_types.h, model/audio_clip.h) for the TypeScript UI layer.

// ─── Grid & Timing Constants ────────────────────────────────────

/** Pulses Per Quarter Note — the DAW's native grid resolution. */
export const PPQN = 960;

/** Beats per bar in 4/4 time. */
export const BEATS_PER_BAR = 4;

/** PPQN per sixteenth note (240 = 960 / 4). */
export const SIXTEENTH_PPQN = PPQN / 4;

/** PPQN per beat (quarter note). */
export const BEAT_PPQN = PPQN;

/** PPQN per bar. */
export const BAR_PPQN = PPQN * BEATS_PER_BAR;

// ─── Rendering Constants ────────────────────────────────────────

/** Default track row height in pixels. */
export const TRACK_HEIGHT = 48;

/** Default track list column width in pixels. */
export const TRACK_LIST_WIDTH = 200;

/** Default pixels per PPQN at 1:1 zoom (≈0.042). */
export const DEFAULT_PIXELS_PER_PPQN = 40 / PPQN;

/** Time ruler height in pixels. */
export const RULER_HEIGHT = 32;

/** Minimum pixels between grid lines before zooming out a level. */
export const GRID_MIN_PX_SPACING = 8;

/** Small clip handle hit area in pixels. */
export const HANDLE_SIZE = 6;

// ─── Zoom Levels ────────────────────────────────────────────────

/** Grid resolution levels matched to zoom. */
export enum GridLevel {
  Sixteenths,
  Beats,
  Bars,
}

/** Resolve grid level based on current zoom. */
export function getGridLevel(pixelsPerPPQN: number): GridLevel {
  if (pixelsPerPPQN * SIXTEENTH_PPQN >= GRID_MIN_PX_SPACING) {
    return GridLevel.Sixteenths;
  }
  if (pixelsPerPPQN * BEAT_PPQN >= GRID_MIN_PX_SPACING) {
    return GridLevel.Beats;
  }
  return GridLevel.Bars;
}

// ─── Type Aliases (mirror C++ FadeShape, TrackType, etc.) ───────

/** Unique identifier for a session scene (mirrors C++ SceneID). */
export interface SceneID {
  value: number;
}

/** Fade curve shapes (matches C++ FadeShape). */
export type FadeShape =
  | 'None'
  | 'LinearIn'
  | 'LinearOut'
  | 'EqualPowerIn'
  | 'EqualPowerOut'
  | 'ExponentialIn'
  | 'ExponentialOut';

/** Track specialization types (matches C++ TrackType). */
export type TrackType =
  | 'Audio'
  | 'MIDI'
  | 'Group'
  | 'VCA'
  | 'Return'
  | 'Master';

/** Group mode (matches C++ GroupMode). */
export type GroupMode = 'Summing' | 'Folder' | 'Routing';

/** VCA affects (matches C++ VCAAffects). */
export type VCAAffects = 'Volume' | 'VolumePan' | 'All';

// ─── Identity Types ─────────────────────────────────────────────

/** Unique identifier for a clip (mirrors C++ ClipID). */
export interface ClipID {
  value: number;
}

export function clipId(v: number): ClipID {
  return { value: v };
}

/** Unique identifier for a track (mirrors C++ TrackID). */
export interface TrackID {
  value: number;
}

export function trackId(v: number): TrackID {
  return { value: v };
}

// ─── Waveform / Gain Envelope ───────────────────────────────────

/** A single point in a clip's gain envelope. */
export interface GainPoint {
  ppqn: number;
  gain: number;
}

/** Pre-computed peak/valley pairs for waveform rendering at a LOD. */
export interface WaveformLOD {
  peaks: number[];
  valleys: number[];
}

// ─── Clip Data (mirrors C++ Clip / AudioClip / MidiClip) ────────

/** Base clip properties (mirrors C++ Clip). */
export interface ClipData {
  id: ClipID;
  start_ppqn: number;
  length_ppqn: number;
  name: string;
  color: number;
  gain: number;
  muted: boolean;
  fade_in_ppqn: number;
  fade_out_ppqn: number;
  fade_in_shape: FadeShape;
  fade_out_shape: FadeShape;
  looping: boolean;
  loop_start_ppqn: number;
  loop_end_ppqn: number;
  track_id: TrackID;
}

/** Audio clip extension (mirrors C++ AudioClip). */
export interface AudioClipData extends ClipData {
  type: 'Audio';
  file_path: string;
  waveform?: WaveformLOD;
}

/** MIDI clip extension. */
export interface MidiClipData extends ClipData {
  type: 'MIDI';
  note_density?: number;
}

/** Automation clip extension. */
export interface AutomationClipData extends ClipData {
  type: 'Automation';
  curve_points?: { ppqn: number; value: number }[];
}

/** Session capture clip extension: a clip captured from Session view into Arrangement. */
export interface SessionCaptureClipData extends ClipData {
  type: 'SessionCapture';
  capture_source_track_id: TrackID;
  capture_source_scene_id: SceneID;
}

/** Union of all clip data variants. */
export type AnyClipData = AudioClipData | MidiClipData | AutomationClipData | SessionCaptureClipData;

/** Discriminated clip type guard. */
export function isAudioClip(c: AnyClipData): c is AudioClipData {
  return c.type === 'Audio';
}
export function isMidiClip(c: AnyClipData): c is MidiClipData {
  return c.type === 'MIDI';
}
export function isAutomationClip(c: AnyClipData): c is AutomationClipData {
  return c.type === 'Automation';
}
export function isSessionCaptureClip(c: AnyClipData): c is SessionCaptureClipData {
  return c.type === 'SessionCapture';
}

// ─── Track Data (mirrors C++ Track) ─────────────────────────────

/** Base track properties (mirrors C++ Track). */
export interface TrackData {
  id: TrackID;
  name: string;
  type: TrackType;
  volume: number;
  pan: number;
  muted: boolean;
  soloed: boolean;
  clips: AnyClipData[];
}

/** Group track extension (mirrors C++ GroupTrack). */
export interface GroupTrackData extends TrackData {
  type: 'Group';
  children: TrackID[];
  group_mode: GroupMode;
  folded: boolean;
}

/** VCA track extension (mirrors C++ VCATrack). */
export interface VCATrackData extends TrackData {
  type: 'VCA';
  slaves: TrackID[];
  affects: VCAAffects;
}

/** Return track extension (mirrors C++ ReturnTrack). */
export interface ReturnTrackData extends TrackData {
  type: 'Return';
  solo_safe: boolean;
}

/** Master track extension (mirrors C++ MasterTrack). */
export interface MasterTrackData extends TrackData {
  type: 'Master';
}

/** Union of all track data variants. */
export type AnyTrackData =
  | TrackData
  | GroupTrackData
  | VCATrackData
  | ReturnTrackData
  | MasterTrackData;

/** Discriminated track type guard. */
export function isGroupTrack(t: AnyTrackData): t is GroupTrackData {
  return t.type === 'Group';
}
export function isVCATrack(t: AnyTrackData): t is VCATrackData {
  return t.type === 'VCA';
}

// ─── Data Source Interface ──────────────────────────────────────

/**
 * Bridge between the C++ model and the TypeScript ArrangementView.
 *
 * In production this is implemented by a binding that reads
 * tracks / clips / playhead from the C++ engine. In tests this
 * is replaced with a mock or fixture.
 */
export interface ArrangementDataSource {
  /** All tracks (including folded children). */
  getTracks(): AnyTrackData[];

  /** Tracks visible in the arrangement view (respects GroupTrack.folded). */
  getVisibleTracks(): AnyTrackData[];

  /** Current playhead position in PPQN. */
  getPlayheadPPQN(): number;

  /** Pixels per PPQN at current zoom level. */
  getPixelsPerPPQN(): number;

  /** Whether grid snap is enabled. */
  isGridSnapEnabled(): boolean;

  /** Grid snap resolution in PPQN (e.g. 240 for 16th notes). */
  getGridPpqn(): number;
}

// ─── Selection State ────────────────────────────────────────────

/** Tracks which clips are selected / hovered. */
export interface SelectionState {
  selectedClipIds: Set<number>;
  hoveredClipId: number | null;
}

// ─── Fade Evaluation ────────────────────────────────────────────

/** Evaluate a fade curve at normalised position t ∈ [0, 1].
 *  Returns gain multiplier (0 = silence, 1 = unity).
 *  Mirrors C++ Clip::evaluate_fade().
 */
export function evaluateFade(shape: FadeShape, t: number): number {
  const clamped = Math.max(0, Math.min(1, t));
  switch (shape) {
    case 'None':
      return 1.0;
    case 'LinearIn':
      return clamped;
    case 'LinearOut':
      return 1 - clamped;
    case 'EqualPowerIn':
      return Math.sqrt(clamped);
    case 'EqualPowerOut':
      return Math.sqrt(1 - clamped);
    case 'ExponentialIn':
      return clamped * clamped;
    case 'ExponentialOut':
      return 1 - (1 - clamped) * (1 - clamped);
  }
  return 1.0;
}

/** Round `ppqn` to the nearest multiple of `gridPpqn`. */
export function snapToGrid(
  ppqn: number,
  gridPpqn: number,
): number {
  if (gridPpqn <= 0) return ppqn;
  return Math.round(ppqn / gridPpqn) * gridPpqn;
}

/** Compute pixel x-position for a PPQN value. */
export function ppqnToPixel(
  ppqn: number,
  pixelsPerPPQN: number,
): number {
  return ppqn * pixelsPerPPQN;
}

/** Compute PPQN from a pixel x-position. */
export function pixelToPpqn(
  pixelX: number,
  pixelsPerPPQN: number,
): number {
  return pixelsPerPPQN > 0 ? Math.round(pixelX / pixelsPerPPQN) : 0;
}

/** Get bar number from a PPQN position (1-indexed). */
export function ppqnToBar(ppqn: number): number {
  return Math.floor(ppqn / BAR_PPQN) + 1;
}

/** Get beat within bar from a PPQN position (1-indexed). */
export function ppqnToBeat(ppqn: number): number {
  return (Math.floor(ppqn / BEAT_PPQN) % BEATS_PER_BAR) + 1;
}
