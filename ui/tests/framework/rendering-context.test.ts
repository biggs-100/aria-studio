// RenderingContext — Unit Tests
// RED phase: Tests reference RenderingContext before it exists.
import { describe, it, expect } from 'vitest';
import { RenderingContext } from '../../src/framework/rendering-context.js';

describe('RenderingContext', () => {
  it('provides default values when created without arguments', () => {
    const ctx = new RenderingContext();
    expect(ctx.width).toBe(0);
    expect(ctx.height).toBe(0);
    expect(ctx.availableWidth).toBe(0);
    expect(ctx.availableHeight).toBe(0);
    expect(ctx.dpr).toBe(1);
  });

  it('stores constructor-assigned values', () => {
    const ctx = new RenderingContext(800, 600, 2);
    expect(ctx.width).toBe(800);
    expect(ctx.height).toBe(600);
    expect(ctx.availableWidth).toBe(800);
    expect(ctx.availableHeight).toBe(600);
    expect(ctx.dpr).toBe(2);
  });

  it('creates child context from parent with inherited DPR', () => {
    const parent = new RenderingContext(1024, 768, 1.5);
    const child = parent.derive(400, 300);

    expect(child.width).toBe(400);
    expect(child.height).toBe(300);
    expect(child.dpr).toBe(1.5);           // inherited from parent
    expect(child.availableWidth).toBe(400);
    expect(child.availableHeight).toBe(300);
  });

  it('parent DPR defaults when not set', () => {
    const parent = new RenderingContext(500, 500);
    const child = parent.derive(200, 100);
    expect(child.dpr).toBe(1);              // default from parent
  });

  it('allChildrenUsed subtracts consumed space from available', () => {
    const ctx = new RenderingContext(800, 600, 1);

    // Simulate two children placed side by side, each taking 300px
    ctx.useWidth(300);
    ctx.useWidth(300);

    // Available should be 800 - 600 = 200
    expect(ctx.availableWidth).toBe(200);
    expect(ctx.availableHeight).toBe(600);   // unchanged
  });

  it('tracks used height for column layouts', () => {
    const ctx = new RenderingContext(400, 900, 1);

    ctx.useHeight(100);
    ctx.useHeight(200);

    expect(ctx.availableWidth).toBe(400);    // unchanged
    expect(ctx.availableHeight).toBe(600);   // 900 - 100 - 200
  });

  it('reset clears used space tracking', () => {
    const ctx = new RenderingContext(300, 300, 1);
    ctx.useWidth(100);
    ctx.useHeight(50);

    expect(ctx.availableWidth).toBe(200);
    expect(ctx.availableHeight).toBe(250);

    ctx.reset();

    expect(ctx.availableWidth).toBe(300);
    expect(ctx.availableHeight).toBe(300);
  });

  it('available never goes below zero', () => {
    const ctx = new RenderingContext(100, 100, 1);
    ctx.useWidth(200);
    ctx.useHeight(200);

    expect(ctx.availableWidth).toBe(0);
    expect(ctx.availableHeight).toBe(0);
  });
});
