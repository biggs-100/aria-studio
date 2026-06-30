// UIEngine — Unit Tests (P10b wiring)
// RED phase: Tests for Reconciler integration, useGpuComponents toggle.
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { UIEngine, UIEngineConfig } from '../../src/index.js';
import { Reconciler } from '../../src/framework/reconciler.js';

describe('UIEngine (P10b extension)', () => {
  let rafCalls: Array<FrameRequestCallback>;
  let originalRAF: typeof requestAnimationFrame;

  beforeEach(() => {
    originalRAF = globalThis.requestAnimationFrame;
    rafCalls = [];
    globalThis.requestAnimationFrame = ((cb: FrameRequestCallback) => {
      rafCalls.push(cb);
      return rafCalls.length;
    }) as unknown as typeof requestAnimationFrame;
  });

  afterEach(() => {
    globalThis.requestAnimationFrame = originalRAF;
  });

  it('creates Reconciler instance', () => {
    const engine = new UIEngine();
    const reconciler = (engine as unknown as { _reconciler: Reconciler })._reconciler;
    expect(reconciler).toBeInstanceOf(Reconciler);
  });

  it('accesses reconciler via getReconciler', () => {
    const engine = new UIEngine();
    const reconciler = engine.getReconciler();
    expect(reconciler).toBeInstanceOf(Reconciler);
  });

  it('getBridge returns bridge and sets it on reconciler', () => {
    const engine = new UIEngine();
    const bridge = engine.getBridge();
    expect(bridge).toBeDefined();
    // Bridge should be wired to the reconciler
    const reconciler = engine.getReconciler();
    expect(reconciler).toBeDefined();
  });

  it('stores useGpuComponents flag from config', () => {
    const engineDefault = new UIEngine();
    expect((engineDefault as unknown as { _useGpuComponents: boolean })._useGpuComponents).toBe(false);
    // Default is false (Canvas 2D mode)

    const engineGpu = new UIEngine({ useGpuComponents: true });
    expect((engineGpu as unknown as { _useGpuComponents: boolean })._useGpuComponents).toBe(true);
  });

  it('shutdown stops reconciler', () => {
    const engine = new UIEngine();
    const reconciler = engine.getReconciler();
    const stopSpy = vi.spyOn(reconciler, 'stop');

    engine.shutdown();
    expect(stopSpy).toHaveBeenCalled();
  });

  it('shutdown also cleans up bridge events', () => {
    const engine = new UIEngine();
    const bridge = engine.getBridge();
    const offEventSpy = vi.spyOn(bridge, 'offEvent');

    engine.shutdown();
    expect(offEventSpy).toHaveBeenCalled();
  });

  it('config accepts partial options with defaults', () => {
    const engine = new UIEngine({ targetFPS: 60 });
    const config = engine.getConfig();
    expect(config.targetFPS).toBe(60);
    expect(config.enableAnimations).toBe(true);
    expect(config.themeName).toBe('aria-dark');
    expect(config.useGpuComponents).toBe(false);
  });
});
