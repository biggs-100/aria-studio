// DockManager — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect, beforeEach } from 'vitest';
import { DockManager } from '../../src/docking/dock-manager.js';
import type { DockNode, DockLayout, DockLocation } from '../../src/docking/types.js';

// ── Helpers ─────────────────────────────────────────────────────────

function createLayout(overrides?: Partial<DockLayout>): DockLayout {
  return {
    root: { id: 'root', type: 'leaf' },
    floating: [],
    ...overrides,
  };
}

// ── openPanel Tests ─────────────────────────────────────────────────

describe('DockManager.openPanel', () => {
  let dm: DockManager;

  beforeEach(() => {
    dm = new DockManager();
  });

  it('places panel at center leaf when root is empty', () => {
    dm.openPanel('mixer', 'center');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('leaf');
    expect(layout.root.panelType).toBe('mixer');
  });

  it('converts leaf to tab when opening second panel at center', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'center');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('tab');
    expect(layout.root.tabs).toEqual(['mixer', 'browser']);
    expect(layout.root.activeTab).toBe(1);
  });

  it('appends to existing tab group when opening at center', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'center');
    dm.openPanel('arrangement', 'center');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('tab');
    expect(layout.root.tabs).toEqual(['mixer', 'browser', 'arrangement']);
  });

  it('creates horizontal split when opening at right', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'right');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('split');
    expect(layout.root.direction).toBe('horizontal');
    expect(layout.root.children).toHaveLength(2);
    expect(layout.root.sizes).toEqual([0.5, 0.5]);
  });

  it('creates vertical split when opening at bottom', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'bottom');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('split');
    expect(layout.root.direction).toBe('vertical');
    expect(layout.root.children).toHaveLength(2);
  });

  it('places new panel as first child when opening at left', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'left');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('split');
    expect(layout.root.children![0].panelType).toBe('browser');
    expect(layout.root.children![1].type).toBe('leaf');
  });

  it('places new panel as last child when opening at right', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'right');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('split');
    expect(layout.root.children![0].panelType).toBe('mixer');
    expect(layout.root.children![1].panelType).toBe('browser');
  });
});

// ── closePanel Tests ────────────────────────────────────────────────

describe('DockManager.closePanel', () => {
  let dm: DockManager;

  beforeEach(() => {
    dm = new DockManager();
  });

  it('removes panel from tab group, converting single-tab to leaf', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'center');
    dm.closePanel('mixer');
    const layout = dm.saveLayout();
    // Single tab remaining — tab group collapses to leaf
    expect(layout.root.type).toBe('leaf');
    expect(layout.root.panelType).toBe('browser');
  });

  it('resets to empty leaf when last panel closed from tab', () => {
    dm.openPanel('mixer', 'center');
    dm.closePanel('mixer');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('leaf');
    expect(layout.root.panelType).toBeUndefined();
  });

  it('removes panel from split container, collapsing split to leaf', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'right');
    dm.closePanel('browser');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('leaf');
    expect(layout.root.panelType).toBe('mixer');
  });

  it('does not affect other panels when closing one', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'right');
    dm.closePanel('mixer');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('leaf');
    expect(layout.root.panelType).toBe('browser');
  });
});

// ── Panel Registry Tests ────────────────────────────────────────────

describe('DockManager panel registry', () => {
  let dm: DockManager;

  beforeEach(() => {
    dm = new DockManager();
  });

  it('registers and creates panel via factory', () => {
    dm.registerPanel('test-panel', () => 'test-panel-rendered');
    dm.openPanel('my-panel', 'center');
    const layout = dm.saveLayout();
    expect(layout.root.panelType).toBe('my-panel');
  });
});

// ── saveLayout / loadLayout Tests ───────────────────────────────────

describe('DockManager layout serialization', () => {
  let dm: DockManager;

  beforeEach(() => {
    dm = new DockManager();
  });

  it('saveLayout returns current layout state', () => {
    dm.openPanel('mixer', 'center');
    const layout = dm.saveLayout();
    expect(layout.root.type).toBe('leaf');
    expect(layout.root.panelType).toBe('mixer');
    expect(Array.isArray(layout.floating)).toBe(true);
  });

  it('round-trips layout correctly', () => {
    // Register all panel types so loadLayout works
    dm.registerPanel('mixer', () => 'mixer-panel');
    dm.registerPanel('browser', () => 'browser-panel');
    dm.registerPanel('arrangement', () => 'arrangement-panel');
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'right');
    dm.openPanel('arrangement', 'center');

    const json = dm.saveLayout();
    const dm2 = new DockManager();
    dm2.registerPanel('mixer', () => 'mixer-panel');
    dm2.registerPanel('browser', () => 'browser-panel');
    dm2.registerPanel('arrangement', () => 'arrangement-panel');
    dm2.loadLayout(json);
    const restored = dm2.saveLayout();

    // Round-trip should preserve structure
    expect(restored.root.type).toBe(json.root.type);
    expect(restored.root.direction).toBe(json.root.direction);
    expect(restored.root.sizes).toEqual(json.root.sizes);
    // Tab group preserved: find a leaf with 'mixer'
    const findLeaf = (n: DockNode): DockNode | null => {
      if (n.type === 'leaf' && n.panelType === 'mixer') return n;
      if (n.children) { for (const c of n.children) { const r = findLeaf(c); if (r) return r; } }
      return null;
    };
    expect(findLeaf(restored.root)).not.toBeNull();
  });

  it('loadLayout replaces current layout', () => {
    dm.registerPanel('browser', () => 'browser-panel');
    dm.openPanel('mixer', 'center');
    const newLayout: DockLayout = {
      root: { id: 'root', type: 'leaf', panelType: 'browser' },
      floating: [],
    };
    dm.loadLayout(newLayout);
    const layout = dm.saveLayout();
    expect(layout.root.panelType).toBe('browser');
  });

  it('replaces missing/unregistered panel type with placeholder', () => {
    const layout: DockLayout = {
      root: { id: 'root', type: 'leaf', panelType: 'unknown-missing-panel' },
      floating: [],
    };
    const dm2 = new DockManager();
    dm2.loadLayout(layout);
    const saved = dm2.saveLayout();
    expect(saved.root.panelType).toBe('placeholder');
  });
});

// ── Floating Window Tests ──────────────────────────────────────────

describe('DockManager floating windows', () => {
  let dm: DockManager;

  beforeEach(() => {
    dm = new DockManager();
  });

  it('floats a panel by creating a floating window entry', () => {
    dm.openPanel('mixer', 'center');
    dm.floatPanel('mixer', { x: 200, y: 150, w: 400, h: 300 });
    const layout = dm.saveLayout();
    expect(layout.floating).toHaveLength(1);
    expect(layout.floating[0].panelType).toBe('mixer');
    expect(layout.floating[0].x).toBe(200);
    expect(layout.floating[0].y).toBe(150);
    expect(layout.floating[0].w).toBe(400);
    expect(layout.floating[0].h).toBe(300);
  });

  it('reattaches a floating window back to the layout', () => {
    dm.openPanel('mixer', 'center');
    dm.openPanel('browser', 'center');
    dm.floatPanel('browser', { x: 100, y: 100, w: 300, h: 200 });
    dm.reattachPanel('browser', 'center');
    const layout = dm.saveLayout();
    expect(layout.floating).toHaveLength(0);
    expect(layout.root.type).toBe('tab');
    // Browser should be back in the tabs
    expect(layout.root.tabs).toContain('browser');
  });
});
