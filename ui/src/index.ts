// ARIA DAW — TypeScript UI Entry Point
// Declarative UI framework entry with FFI bridge to C++ SkiaCanvas.
//
// P10b: UIEngine now owns a Reconciler for dirty-tree scheduling and
// a useGpuComponents toggle to switch between Canvas 2D and GPU rendering.

export * from './components/index.js';
export * from './views/index.js';
export * from './ffi/types.js';
export * from './ffi/commands.js';
export { Bridge } from './ffi/bridge.js';
export type { VNode } from './ffi/bridge.js';

import { Bridge } from './ffi/bridge.js';
import { Reconciler } from './framework/reconciler.js';
import type { EventHandler } from './ffi/types.js';

export interface UIEngineConfig {
  targetFPS: number;
  enableAnimations: boolean;
  themeName: string;
  /** If true, use GPU-backed component rendering instead of Canvas 2D. */
  useGpuComponents: boolean;
}

const DEFAULT_CONFIG: UIEngineConfig = {
  targetFPS: 144,
  enableAnimations: true,
  themeName: 'aria-dark',
  useGpuComponents: false,
};

/**
 * UIEngine — entry point for the TypeScript UI layer.
 *
 * Owns the FFI bridge instance used to serialize component trees
 * into SkiaCanvas draw commands and receive input events from C++.
 *
 * P10b+: Also owns a Reconciler for dirty-tree scheduling that drives
 * serialization through the bridge. The useGpuComponents toggle switches
 * between traditional Canvas 2D and the GPU component pipeline.
 */
export class UIEngine {
  private config: UIEngineConfig;
  private bridge: Bridge;
  private _reconciler: Reconciler;
  private _useGpuComponents: boolean;

  constructor(config: Partial<UIEngineConfig> = {}) {
    this.config = { ...DEFAULT_CONFIG, ...config };
    this._useGpuComponents = this.config.useGpuComponents;
    this.bridge = new Bridge();
    this._reconciler = new Reconciler();

    // Wire bridge into reconciler for dirty-tree serialization
    this._reconciler.setBridge(this.bridge);
  }

  async initialize(_canvasElement: HTMLCanvasElement): Promise<void> {
    console.log('[ARIA UI] Initializing...', this.config);

    if (this._useGpuComponents) {
      // Start the component render loop
      this._reconciler.start();
    }

    console.log('[ARIA UI] FFI Bridge ready');
  }

  shutdown(): void {
    this._reconciler.stop();
    this.bridge.offEvent();
    console.log('[ARIA UI] Shutdown complete');
  }

  getConfig(): UIEngineConfig {
    return { ...this.config };
  }

  /** Access the FFI bridge for serialisation and execution. */
  getBridge(): Bridge {
    return this.bridge;
  }

  /** Access the Reconciler for dirty-tree management. */
  getReconciler(): Reconciler {
    return this._reconciler;
  }

  /** Register an input event handler from C++. */
  onEvent(handler: EventHandler): void {
    this.bridge.onEvent(handler);
  }
}
