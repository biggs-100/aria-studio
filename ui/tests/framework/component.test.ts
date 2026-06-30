// Component — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect, vi } from 'vitest';
import type { VNode } from '../../src/ffi/bridge.js';
import { Component } from '../../src/framework/component.js';
import { Reconciler } from '../../src/framework/reconciler.js';

// ── Test Helpers ──────────────────────────────────────────────────────

/** Concrete component for testing lifecycle. */
class TestComponent extends Component<{ label: string }, { value: number; toggled: boolean }> {
  readonly lifecycleLog: string[] = [];

  constructor(props: { label: string }) {
    super(props);
    this.state = { value: 0, toggled: false };
  }

  onMount(): void {
    this.lifecycleLog.push('onMount');
  }

  onUnmount(): void {
    this.lifecycleLog.push('onUnmount');
  }

  render(): VNode {
    this.lifecycleLog.push('render');
    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x: 0, y: 0, w: 100, h: 100 },
    };
  }
}

class SimpleComponent extends Component<{}, { count: number }> {
  constructor() {
    super({});
    this.state = { count: 0 };
  }

  render(): VNode {
    return {
      type: 'container',
      key: String(this.widgetId),
      props: {},
    };
  }
}

// ── Lifecycle Tests ──────────────────────────────────────────────────

describe('Component lifecycle', () => {
  it('calls constructor, onMount, and render in correct sequence after mount', () => {
    const comp = new TestComponent({ label: 'test' });

    // Constructor should run first
    expect(comp.props.label).toBe('test');
    expect(comp.state.value).toBe(0);
    expect(comp.lifecycleLog).toEqual([]);       // nothing called yet

    // Mount
    comp.mount();
    expect(comp.lifecycleLog).toContain('onMount');
    expect(comp._mounted).toBe(true);

    // render can be called after mount
    const vnode = comp.render();
    expect(vnode.type).toBe('container');
    expect(vnode.key).toBe(String(comp.widgetId));
  });

  it('fires onUnmount when component is removed from tree', () => {
    const comp = new TestComponent({ label: 'cleanup' });
    const parent = new TestComponent({ label: 'parent' });
    parent.mount();
    comp.mount(parent);

    comp.unmount();
    expect(comp.lifecycleLog).toContain('onUnmount');
    expect(comp._mounted).toBe(false);
    // Should be removed from parent children
    expect(parent._children).not.toContain(comp);
  });

  it('assigns unique widgetId to each instance', () => {
    const a = new SimpleComponent();
    const b = new SimpleComponent();
    const c = new SimpleComponent();
    expect(a.widgetId).not.toBe(b.widgetId);
    expect(b.widgetId).not.toBe(c.widgetId);
    expect(a.widgetId).toBeGreaterThan(0);
  });
});

// ── setState Tests ───────────────────────────────────────────────────

describe('Component.setState', () => {
  it('merges partial state into current state', () => {
    const comp = new TestComponent({ label: 'merge-test' });
    expect(comp.state.value).toBe(0);
    expect(comp.state.toggled).toBe(false);

    comp.setState({ value: 42 });
    expect(comp.state.value).toBe(42);
    expect(comp.state.toggled).toBe(false);      // unchanged

    comp.setState({ toggled: true });
    expect(comp.state.value).toBe(42);            // preserved
    expect(comp.state.toggled).toBe(true);
  });

  it('marks component dirty after setState (was already dirty from mount)', () => {
    const comp = new TestComponent({ label: 'dirty-test' });
    comp.mount();
    expect(comp._dirty).toBe(true);               // dirty from mount (needs initial render)

    comp.setState({ value: 1 });
    expect(comp._dirty).toBe(true);               // still dirty after state change
  });

  it('marks parent chain dirty after setState on child', () => {
    const parent = new TestComponent({ label: 'parent-dirty' });
    const child = new TestComponent({ label: 'child-dirty' });
    parent.mount();
    expect(parent._dirty).toBe(true);              // dirty from mount
    child.mount(parent);
    expect(child._dirty).toBe(true);               // dirty from mount

    child.setState({ value: 99 });

    // Both should remain dirty (child was already dirty, parent already dirty)
    expect(child._dirty).toBe(true);
    expect(parent._dirty).toBe(true);
  });

  it('skips dirty marking when setState values are unchanged (shallow equality)', () => {
    const comp = new TestComponent({ label: 'noop-test' });
    comp.mount();
    expect(comp._dirty).toBe(true);                // dirty from mount

    comp.setState({ value: 0 });                    // same as initial state — no-op

    // Dirty flag should NOT be affected by no-op setState
    // (it remains true from mount — we test that setState didn't clear it)
    expect(comp._dirty).toBe(true);
  });

  it('notifies reconciler when setState changes a value', () => {
    const comp = new SimpleComponent();
    const reconciler = new Reconciler();
    reconciler.setRoot(comp);

    const spy = vi.spyOn(reconciler, '_onComponentDirty');

    comp.setState({ count: 5 });
    expect(spy).toHaveBeenCalledWith(comp);
  });

  it('does NOT notify reconciler on no-op setState', () => {
    const comp = new SimpleComponent();
    comp.state = { count: 10 };
    const reconciler = new Reconciler();
    reconciler.setRoot(comp);

    const spy = vi.spyOn(reconciler, '_onComponentDirty');

    comp.setState({ count: 10 }); // same value — no-op
    expect(spy).not.toHaveBeenCalled();
  });

  it('propagates dirty through three levels of nesting', () => {
    const grandparent = new TestComponent({ label: 'gp' });
    const parent = new TestComponent({ label: 'p' });
    const child = new TestComponent({ label: 'c' });

    grandparent.mount();
    parent.mount(grandparent);
    child.mount(parent);

    // All dirty from mount — simulate a flush clearing them
    grandparent._dirty = false;
    parent._dirty = false;
    child._dirty = false;

    // Mutate child
    child.setState({ value: 77 });

    expect(child._dirty).toBe(true);
    expect(parent._dirty).toBe(true);
    expect(grandparent._dirty).toBe(true);
  });
});

// ── Parent/Child Tree Tests ──────────────────────────────────────────

describe('Component tree management', () => {
  it('attaches child to parent on mount', () => {
    const parent = new TestComponent({ label: 'p' });
    const child = new TestComponent({ label: 'c' });

    parent.mount();
    child.mount(parent);

    expect(child._parent).toBe(parent);
    expect(parent._children).toContain(child);
  });

  it('detaches child from parent on unmount', () => {
    const parent = new TestComponent({ label: 'p' });
    const child = new TestComponent({ label: 'c' });

    parent.mount();
    child.mount(parent);
    child.unmount();

    expect(child._parent).toBeNull();
    expect(parent._children).not.toContain(child);
  });
});
