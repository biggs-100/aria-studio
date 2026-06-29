// ARIA DAW — UI Component Library

export interface Component {
  readonly id: string;
  readonly type: string;
  render(): void;
  destroy(): void;
}

export class Button implements Component {
  readonly id: string;
  readonly type = 'Button';

  constructor(id: string, _label: string) {
    this.id = id;
  }

  render(): void {
    // GPU-rendered button placeholder
  }

  destroy(): void {}
}

export class Slider implements Component {
  readonly id: string;
  readonly type = 'Slider';
  private value = 0.5;

  constructor(id: string) {
    this.id = id;
  }

  setValue(v: number): void {
    this.value = Math.max(0, Math.min(1, v));
  }

  getValue(): number {
    return this.value;
  }

  render(): void {}
  destroy(): void {}
}
