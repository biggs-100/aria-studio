// ThemeEngine — Unit Tests
// RED phase: Tests reference ThemeEngine before it exists.
import { describe, it, expect } from 'vitest';
import { ThemeEngine } from '../../src/theme/ThemeEngine.js';

// ── ThemeEngine Tests ──────────────────────────────────────────────

describe('ThemeEngine', () => {
  it('creates a ThemeEngine instance', () => {
    const engine = new ThemeEngine();
    expect(engine).toBeInstanceOf(ThemeEngine);
  });

  it('loads a minimal theme JSON string', () => {
    const engine = new ThemeEngine();
    const json = {
      name: 'Test',
      colors: {
        bg: { primary: '#1A1A1E', secondary: '#2A2A30' },
        text: { primary: '#E0E0E0' },
        accent: { primary: '#4A9EFF' },
      },
      typography: { fontSize: 14, fontFamily: 'Inter' },
      spacing: { padding: 8 },
    };
    engine.load(json);
    expect(engine.name).toBe('Test');
  });

  it('resolveColor parses hex colors correctly', () => {
    const engine = new ThemeEngine();
    engine.load({
      name: 'ColorTest',
      colors: {
        bg: { primary: '#1A1A1E' },
        accent: { primary: '#4A9EFF' },
      },
    });

    const bg = engine.resolve('colors.bg.primary');
    expect(bg).toBeDefined();
    expect(bg.r).toBeCloseTo(0.102, 1); // 0x1A / 255
    expect(bg.g).toBeCloseTo(0.102, 1);
    expect(bg.b).toBeCloseTo(0.118, 1);
    expect(bg.a).toBe(1);

    const accent = engine.resolve('colors.accent.primary');
    expect(accent.r).toBeCloseTo(0.29, 1); // 0x4A / 255
    expect(accent.g).toBeCloseTo(0.62, 1); // 0x9E / 255
    expect(accent.b).toBe(1);              // 0xFF / 255
  });

  it('resolveFloat reads numeric values from theme', () => {
    const engine = new ThemeEngine();
    engine.load({
      name: 'SpacingTest',
      typography: { fontSize: 14 },
      spacing: { padding: 8, unit: 4 },
    });

    expect(engine.resolveFloat('typography.fontSize')).toBe(14);
    expect(engine.resolveFloat('spacing.padding')).toBe(8);
    expect(engine.resolveFloat('spacing.unit')).toBe(4);
  });

  it('resolveFloat returns default for missing path', () => {
    const engine = new ThemeEngine();
    engine.load({ name: 'Empty', colors: {} });
    expect(engine.resolveFloat('typography.nonexistent')).toBe(0);
  });

  it('resolve returns fallback Color for missing paths', () => {
    const engine = new ThemeEngine();
    engine.load({ name: 'Empty', colors: {} });

    const c = engine.resolve('colors.nonexistent.key');
    expect(c.r).toBe(0);
    expect(c.g).toBe(0);
    expect(c.b).toBe(0);
    expect(c.a).toBe(1);
  });

  it('caches resolved tokens for repeated access', () => {
    const engine = new ThemeEngine();
    engine.load({
      name: 'CacheTest',
      colors: { test: { value: '#FF0000' } },
    });

    const c1 = engine.resolve('colors.test.value');
    const c2 = engine.resolve('colors.test.value');
    expect(c1.r).toBe(1);
    expect(c2.r).toBe(1);
  });

  it('re-loading a theme invalidates the cache', () => {
    const engine = new ThemeEngine();
    engine.load({
      name: 'First',
      colors: { bg: { primary: '#FF0000' } },
    });
    expect(engine.resolve('colors.bg.primary').r).toBe(1);

    engine.load({
      name: 'Second',
      colors: { bg: { primary: '#00FF00' } },
    });
    expect(engine.resolve('colors.bg.primary').r).toBe(0);
    expect(engine.resolve('colors.bg.primary').g).toBe(1);
  });

  it('availableThemes returns built-in themes list', () => {
    const engine = new ThemeEngine();
    const themes = engine.availableThemes();
    expect(themes.length).toBeGreaterThanOrEqual(3);
    const names = themes.map(t => t.name);
    expect(names).toContain('ARIA Dark');
    expect(names).toContain('ARIA Light');
    expect(names).toContain('ARIA High Contrast');
  });

  it('handles rgba() syntax in color values', () => {
    const engine = new ThemeEngine();
    engine.load({
      name: 'RGBATest',
      colors: {
        bg: { overlay: 'rgba(0, 0, 0, 0.6)' },
        accent: { glow: 'rgba(255, 122, 0, 0.3)' },
      },
    });

    const overlay = engine.resolve('colors.bg.overlay');
    expect(overlay.r).toBe(0);
    expect(overlay.g).toBe(0);
    expect(overlay.b).toBe(0);
    expect(overlay.a).toBeCloseTo(0.6);

    const glow = engine.resolve('colors.accent.glow');
    expect(glow.r).toBe(1);
    expect(glow.g).toBeCloseTo(0.478, 1);
    expect(glow.b).toBe(0);
    expect(glow.a).toBeCloseTo(0.3);
  });

  it('resolveColor returns Color object for theme colors', () => {
    const engine = new ThemeEngine();
    engine.load({
      name: 'ColorExport',
      colors: {
        meter: { cold: '#4A9EFF' },
      },
    });

    const c = engine.resolveColor('colors.meter.cold');
    expect(c).toBeDefined();
    expect(c.r).toBeCloseTo(0.29, 1);
    expect(c.g).toBeCloseTo(0.62, 1);
    expect(c.b).toBe(1);
  });

  it('resolveColor returns null for missing paths', () => {
    const engine = new ThemeEngine();
    engine.load({ name: 'Empty', colors: {} });
    expect(engine.resolveColor('nothing.here')).toBeNull();
  });
});
