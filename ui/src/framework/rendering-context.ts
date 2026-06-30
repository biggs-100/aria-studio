// ── RenderingContext — Layout Context ────────────────────────────────
//
// Carries parent size information, device pixel ratio, and available
// space tracking for layout propagation through the component tree.
//
// Design:
//   - Created per component or inherited from parent via derive()
//   - useWidth()/useHeight() track consumed space for siblings
//   - DPR inherited from parent, defaults to 1

export class RenderingContext {
  /** Full logical width assigned to this layout slot. */
  readonly width: number;

  /** Full logical height assigned to this layout slot. */
  readonly height: number;

  /** Device pixel ratio (inherited from parent, default 1). */
  readonly dpr: number;

  /** Accumulated used width from sibling calls to useWidth(). */
  private _usedWidth = 0;

  /** Accumulated used height from sibling calls to useHeight(). */
  private _usedHeight = 0;

  constructor(width = 0, height = 0, dpr = 1) {
    this.width = width;
    this.height = height;
    this.dpr = dpr;
  }

  // ── Available space ───────────────────────────────────────────

  /** Remaining available width after useWidth() calls. */
  get availableWidth(): number {
    return Math.max(0, this.width - this._usedWidth);
  }

  /** Remaining available height after useHeight() calls. */
  get availableHeight(): number {
    return Math.max(0, this.height - this._usedHeight);
  }

  // ── Usage tracking ────────────────────────────────────────────

  /**
   * Record that a child or sibling consumed some width.
   * Decrements availableWidth for subsequent children.
   */
  useWidth(amount: number): void {
    this._usedWidth += amount;
  }

  /**
   * Record that a child or sibling consumed some height.
   * Decrements availableHeight for subsequent children.
   */
  useHeight(amount: number): void {
    this._usedHeight += amount;
  }

  /**
   * Reset usage tracking (called before laying out a new row/column).
   */
  reset(): void {
    this._usedWidth = 0;
    this._usedHeight = 0;
  }

  // ── Inheritance ──────────────────────────────────────────────

  /**
   * Create a child context for a nested layout slot.
   * Inherits DPR from the parent.
   */
  derive(childWidth: number, childHeight: number): RenderingContext {
    return new RenderingContext(childWidth, childHeight, this.dpr);
  }
}
