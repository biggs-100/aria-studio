// DockSplitPanel — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect } from 'vitest';
import { calculateResize } from '../../src/docking/dock-split-panel.js';

// ── calculateResize Tests ────────────────────────────────────────────

describe('calculateResize', () => {
  it('reduces left panel and grows right panel when divider dragged left', () => {
    // Total width 500, initial sizes [250, 250] (50/50)
    // Divider dragged 50px left → left shrinks, right grows
    const result = calculateResize(500, [250, 250], 0, -50, 100);
    expect(result[0]).toBe(200);
    expect(result[1]).toBe(300);
  });

  it('reduces right panel and grows left panel when divider dragged right', () => {
    const result = calculateResize(500, [250, 250], 0, 50, 100);
    expect(result[0]).toBe(300);
    expect(result[1]).toBe(200);
  });

  it('enforces min-size clamp at 100px for left panel', () => {
    // Trying to drag left panel from 250 to 50 — should clamp at 100
    const result = calculateResize(500, [250, 250], 0, -200, 100);
    expect(result[0]).toBe(100);
    expect(result[1]).toBe(400);
  });

  it('enforces min-size clamp at 100px for right panel', () => {
    const result = calculateResize(500, [250, 250], 0, 200, 100);
    expect(result[0]).toBe(400);
    expect(result[1]).toBe(100);
  });

  it('does not move divider when both panels would be below min', () => {
    // Both at 100 already, can't resize further
    const result = calculateResize(200, [100, 100], 0, -50, 100);
    expect(result[0]).toBe(100);
    expect(result[1]).toBe(100);
  });

  it('handles vertical split the same way by index', () => {
    // Vertical: same logic but on height instead of width
    const result = calculateResize(600, [300, 300], 0, -100, 100);
    expect(result[0]).toBe(200);
    expect(result[1]).toBe(400);
  });

  it('resizes correct index when divider index is non-zero', () => {
    // Three panels: [200, 200, 200] at total 600
    // Drag divider at index 1 (between panel 1 and panel 2)
    const result = calculateResize(600, [200, 200, 200], 1, -50, 100);
    expect(result[0]).toBe(200); // unchanged
    expect(result[1]).toBe(150); // shrinks
    expect(result[2]).toBe(250); // grows
  });

  it('handles divider at last index correctly', () => {
    // Three panels: [200, 200, 200]
    // Last valid divider index is 1 (between panel[1] and panel[2])
    // delta=50 (drag right) → left panel grows, right panel shrinks
    const result = calculateResize(600, [200, 200, 200], 1, 50, 100);
    expect(result[0]).toBe(200); // unchanged (left of the divider)
    expect(result[1]).toBe(250); // grows (left side of divider 1)
    expect(result[2]).toBe(150); // shrinks (right side of divider 1)
  });

  it('returns unchanged sizes when there is only one panel', () => {
    const result = calculateResize(400, [400], 0, -100, 100);
    expect(result).toEqual([400]);
  });
});
