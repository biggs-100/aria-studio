// ARIA DAW — View Definitions

export interface View {
  readonly name: string;
  activate(): void;
  deactivate(): void;
  render(): void;
}

export class ArrangementView implements View {
  readonly name = 'Arrangement';

  activate(): void {}
  deactivate(): void {}
  render(): void {}
}

export class SessionView implements View {
  readonly name = 'Session';

  activate(): void {}
  deactivate(): void {}
  render(): void {}
}

export class MixerView implements View {
  readonly name = 'Mixer';

  activate(): void {}
  deactivate(): void {}
  render(): void {}
}
