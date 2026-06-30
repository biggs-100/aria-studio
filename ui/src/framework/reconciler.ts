// ── Reconciler — Dirty-Tree Scheduler ────────────────────────────────
//
// Schedules requestAnimationFrame callbacks on dirty marking,
// collects dirty subtree roots during flush, and drives serialization
// through the FFI bridge.
//
// Design:
//   - markDirty propagates dirty flag from component to root
//   - collectDirty returns Set<number> of widget IDs for dirty subtree roots
//   - Single rAF coalesces multiple dirty marks into one flush pass

import type { Component } from './component.js';
import type { Bridge, VNode } from '../ffi/bridge.js';

export class Reconciler {
  /** Root component of the tree. */
  private root: Component | null = null;

  /** FFI bridge for serialization and execution. */
  private _bridge: Bridge | null = null;

  /** Whether a rAF callback is already scheduled. */
  private _frameScheduled = false;

  // ── Configuration ─────────────────────────────────────────────

  setRoot(component: Component): void {
    this.root = component;
    component._reconciler = this;
  }

  setBridge(bridge: Bridge): void {
    this._bridge = bridge;
  }

  getRoot(): Component | null {
    return this.root;
  }

  // ── Dirty notification (called by Component) ─────────────────

  /**
   * Called by a component when it becomes dirty (via setState or mount).
   * Schedules a rAF flush if none is pending.
   */
  _onComponentDirty(_component: Component): void {
    this._scheduleFrame();
  }

  // ── Frame scheduling ──────────────────────────────────────────

  private _scheduleFrame(): void {
    if (this._frameScheduled) return;
    this._frameScheduled = true;
    requestAnimationFrame(() => this._flush());
  }

  /** Start the rendering loop. */
  start(): void {
    // Renders are driven by dirty marks — no continuous loop needed
    // A manual initial render can be triggered by marking the root dirty
    if (this.root) {
      this.root._dirty = true;
      this._scheduleFrame();
    }
  }

  /** Stop the rendering loop (prevents pending rAF from flushing). */
  stop(): void {
    this._frameScheduled = false;
  }

  // ── Flush ─────────────────────────────────────────────────────

  /**
   * The rAF callback. Collects dirty subtree roots, renders them,
   * serializes through the bridge, and clears dirty flags.
   */
  private _flush(): void {
    this._frameScheduled = false;
    if (!this.root || !this._bridge) return;

    const dirtyIds = this.collectDirty();
    if (dirtyIds.size === 0) return;

    // Render component tree to VNode tree (recursive via component.render())
    const rootVNode: VNode = this.root.render();

    // Build commands for dirty subtrees — bridge walks VNode tree filtering by dirtyIds
    const commands = this._bridge.serializeDirty(rootVNode, dirtyIds);
    this._bridge.execute(commands);

    // Clear all dirty flags after successful flush
    this._clearDirty(this.root);
  }

  // ── Dirty root collection ─────────────────────────────────────

  /**
   * Walk the component tree and collect widget IDs of topmost dirty
   * components (dirty subtree roots). Clean subtrees are skipped.
   */
  collectDirty(): Set<number> {
    const ids = new Set<number>();
    if (this.root) {
      this._collectDirtyRecursive(this.root, ids);
    }
    return ids;
  }

  /**
   * Recursively walk the component tree.
   * When a dirty node is found, add its widgetId and STOP descending
   * (it becomes a dirty subtree root). Otherwise, recurse into children.
   */
  private _collectDirtyRecursive(node: Component, out: Set<number>): void {
    if (node._dirty) {
      out.add(node.widgetId);
      // This is a dirty subtree root — don't descend further.
      // The bridge serializes the entire subtree starting from this node.
      return;
    }
    for (let i = 0; i < node._children.length; i++) {
      this._collectDirtyRecursive(node._children[i], out);
    }
  }

  /**
   * Clear dirty flags on all components in the tree.
   */
  private _clearDirty(node: Component): void {
    node._dirty = false;
    for (let i = 0; i < node._children.length; i++) {
      this._clearDirty(node._children[i]);
    }
  }
}
