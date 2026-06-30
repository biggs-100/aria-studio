// Reconciler — Unit Tests
// Tests for dirty-tree scheduling, collection, and flush.
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { Component } from '../../src/framework/component.js';
import { Reconciler } from '../../src/framework/reconciler.js';
import type { VNode } from '../../src/ffi/bridge.js';
import { Bridge } from '../../src/ffi/bridge.js';

// ── Test Helper Components ──────────────────────────────────────────

class RootComponent extends Component<{}, { label: string }> {
  constructor() {
    super({});
    this.state = { label: 'root' };
  }
  render(): VNode {
    return { type: 'container', key: String(this.widgetId), props: {} };
  }
}

class LeafComponent extends Component<{}, { value: number }> {
  constructor() {
    super({});
    this.state = { value: 0 };
  }
  render(): VNode {
    return {
      type: 'rect',
      key: String(this.widgetId),
      props: { x: 0, y: 0, w: 10, h: 10 },
    };
  }
}

// ── Tests ───────────────────────────────────────────────────────────

describe('Reconciler', () => {
  let rafCalls: Array<FrameRequestCallback>;
  let originalRAF: typeof requestAnimationFrame;

  beforeEach(() => {
    originalRAF = globalThis.requestAnimationFrame;
    rafCalls = [];
    // Mock rAF to COLLECT callbacks instead of executing them immediately
    globalThis.requestAnimationFrame = ((cb: FrameRequestCallback) => {
      rafCalls.push(cb);
      return rafCalls.length;
    }) as unknown as typeof requestAnimationFrame;
  });

  afterEach(() => {
    globalThis.requestAnimationFrame = originalRAF;
  });

  /** Drain queued rAF callbacks synchronously. */
  function drainRAF(): void {
    const pending = rafCalls.splice(0);
    for (const cb of pending) cb(performance.now());
  }

  describe('markDirty / schedule', () => {
    it('schedules a rAF frame on _onComponentDirty', () => {
      const reconciler = new Reconciler();
      const root = new RootComponent();
      reconciler.setRoot(root);

      reconciler._onComponentDirty(root);

      expect(rafCalls.length).toBe(1);
    });

    it('does NOT schedule additional rAF when one is already pending', () => {
      const reconciler = new Reconciler();
      const root = new RootComponent();
      reconciler.setRoot(root);

      reconciler._onComponentDirty(root);
      expect(rafCalls.length).toBe(1);

      // Second call while rAF is still pending
      reconciler._onComponentDirty(root);
      expect(rafCalls.length).toBe(1); // still only one rAF queued
    });
  });

  describe('collectDirty', () => {
    it('returns empty set when no components are dirty', () => {
      const reconciler = new Reconciler();
      const root = new RootComponent();
      root._dirty = false; // not dirty
      reconciler.setRoot(root);

      const ids = reconciler.collectDirty();
      expect(ids.size).toBe(0);
    });

    it('returns root widgetId when root is dirty', () => {
      const reconciler = new Reconciler();
      const root = new RootComponent();
      root._dirty = true;
      reconciler.setRoot(root);

      const ids = reconciler.collectDirty();
      expect(ids.size).toBe(1);
      expect(ids.has(root.widgetId)).toBe(true);
    });

    it('returns only topmost dirty roots, not all dirty nodes', () => {
      const reconciler = new Reconciler();
      const root = new RootComponent();
      const child = new LeafComponent();
      const grandchild = new LeafComponent();

      // Setup tree: root → child → grandchild
      root.mount();
      child.mount(root);
      grandchild.mount(child);

      // Clear all mount-induced dirty flags
      root._dirty = false;
      child._dirty = false;
      grandchild._dirty = false;

      // Mark just the middle node dirty
      child._dirty = true;
      grandchild._dirty = true; // under child — should be hidden by child

      reconciler.setRoot(root);

      const ids = reconciler.collectDirty();

      // Only child should be returned (grandchild is under child's dirty subtree)
      expect(ids.size).toBe(1);
      expect(ids.has(child.widgetId)).toBe(true);
      expect(ids.has(grandchild.widgetId)).toBe(false);
      expect(ids.has(root.widgetId)).toBe(false);
    });

    it('returns multiple dirty roots from different branches', () => {
      const reconciler = new Reconciler();
      const root = new RootComponent();
      const branch1 = new LeafComponent();
      const branch2 = new LeafComponent();
      const branch3 = new LeafComponent();

      root.mount();
      branch1.mount(root);
      branch2.mount(root);
      branch3.mount(root);

      // Clear mount-induced dirty flags
      root._dirty = false;
      branch1._dirty = false;
      branch2._dirty = false;
      branch3._dirty = false;

      // Mark branches 1 and 3 as dirty
      branch1._dirty = true;
      branch3._dirty = true;

      reconciler.setRoot(root);

      const ids = reconciler.collectDirty();
      expect(ids.size).toBe(2);
      expect(ids.has(branch1.widgetId)).toBe(true);
      expect(ids.has(branch3.widgetId)).toBe(true);
      expect(ids.has(branch2.widgetId)).toBe(false);
    });
  });

  describe('flush cycle', () => {
    it('calls bridge.serializeDirty during flush', () => {
      const reconciler = new Reconciler();
      const bridge = new Bridge();
      reconciler.setBridge(bridge);
      const root = new RootComponent();
      root._dirty = true;
      reconciler.setRoot(root);

      const serializeSpy = vi.spyOn(bridge, 'serializeDirty');

      // Trigger flush
      reconciler._onComponentDirty(root);
      drainRAF();

      // Flush ran — serializeDirty should have been called
      expect(serializeSpy).toHaveBeenCalled();
      const callArgs = serializeSpy.mock.calls[0];
      expect(callArgs[0]).toHaveProperty('type');   // VNode root
      expect(typeof callArgs[0].type).toBe('string');
      expect(callArgs[1] instanceof Set).toBe(true); // Set of widget IDs
    });

    it('clears dirty flags after flush', () => {
      const reconciler = new Reconciler();
      const bridge = new Bridge();
      reconciler.setBridge(bridge);
      const root = new RootComponent();
      const child = new LeafComponent();

      root.mount();
      child.mount(root);
      reconciler.setRoot(root);

      // Clear mount flags, then set specific ones
      root._dirty = false;
      child._dirty = false;
      child._dirty = true;

      // Trigger and drain
      reconciler._onComponentDirty(root);
      drainRAF();

      // Dirty flags should be cleared
      expect(root._dirty).toBe(false);
      expect(child._dirty).toBe(false);
    });

    it('does nothing when no dirty components exist', () => {
      const reconciler = new Reconciler();
      const bridge = new Bridge();
      reconciler.setBridge(bridge);
      const root = new RootComponent();
      root._dirty = false;
      reconciler.setRoot(root);

      const serializeSpy = vi.spyOn(bridge, 'serializeDirty');

      reconciler._onComponentDirty(root);
      drainRAF();

      // Since no dirty components, serializeDirty should NOT be called
      expect(serializeSpy).not.toHaveBeenCalled();
    });
  });
});
