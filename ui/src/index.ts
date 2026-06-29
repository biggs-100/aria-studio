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

  constructor(config: Partial<UIEngineConfig> = {}) {
    this.config = {
      targetFPS: 144,
      enableAnimations: true,
      themeName: 'aria-dark',
      ...config,
    };
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  async initialize(_canvasElement: HTMLCanvasElement): Promise<void> {
    console.log('[ARIA UI] Initializing...', this.config);
  }

  shutdown(): void {
    console.log('[ARIA UI] Shutdown complete');
  }

  getConfig(): UIEngineConfig {
    return { ...this.config };
  }
}
