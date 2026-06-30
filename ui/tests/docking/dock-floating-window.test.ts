// DockFloatingWindow — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect } from 'vitest';
import { createFloatingNode, isInsideWindow } from '../../src/docking/dock-floating-window.js';
import type { DockNode } from '../../src/docking/types.js';

// ── createFloatingNode Tests ────────────────────────────────────────

describe('createFloatingNode', () => {
  it('creates a floating node with position and size', () => {
    const node = createFloatingNode('panel1', 100, 200, 400, 300);
    expect(node.id).toBe('panel1');
    expect(node.type).toBe('float');
    expect(node.panelType).toBe('panel1');
    expect(node.x).toBe(100);
    expect(node.y).toBe(200);
    expect(node.w).toBe(400);
    expect(node.h).toBe(300);
  });

  it('creates a floating node with default position when omitted', () => {
    const node = createFloatingNode('panel1');
    expect(node.x).toBe(100);
    expect(node.y).toBe(100);
    expect(node.w).toBe(400);
    expect(node.h).toBe(300);
  });

  it('assigns a unique id when none provided', () => {
    const node = createFloatingNode('no-id', 0, 0, 100, 100);
    // id should match panelType or be generated
    expect(node.id).toBe('no-id');
  });
});

// ── isInsideWindow Tests ────────────────────────────────────────────

describe('isInsideWindow', () => {
  it('returns true when point is inside the window rect', () => {
    const win: DockNode = {
      id: 'fw', type: 'float', panelType: 'test',
      x: 100, y: 100, w: 400, h: 300,
    };
    expect(isInsideWindow(win, 200, 200)).toBe(true);
    expect(isInsideWindow(win, 100, 100)).toBe(true);
    expect(isInsideWindow(win, 499, 399)).toBe(true);
  });

  it('returns false when point is outside the window rect', () => {
    const win: DockNode = {
      id: 'fw', type: 'float', panelType: 'test',
      x: 100, y: 100, w: 400, h: 300,
    };
    expect(isInsideWindow(win, 50, 200)).toBe(false);
    expect(isInsideWindow(win, 200, 50)).toBe(false);
    expect(isInsideWindow(win, 600, 400)).toBe(false);
    expect(isInsideWindow(win, 500, 400)).toBe(false);
  });
});
