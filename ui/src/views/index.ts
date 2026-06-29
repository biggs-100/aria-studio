// ARIA DAW — View Definitions
//
// Re-exports all view implementations. The ArrangementView and
// SessionView are imported from their full modules.

export interface View {
  readonly name: string;
  activate(): void;
  deactivate(): void;
  render(): void;
}

export { ArrangementView } from './ArrangementView/index.js';
export { SessionView } from './SessionView/index.js';

export class MixerView implements View {
  readonly name = 'Mixer';

  activate(): void {}
  deactivate(): void {}
  render(): void {}
}
