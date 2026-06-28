// ARIA DAW — TypeScript UI Entry Point
// Declarative UI framework entry.

export * from './components/index.js';
export * from './views/index.js';

export interface UIEngineConfig {
  targetFPS: number;
  enableAnimations: boolean;
  themeName: string;
}

export class UIEngine {
  private config: UIEngineConfig;
  private running = false;

  constructor(config: Partial<UIEngineConfig> = {}) {
    this.config = {
      targetFPS: 144,
      enableAnimations: true,
      themeName: 'aria-dark',
      ...config,
    };
  }

  async initialize(canvasElement: HTMLCanvasElement): Promise<void> {
    console.log('[ARIA UI] Initializing...', this.config);
    this.running = true;
  }

  shutdown(): void {
    this.running = false;
    console.log('[ARIA UI] Shutdown complete');
  }

  getConfig(): UIEngineConfig {
    return { ...this.config };
  }
}
