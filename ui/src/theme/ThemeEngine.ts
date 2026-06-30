// ── ThemeEngine — TS Token Resolver ──────────────────────────────────
//
// Mirrors the C++ ThemeEngine API. Loads theme JSON, builds a flat
// token cache, and resolves dot-path tokens to colors or numbers.
//
// Usage:
//   const engine = new ThemeEngine();
//   engine.load(themeData);
//   const bgColor = engine.resolve('colors.bg.primary');  // {r, g, b, a}
//   const fontSize = engine.resolveFloat('typography.fontSize');  // 14

export interface ThemeData {
  name?: string;
  author?: string;
  version?: string;
  description?: string;
  colors?: Record<string, any>;
  typography?: Record<string, any>;
  spacing?: Record<string, any>;
  components?: Record<string, any>;
  [key: string]: any;
}

export interface ThemeInfo {
  name: string;
  author: string;
  version: string;
  description: string;
  filePath: string;
}

export interface Color {
  r: number;
  g: number;
  b: number;
  a: number;
}

export class ThemeEngine {
  /** Theme metadata. */
  name = '';
  author = '';
  version = '';
  description = '';

  /** Flat token cache: "colors.bg.primary" → "#1A1A1E" */
  private tokenCache = new Map<string, string>();

  /** Color cache: "colors.bg.primary" → {r, g, b, a} */
  private colorCache = new Map<string, Color>();

  /** Float cache: "typography.fontSize" → 14 */
  private floatCache = new Map<string, number>();

  /** Raw theme data for reference. */
  private rawData: ThemeData | null = null;

  // ── Loading ──────────────────────────────────────────────────

  /**
   * Load a theme from a JSON object.
   * Populates the flat token cache and invalidates any previous cache.
   */
  load(data: ThemeData): void {
    this.rawData = data;
    this.tokenCache.clear();
    this.colorCache.clear();
    this.floatCache.clear();

    this.name = data.name ?? '';
    this.author = data.author ?? '';
    this.version = data.version ?? '';
    this.description = data.description ?? '';

    // Build flat token cache by walking the tree recursively
    this.buildCache(data);
  }

  // ── Token Resolution ─────────────────────────────────────────

  /**
   * Resolve a color by dot path (e.g. "colors.bg.primary").
   * Returns a Color object or a fallback (black) if the path is invalid.
   */
  resolve(path: string): Color {
    const cached = this.colorCache.get(path);
    if (cached) return cached;

    const raw = this.resolveToken(path);
    if (!raw) {
      const fallback: Color = { r: 0, g: 0, b: 0, a: 1 };
      this.colorCache.set(path, fallback);
      return fallback;
    }

    const color = this.parseColor(raw);
    this.colorCache.set(path, color);
    return color;
  }

  /**
   * Resolve a float by dot path (e.g. "typography.fontSize").
   * Returns 0 if the path is invalid.
   */
  resolveFloat(path: string): number {
    const cached = this.floatCache.get(path);
    if (cached !== undefined) return cached;

    const raw = this.resolveToken(path);
    if (!raw) {
      this.floatCache.set(path, 0);
      return 0;
    }

    const num = parseFloat(raw);
    const result = isNaN(num) ? 0 : num;
    this.floatCache.set(path, result);
    return result;
  }

  /**
   * Alias for resolve() returning Color.
   */
  resolveColor(path: string): Color | null {
    const cached = this.colorCache.get(path);
    if (cached) return cached;

    const raw = this.resolveToken(path);
    if (!raw) return null;

    const color = this.parseColor(raw);
    this.colorCache.set(path, color);
    return color;
  }

  // ── Theme Enumeration ────────────────────────────────────────

  /**
   * List available built-in themes.
   */
  availableThemes(): ThemeInfo[] {
    return [
      { name: 'ARIA Dark', author: 'ARIA Team', version: '1.0', description: 'Default dark theme — optimized for long sessions', filePath: 'themes/aria-dark.json' },
      { name: 'ARIA Light', author: 'ARIA Team', version: '1.0', description: 'Light theme for bright environments', filePath: 'themes/aria-light.json' },
      { name: 'ARIA High Contrast', author: 'ARIA Team', version: '1.0', description: 'High contrast theme for accessibility', filePath: 'themes/aria-high-contrast.json' },
    ];
  }

  // ── Private ──────────────────────────────────────────────────

  /**
   * Walk the theme data tree and build a flat token cache.
   */
  private buildCache(obj: Record<string, any>, prefix = ''): void {
    for (const [key, value] of Object.entries(obj)) {
      const path = prefix ? `${prefix}.${key}` : key;

      if (value !== null && typeof value === 'object' && !Array.isArray(value)) {
        this.buildCache(value, path);
      } else if (typeof value === 'string') {
        // Resolve any {token} references
        this.tokenCache.set(path, this.resolveReferences(value));
      } else if (typeof value === 'number') {
        this.tokenCache.set(path, String(value));
      }
    }
  }

  /**
   * Resolve a raw token value from the flat cache.
   * Handles {token} references recursively (max depth 10).
   */
  private resolveToken(path: string): string | null {
    const value = this.tokenCache.get(path);
    if (value === undefined) return null;

    // Resolve {token} references
    let result = value;
    let depth = 0;
    while (depth < 10) {
      const match = result.match(/\{([^}]+)\}/);
      if (!match) break;

      const refPath = match[1];
      const refValue = this.tokenCache.get(refPath);
      if (refValue !== undefined) {
        result = result.replace(match[0], refValue);
      } else {
        break;
      }
      depth++;
    }

    return result;
  }

  /**
   * Resolve all {token} references within a string value.
   */
  private resolveReferences(value: string): string {
    let result = value;
    let depth = 0;
    while (depth < 10) {
      const match = result.match(/\{([^}]+)\}/);
      if (!match) break;

      const refPath = match[1];
      const refValue = this.tokenCache.get(refPath);
      if (refValue !== undefined) {
        result = result.replace(match[0], refValue);
      } else {
        break;
      }
      depth++;
    }
    return result;
  }

  /**
   * Parse a color string (#RRGGBB, RRGGBB, #RGB, rgba(r,g,b,a)) into a Color.
   */
  private parseColor(value: string): Color {
    const c: Color = { r: 0, g: 0, b: 0, a: 1 };

    // Handle rgba() syntax
    const rgbaMatch = value.match(/rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+)\s*)?\)/);
    if (rgbaMatch) {
      c.r = parseInt(rgbaMatch[1]) / 255;
      c.g = parseInt(rgbaMatch[2]) / 255;
      c.b = parseInt(rgbaMatch[3]) / 255;
      c.a = rgbaMatch[4] !== undefined ? parseFloat(rgbaMatch[4]) : 1;
      return c;
    }

    // Handle hex colors
    let hex = value.trim();
    if (hex.startsWith('#')) hex = hex.slice(1);

    // Handle short form (#RGB)
    if (hex.length === 3) {
      c.r = parseInt(hex[0] + hex[0], 16) / 255;
      c.g = parseInt(hex[1] + hex[1], 16) / 255;
      c.b = parseInt(hex[2] + hex[2], 16) / 255;
      return c;
    }

    // Handle full form (#RRGGBB or #RRGGBBAA)
    if (hex.length >= 6) {
      c.r = parseInt(hex.substring(0, 2), 16) / 255;
      c.g = parseInt(hex.substring(2, 4), 16) / 255;
      c.b = parseInt(hex.substring(4, 6), 16) / 255;
      if (hex.length >= 8) {
        c.a = parseInt(hex.substring(6, 8), 16) / 255;
      }
    }

    return c;
  }
}
