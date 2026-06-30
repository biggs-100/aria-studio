// ── Docking System — Types ──────────────────────────────────────────
//
// Core types for the JSON-layout-driven docking system.
// DockManager owns a DockLayout tree as component state.
//
// Design:
//   - DockNode is a discriminated union via `type` field
//   - 'leaf': single panel container (no children)
//   - 'split': horizontal/vertical divider with children[]
//   - 'tab': tab group with ordered panel IDs
//   - 'float': floating/overlay window (stored in DockLayout.floating[])
//   - DockLayout.root is the main tree; floating[] are overlay windows

/** Where a panel is placed relative to existing content. */
export type DockLocation = 'left' | 'right' | 'top' | 'bottom' | 'center';

/**
 * A node in the docking layout tree.
 *
 * Discriminated by `type`:
 * - `leaf`  → panelType (optional — undefined = empty slot)
 * - `split` → direction, children, sizes[]
 * - `tab`   → tabs[], activeTab
 * - `float` → panelType, x/y/w/h (position data)
 */
export interface DockNode {
  /** Unique node identifier. Stable across serialization. */
  id: string;

  /** Node kind — determines which fields are meaningful. */
  type: 'leaf' | 'split' | 'tab' | 'float';

  // ── Leaf ─────────────────────────────────────────────────────

  /** Type key for the panel component (leaf and float nodes). */
  panelType?: string;

  // ── Split ────────────────────────────────────────────────────

  /** Split direction (only for split nodes). */
  direction?: 'horizontal' | 'vertical';

  /** Child nodes (only for split nodes). */
  children?: DockNode[];

  /** Relative or absolute sizes for each child (only for split nodes). */
  sizes?: number[];

  // ── Tab ──────────────────────────────────────────────────────

  /** Ordered list of panel IDs in this tab group. */
  tabs?: string[];

  /** Index of the active (visible) tab. */
  activeTab?: number;

  // ── Float ────────────────────────────────────────────────────

  /** Floating window X position. */
  x?: number;

  /** Floating window Y position. */
  y?: number;

  /** Floating window width. */
  w?: number;

  /** Floating window height. */
  h?: number;
}

/**
 * Complete docking layout.
 * - `root`: the main dock tree (leaf, split, or tab)
 * - `floating`: array of floating overlay windows
 */
export interface DockLayout {
  root: DockNode;
  floating: DockNode[];
}

/**
 * Factory function signature for panel creation.
 * Receives the panelId and returns a type key the bridge can serialize.
 */
export type PanelFactory = (panelId: string) => string;

/**
 * Default min size for panels in a split, in pixels.
 */
export const DEFAULT_MIN_SIZE = 100;

/**
 * Default split ratio used when creating new splits.
 */
export const DEFAULT_SPLIT_RATIO = 0.5;
