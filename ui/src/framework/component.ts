// ── Component — Base Class ───────────────────────────────────────────
//
// Abstract component base with typed generics, lifecycle hooks,
// setState with dirty marking, and parent-chain dirty propagation.
//
// Design:
//   - Component<P, S> generic over props and state types
//   - setState(partial) merges state, marks dirty, propagates up
//   - Shallow equality check skips no-op setState calls
//   - Dirty flag propagates from child to root for reconcoler collection

import type { VNode } from '../ffi/bridge.js';
import type { Reconciler } from './reconciler.js';

// ── Global widget ID counter ─────────────────────────────────────

let _nextWidgetId = 1;

// ── Component ──────────────────────────────────────────────────────

export abstract class Component<P = {}, S = {}> {
  /** Unique widget ID, assigned on construction. Maps to C++ WidgetID. */
  readonly widgetId: number;

  /** Injected props (immutable reference by convention). */
  props: P;

  /** Mutable state. Updated via setState(). */
  state: S;

  // ── Internal tree and dirty-tracking fields ───────────────────

  /** Parent component in the component tree. */
  _parent: Component | null = null;

  /** Child components mounted under this component. */
  _children: Component[] = [];

  /** Dirty flag — true when this component or a descendant needs re-render. */
  _dirty = false;

  /** Whether this component is currently mounted in the tree. */
  _mounted = false;

  /** Reference to the reconciler that manages this component tree. */
  _reconciler: Reconciler | null = null;

  constructor(props: P) {
    this.widgetId = _nextWidgetId++;
    this.props = props;
    this.state = {} as S;
  }

  // ── State management ─────────────────────────────────────────

  /**
   * Merge partial state into current state.
   * Performs shallow equality check — skips dirty marking if nothing changed.
   */
  setState(partial: Partial<S>): void {
    // Shallow equality: check if any value actually changed
    let changed = false;
    const keys = Object.keys(partial) as (keyof S)[];
    for (let i = 0; i < keys.length; i++) {
      if (this.state[keys[i]] !== partial[keys[i]]) {
        changed = true;
        break;
      }
    }
    if (!changed) return;

    // Merge partial into state
    this.state = { ...this.state, ...partial };

    // Mark this component dirty
    this._dirty = true;

    // Propagate dirty flag up the parent chain
    if (this._parent) {
      this._parent._markDirtyUp();
    }

    // Notify reconciler to schedule a frame
    if (this._reconciler) {
      this._reconciler._onComponentDirty(this);
    }
  }

  // ── Dirty propagation (internal) ──────────────────────────────

  /**
   * Recursively mark this component and all ancestors dirty.
   * Called from child's setState or mount.
   */
  _markDirtyUp(): void {
    this._dirty = true;
    if (this._parent) {
      this._parent._markDirtyUp();
    }
  }

  // ── Lifecycle ─────────────────────────────────────────────────

  /**
   * Mount this component into the tree.
   * @param parent Optional parent component to attach to.
   */
  mount(parent?: Component): void {
    if (parent) {
      this._parent = parent;
      parent._children.push(this);
    }

    this._mounted = true;
    this._dirty = true;
    this.onMount();

    // The new dirty component needs to propagate up for scheduling
    if (this._parent) {
      this._parent._markDirtyUp();
    }
    if (this._reconciler) {
      this._reconciler._onComponentDirty(this);
    }
  }

  /**
   * Unmount this component from the tree.
   */
  unmount(): void {
    this._mounted = false;

    // Remove from parent's children list
    if (this._parent) {
      const idx = this._parent._children.indexOf(this);
      if (idx !== -1) {
        this._parent._children.splice(idx, 1);
      }
      this._parent = null;
    }

    this.onUnmount();
  }

  // ── Lifecycle hooks (override in subclasses) ─────────────────

  /** Called once after mount. Override for post-mount setup. */
  onMount(): void {
    // override in subclass
  }

  /** Called before unmount. Override for cleanup. */
  onUnmount(): void {
    // override in subclass
  }

  // ── Rendering ─────────────────────────────────────────────────

  /**
   * Produce a VNode descriptor for this component.
   * Called by the reconciler during flush for dirty components.
   */
  abstract render(): VNode;
}
