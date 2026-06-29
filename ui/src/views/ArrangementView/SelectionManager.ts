// ARIA DAW — SelectionManager
//
// Handles clip selection (click, SHIFT-click), drag-to-reposition
// with grid snap, and provides the active SelectionState to the
// arrangement view renderer.

import {
  AnyClipData,
  AnyTrackData,
  ClipID,
  RULER_HEIGHT,
  TRACK_HEIGHT,
  SelectionState,
  snapToGrid,
} from './types.js';

// ─── Drag Operation ─────────────────────────────────────────────

export interface DragOperation {
  /** Clip IDs being dragged. */
  clipIds: ClipID[];
  /** Offset in PPQN from each clip's start_ppqn to the initial click. */
  anchorOffsets: Map<number, number>;
  /** PPQN position of the cursor at drag start. */
  originPPQN: number;
  /** Whether the drag has moved enough to be considered a real drag. */
  didDrag: boolean;
  /** Last snap-resolved PPQN delta applied. */
  lastDeltaPPQN: number;
}

// ─── SelectionManager ───────────────────────────────────────────

export class SelectionManager {
  private _state: SelectionState = {
    selectedClipIds: new Set<number>(),
    hoveredClipId: null,
  };

  private _dragOp: DragOperation | null = null;

  /** Current selection state (read-only reference for rendering). */
  get state(): SelectionState {
    return this._state;
  }

  /** Whether a drag operation is in progress. */
  get isDragging(): boolean {
    return this._dragOp !== null && this._dragOp.didDrag;
  }

  /** The active drag operation, if any. */
  get dragOp(): DragOperation | null {
    return this._dragOp;
  }

  // ─── Mouse Event Handlers ───────────────────────────────────

  /** Handle a mouse down event on the clip canvas. Returns the clip
   *  that was clicked, if any. */
  onMouseDown(
    tracks: AnyTrackData[],
    scrollX: number,
    scrollY: number,
    pixelsPerPPQN: number,
    trackListWidth: number,
    _gridSnapEnabled: boolean,
    _gridPpqn: number,
    shiftHeld: boolean,
    px: number,
    py: number,
  ): AnyClipData | null {
    const clicked = this.hitTestClip(
      tracks, scrollX, scrollY, pixelsPerPPQN, trackListWidth, px, py,
    );

    if (!clicked) {
      // Click on empty space — deselect all unless SHIFT is held
      if (!shiftHeld) {
        this.clearSelection();
      }
      return null;
    }

    // SHIFT-click: toggle this clip in selection
    if (shiftHeld) {
      this.toggleClip(clicked);
    } else if (!this._state.selectedClipIds.has(clicked.id.value)) {
      // Click on unselected clip — select only this one
      this.selectOnly(clicked);
    }

    // Start drag operation
    const clipIds = this.getSelectedClipIDs();
    const anchorPPQN = this.pixelToPPQN(px, scrollX, pixelsPerPPQN, trackListWidth);
    const offsets = new Map<number, number>();

    for (const cid of clipIds) {
      const clip = this.findClipByID(tracks, cid);
      if (clip) {
        offsets.set(cid.value, clip.start_ppqn - anchorPPQN);
      }
    }

    this._dragOp = {
      clipIds,
      anchorOffsets: offsets,
      originPPQN: anchorPPQN,
      didDrag: false,
      lastDeltaPPQN: 0,
    };

    return clicked;
  }

