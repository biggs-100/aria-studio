// ARIA DAW — Session View Types & Constants
//
// TypeScript mirrors of C++ session model types (model/session.h,
// model/types.h) for the TypeScript Canvas SessionView UI layer.

import type { TrackID, TrackData } from '../ArrangementView/types.js';

// ─── Grid & Layout Constants ─────────────────────────────────────

/** Session view cell (clip slot) height in pixels. */
export const SLOT_HEIGHT = 48;

/** Session view column (track) header height in pixels. */
export const COLUMN_HEADER_HEIGHT = 32;

/** Scene strip width (left column) in pixels. */
export const SCENE_STRIP_WIDTH = 140;

/** Crossfader widget height in pixels. */
export const CROSSFADER_HEIGHT = 48;

/** Minimum cell width for a clip slot. */
export const MIN_SLOT_WIDTH = 100;

/** Maximum cell width before clips look too big. */
export const MAX_SLOT_WIDTH = 200;

// ─── Enums (mirror C++ model/session.h) ──────────────────────────

/** Launch state of a clip slot. */
export type LaunchState = 'Stopped' | 'Triggered' | 'Playing';

/** Follow action after clip/scene playback ends. */
export type FollowAction =
  | 'None'
  | 'Stop'
  | 'PlayNext'
  | 'PlayRandom'
  | 'ContinueAsLoop';

/** Crossfader assignment for a track. */
export type CrossfaderAssignment = 'None' | 'A' | 'B' | 'Both';

/** Crossfader curve type. */
export type CrossfaderCurve = 'Linear' | 'EqualPower';

/** Launch quantization level. */
export type LaunchQuantization = 'None' | 'Bar' | 'Half' | 'Quarter' | 'Eighth';

// ─── Identity Types ──────────────────────────────────────────────

/** Unique identifier for a session scene (mirrors C++ SceneID). */
export interface SceneID {
  value: number;
}

export function sceneId(v: number): SceneID {
  return { value: v };
}

// ─── Session Data Types (mirror C++ Scene, ClipSlot, Crossfader) ─

/** A session scene — a row in the clip grid (mirrors C++ Scene). */
export interface SceneData {
  id: SceneID;
  name: string;
  color: number;
  order_index: number;
  follow_action: FollowAction;
}

/** A single cell in the session grid (track × scene). */
export interface ClipSlotData {
  clip_name: string;
  clip_color: number;
  has_clip: boolean;
  state: LaunchState;
  follow_action: FollowAction;
}

/** Scene-level summary — playing state indicator. */
export interface SceneSummary {
  scene_id: SceneID;
  is_playing: boolean;
  any_triggered: boolean;
}

/** Global crossfader state (mirrors C++ Crossfader). */
export interface CrossfaderData {
  position: number;        // -1.0 (full A) to +1.0 (full B)
  curve: CrossfaderCurve;
}

// ─── Viewport Geometry ───────────────────────────────────────────

/** Scroll / viewport state for the session view. */
export interface SessionViewport {
  scrollX: number;
  scrollY: number;
  viewWidth: number;
  viewHeight: number;
}

// ─── Data Source Interface ───────────────────────────────────────

/**
 * Bridge between the C++ Session model and the TypeScript SessionView.
 *
 * In production this is implemented by a binding that reads
 * scenes / clip slots / crossfader state from the C++ engine.
 * In tests this is replaced with a mock or fixture.
 */
export interface SessionDataSource {
  /** All tracks (visible in the session). */
  getTracks(): TrackData[];

  /** All scenes in order. */
  getScenes(): SceneData[];

  /** Get the clip slot data for a given track × scene. */
  getSlot(trackId: TrackID, sceneId: SceneID): ClipSlotData;

  /** Get scene-level playing state. */
  getSceneSummary(sceneId: SceneID): SceneSummary;

  /** Current crossfader state. */
  getCrossfader(): CrossfaderData;

  /** Initiate a clip launch. */
  launchClip(trackId: TrackID, sceneId: SceneID): void;

  /** Initiate a scene launch. */
  launchScene(sceneId: SceneID): void;

  /** Set crossfader position (-1..+1). */
  setCrossfaderPosition(pos: number): void;
}

// ─── Crossfader Gain Evaluation ──────────────────────────────────

/**
 * Evaluate the crossfader gain for a given assignment and position.
 *
 * Mirrors C++ Session::evaluate_crossfader_gain().
 *
 * @param assignment The track's crossfader assignment.
 * @param position   Global crossfader position (-1 = full A, +1 = full B).
 * @param curve      The crossfader curve type.
 * @returns Gain multiplier [0..1] to apply to the track's output.
 */
export function evaluateCrossfaderGain(
  assignment: CrossfaderAssignment,
  position: number,
  curve: CrossfaderCurve,
): number {
  switch (assignment) {
    case 'None':
    case 'Both':
      return 1.0;
    case 'A':
      return crossfadeSideGain(position, false, curve);
    case 'B':
      return crossfadeSideGain(position, true, curve);
  }
}

/**
 * Compute gain for one side of the crossfader.
 *
 * @param position   Global crossfader position (-1..+1).
 * @param isSideB    True for side B, false for side A.
 * @param curve      Curve type.
 * @returns Gain multiplier [0..1].
 */
function crossfadeSideGain(
  position: number,
  isSideB: boolean,
  curve: CrossfaderCurve,
): number {
  const clamped = Math.max(-1, Math.min(1, position));

  // Side A gain when position is -1 (full left) → 1.0, at +1 → 0.0
  // Side B gain when position is +1 (full right) → 1.0, at -1 → 0.0
  const t = isSideB
    ? (clamped + 1) / 2   // 0 at -1, 1 at +1
    : 1 - (clamped + 1) / 2; // 1 at -1, 0 at +1

  const clampedT = Math.max(0, Math.min(1, t));

  switch (curve) {
    case 'Linear':
      return clampedT;
    case 'EqualPower':
      return Math.sqrt(clampedT);
  }
  return clampedT;
}
