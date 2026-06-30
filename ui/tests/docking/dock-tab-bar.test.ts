// DockTabBar — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect } from 'vitest';
import { reorderTabs, isDetachEvent } from '../../src/docking/dock-tab-bar.js';

// ── reorderTabs Tests ───────────────────────────────────────────────

describe('reorderTabs', () => {
  it('swaps tab order when dragged left past another tab', () => {
    // [Mixer, Browser, Arrangement] — drag Browser (idx 1) left past Mixer
    const result = reorderTabs(['mixer', 'browser', 'arrangement'], 1, 0);
    expect(result).toEqual(['browser', 'mixer', 'arrangement']);
  });

  it('swaps tab order when dragged right past another tab', () => {
    // [Mixer, Browser, Arrangement] — drag Mixer (idx 0) right past Browser
    const result = reorderTabs(['mixer', 'browser', 'arrangement'], 0, 1);
    expect(result).toEqual(['browser', 'mixer', 'arrangement']);
  });

  it('moves tab to end when dragged to last position', () => {
    // [Mixer, Browser, Arrangement] — drag Mixer to end
    const result = reorderTabs(['mixer', 'browser', 'arrangement'], 0, 2);
    expect(result).toEqual(['browser', 'arrangement', 'mixer']);
  });

  it('moves tab to start when dragged to first position', () => {
    const result = reorderTabs(['mixer', 'browser', 'arrangement'], 2, 0);
    expect(result).toEqual(['arrangement', 'mixer', 'browser']);
  });

  it('returns same array when fromIndex equals toIndex', () => {
    const result = reorderTabs(['a', 'b', 'c'], 1, 1);
    expect(result).toEqual(['a', 'b', 'c']);
  });

  it('returns same array for single-element list', () => {
    const result = reorderTabs(['only'], 0, 0);
    expect(result).toEqual(['only']);
  });

  it('drag to index 0 moves element to front', () => {
    const result = reorderTabs(['a', 'b', 'c', 'd'], 3, 0);
    expect(result).toEqual(['d', 'a', 'b', 'c']);
  });
});

// ── isDetachEvent Tests ──────────────────────────────────────────────

describe('isDetachEvent', () => {
  it('detects drag outside tab bar bounds to the left', () => {
    // offsetX < 0 means dragged outside left edge
    const result = isDetachEvent({ offsetX: -50, barWidth: 500 });
    expect(result).toBe(true);
  });

  it('detects drag outside tab bar bounds to the right', () => {
    // offsetX > barWidth means dragged outside right edge
    const result = isDetachEvent({ offsetX: 550, barWidth: 500 });
    expect(result).toBe(true);
  });

  it('does not detach when drag is within bounds', () => {
    // offsetX within [0, barWidth]
    const result = isDetachEvent({ offsetX: 100, barWidth: 500 });
    expect(result).toBe(false);
  });

  it('does not detach when offsetX equals zero (edge of bar)', () => {
    const result = isDetachEvent({ offsetX: 0, barWidth: 500 });
    expect(result).toBe(false);
  });

  it('does not detach when offsetX equals barWidth (far edge)', () => {
    const result = isDetachEvent({ offsetX: 500, barWidth: 500 });
    expect(result).toBe(false);
  });
});