  /** Handle mouse move during a potential drag. Returns PPQN delta
   *  if the drag moved, or null if there's no active drag. */
  onMouseMove(
    _tracks: AnyTrackData[],
    scrollX: number,
    _scrollY: number,
    pixelsPerPPQN: number,
    trackListWidth: number,
    gridSnapEnabled: boolean,
    gridPpqn: number,
    px: number,
    _py: number,
  ): number | null {
    if (!this._dragOp) return null;

    const currentPPQN = this.pixelToPPQN(
      px, scrollX, pixelsPerPPQN, trackListWidth,
    );

    const delta = currentPPQN - this._dragOp.originPPQN;

    // Minimum drag threshold (5 PPQN) to distinguish click from drag
    if (!this._dragOp.didDrag && Math.abs(delta) < 5) {
      return null;
    }

    this._dragOp.didDrag = true;

    let snappedDelta = delta;
    if (gridSnapEnabled && gridPpqn > 0) {
      // Compute grid-snapped delta
      const rawStart = (this._dragOp.clipIds.length > 0)
        ? this._dragOp.anchorOffsets.get(
            this._dragOp.clipIds[0].value,
          ) ?? 0 + currentPPQN
        : currentPPQN;
      const snapped = snapToGrid(rawStart, gridPpqn);
      snappedDelta = snapped - ((this._dragOp.anchorOffsets.get(
        this._dragOp.clipIds[0]?.value ?? 0,
      ) ?? 0) + this._dragOp.originPPQN);

      // Alternate approach: snap the total new position of the first clip
      const firstClipId = this._dragOp.clipIds[0]?.value;
      const firstOffset = this._dragOp.anchorOffsets.get(firstClipId ?? 0) ?? 0;
      const rawNewPos = currentPPQN + firstOffset;
      const snappedPos = snapToGrid(rawNewPos, gridPpqn);
      snappedDelta = snappedPos - (this._dragOp.originPPQN + firstOffset);
    }

    this._dragOp.lastDeltaPPQN = snappedDelta;
    return snappedDelta;
  }

  /** Handle mouse up — finalize the drag and return the deltas
   *  to apply. Returns a map of clipID → new start_ppqn, or null. */
  onMouseUp(
    _tracks: AnyTrackData[],
    _pixelsPerPPQN: number,
    _gridSnapEnabled: boolean,
    _gridPpqn: number,
  ): Map<number, number> | null {
    if (!this._dragOp || !this._dragOp.didDrag) {
      this._dragOp = null;
      return null;
    }

    const delta = this._dragOp.lastDeltaPPQN;
    const result = new Map<number, number>();

    for (const cid of this._dragOp.clipIds) {
      const clip = this.findClipByID(_tracks, cid);
      if (clip) {
        const newStart = Math.max(0, clip.start_ppqn + delta);
        result.set(clip.id.value, newStart);
      }
    }

    this._dragOp = null;
    return result;
  }

  /** Cancel an in-progress drag. Call when ESC is pressed or drag
   *  leaves valid area. */
  cancelDrag(): void {
    this._dragOp = null;
  }

  // ─── Selection Mutators ──────────────────────────────────────

  selectOnly(clip: AnyClipData): void {
    this._state.selectedClipIds.clear();
    this._state.selectedClipIds.add(clip.id.value);
  }

  toggleClip(clip: AnyClipData): void {
    if (this._state.selectedClipIds.has(clip.id.value)) {
      this._state.selectedClipIds.delete(clip.id.value);
    } else {
      this._state.selectedClipIds.add(clip.id.value);
    }
  }

  clearSelection(): void {
    this._state.selectedClipIds.clear();
  }

  setHovered(clip: AnyClipData | null): void {
    this._state.hoveredClipId = clip ? clip.id.value : null;
  }

  // ─── Helpers ─────────────────────────────────────────────────

  private getSelectedClipIDs(): ClipID[] {
    return Array.from(this._state.selectedClipIds).map(id => ({ value: id }));
  }

  private findClipByID(
    tracks: AnyTrackData[],
    clipId: ClipID,
  ): AnyClipData | null {
    for (const track of tracks) {
      for (const clip of track.clips) {
        if (clip.id.value === clipId.value) {
          return clip;
        }
      }
    }
    return null;
  }

  private hitTestClip(
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

  private pixelToPPQN(
    px: number,
    scrollX: number,
    pixelsPerPPQN: number,
    trackListWidth: number,
  ): number {
    const canvasLeft = trackListWidth;
    const ppqn = (px + scrollX - canvasLeft) / (pixelsPerPPQN || 1);
    return Math.round(ppqn);
  }
}
