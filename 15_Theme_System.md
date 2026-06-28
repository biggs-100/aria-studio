# ARIA DAW — Theme System Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Format**: JSON-based, completely customizable

---

## Table of Contents

1. [Overview](#1-overview)
2. [Theme Model](#2-theme-model)
3. [Color System](#3-color-system)
4. [Typography](#4-typography)
5. [Spacing & Layout](#5-spacing--layout)
6. [Component Styles](#6-component-styles)
7. [Animation Timing](#7-animation-timing)
8. [Icon System](#8-icon-system)
9. [Theme Creation](#9-theme-creation)
10. [Built-in Themes](#10-built-in-themes)
11. [Live Reload](#11-live-reload)
12. [API Reference](#12-api-reference)
13. [RFC Index](#13-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Theme System provides complete visual customization of ARIA DAW. Every visual property — colors, spacing, typography, animations, component styles — is defined in a JSON theme file. Users can create, share, and switch themes without modifying any code.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Customizability | Every visual property themable |
| Performance | Theme switch in < 100ms |
| Format | JSON, human-editable |
| Distribution | Import/export single file |
| Scope | Colors, typography, spacing, animations, icons, component variants |

### 1.3. Theme File

```json
{
  "$schema": "https://aria-daw.com/theme-schema-v1.json",
  "name": "ARIA Dark",
  "author": "ARIA Team",
  "version": "1.0.0",
  "description": "Default dark theme for ARIA DAW",
  
  "colors": { ... },
  "typography": { ... },
  "spacing": { ... },
  "components": { ... },
  "animations": { ... },
  "icons": { ... }
}
```

---

## 2. Theme Model

### 2.1. Theme Structure

```typescript
interface Theme {
  // Metadata
  name: string;
  author: string;
  version: string;
  description?: string;
  base?: string;            // Inherit from another theme
  
  // Core tokens
  colors: ColorPalette;
  typography: TypographySettings;
  spacing: SpacingSystem;
  
  // Component overrides
  components: ComponentStyles;
  
  // Animation settings
  animations: AnimationTheme;
  
  // Icons
  icons: IconTheme;
}
```

### 2.2. Theme Inheritance

```typescript
// Themes can inherit from other themes:
// 1. Base theme defines core tokens
// 2. Child theme overrides specific values
// 3. Undefined values fall through to parent
//
// Example:
// "ARIA Dark Blue" inherits from "ARIA Dark"
// and overrides only accent colors

interface ThemeInheritance {
  base: string;           // Parent theme name
  overrides: Partial<Theme>;
}
```

---

## 3. Color System

### 3.1. Color Palette

```json
{
  "colors": {
    "schema": "1.0",
    
    "bg": {
      "primary": "#1A1A1A",
      "secondary": "#222222",
      "tertiary": "#2A2A2A",
      "surface": "#333333",
      "elevated": "#3D3D3D",
      "tooltip": "#4A4A4A",
      "modal_overlay": "rgba(0, 0, 0, 0.6)"
    },
    
    "text": {
      "primary": "#FFFFFF",
      "secondary": "#BBBBBB",
      "tertiary": "#888888",
      "disabled": "#555555",
      "inverse": "#1A1A1A",
      "link": "#4A9EFF",
      "error": "#FF4A4A",
      "warning": "#FFAA00",
      "success": "#4AFF4A"
    },
    
    "accent": {
      "primary": "#FF7A00",
      "secondary": "#FF9A3A",
      "tertiary": "#CC6200",
      "hover": "#FF8C1A",
      "pressed": "#E66E00",
      "muted": "rgba(255, 122, 0, 0.15)",
      "glow": "rgba(255, 122, 0, 0.3)"
    },
    
    "border": {
      "primary": "#3D3D3D",
      "secondary": "#333333",
      "focus": "#FF7A00",
      "error": "#FF4A4A",
      "selected": "#4A9EFF"
    },
    
    "track": {
      "default": "#4A9EFF",
      "audio": "#4AFF7A",
      "midi": "#FFAA4A",
      "group": "#FF7A7A",
      "vca": "#FF7AFF",
      "return": "#7AFFAA",
      "master": "#FFFFFF",
      "automation": "#FF7A00"
    },
    
    "note": {
      "velocity_low": "#4A9EFF",
      "velocity_mid": "#7AFF7A",
      "velocity_high": "#FF7A4A",
      "ghost": "rgba(255, 255, 255, 0.15)",
      "selected_outline": "#FFFFFF",
      "muted_pattern": "rgba(255, 255, 255, 0.1)"
    },
    
    "meter": {
      "cold": "#4A9EFF",
      "warm": "#7AFF7A",
      "hot": "#FFAA00",
      "critical": "#FF4A4A",
      "clip": "#FF0000",
      "background": "#1A1A1A"
    },
    
    "waveform": {
      "fill": "#4A9EFF",
      "outline": "#7AC0FF",
      "background": "#1A1A1A",
      "grid": "#2A2A2A"
    },
    
    "scrollbar": {
      "thumb": "#555555",
      "thumb_hover": "#666666",
      "track": "transparent"
    },
    
    "selection": {
      "fill": "rgba(74, 158, 255, 0.15)",
      "outline": "#4A9EFF",
      "text": "#FFFFFF"
    },
    
    "drag": {
      "preview": "rgba(255, 122, 0, 0.3)",
      "target_indicator": "#FF7A00",
      "ghost": "rgba(255, 255, 255, 0.2)"
    }
  }
}
```

### 3.2. Color Roles

```typescript
// Semantic color roles (used by components):

interface ColorRoles {
  // Background hierarchy
  background: Color;        // Primary bg
  surface: Color;           // Card/panel bg
  elevated: Color;          // Elevated surface (dropdown, popover)
  
  // Text hierarchy
  textPrimary: Color;       // Main content
  textSecondary: Color;     // Less important
  textTertiary: Color;      // Metadata, labels
  textDisabled: Color;      // Disabled state
  
  // Interactive
  accent: Color;            // Primary action
  accentHover: Color;
  accentPressed: Color;
  
  // Feedback
  success: Color;
  warning: Color;
  error: Color;
  info: Color;
  
  // Structural
  border: Color;
  borderFocus: Color;
  divider: Color;
}
```

---

## 4. Typography

### 4.1. Font Configuration

```json
{
  "typography": {
    "font_family": {
      "ui": "Inter, -apple-system, sans-serif",
      "mono": "JetBrains Mono, SF Mono, monospace",
      "display": "Inter, -apple-system, sans-serif"
    },
    
    "font_weight": {
      "light": 300,
      "regular": 400,
      "medium": 500,
      "semibold": 600,
      "bold": 700
    },
    
    "font_size": {
      "xs": 10,
      "sm": 11,
      "base": 12,
      "md": 13,
      "lg": 14,
      "xl": 16,
      "2xl": 18,
      "3xl": 24,
      "4xl": 32
    },
    
    "line_height": {
      "tight": 1.2,
      "normal": 1.4,
      "relaxed": 1.6
    },
    
    "letter_spacing": {
      "tight": -0.02,
      "normal": 0,
      "wide": 0.05
    },
    
    "text_styles": {
      "h1": {
        "fontFamily": "display",
        "fontSize": "4xl",
        "fontWeight": "bold",
        "lineHeight": "tight",
        "letterSpacing": "tight"
      },
      "h2": {
        "fontFamily": "display",
        "fontSize": "3xl",
        "fontWeight": "semibold",
        "lineHeight": "tight"
      },
      "h3": {
        "fontFamily": "display",
        "fontSize": "2xl",
        "fontWeight": "semibold"
      },
      "body": {
        "fontFamily": "ui",
        "fontSize": "base",
        "fontWeight": "regular",
        "lineHeight": "normal"
      },
      "body_small": {
        "fontFamily": "ui",
        "fontSize": "sm",
        "fontWeight": "regular"
      },
      "caption": {
        "fontFamily": "ui",
        "fontSize": "xs",
        "fontWeight": "regular",
        "letterSpacing": "wide"
      },
      "mono": {
        "fontFamily": "mono",
        "fontSize": "base",
        "fontWeight": "regular"
      },
      "label": {
        "fontFamily": "ui",
        "fontSize": "sm",
        "fontWeight": "medium",
        "letterSpacing": "wide"
      },
      "button": {
        "fontFamily": "ui",
        "fontSize": "base",
        "fontWeight": "medium"
      }
    }
  }
}
```

---

## 5. Spacing & Layout

### 5.1. Spacing Scale

```json
{
  "spacing": {
    "unit": 4,
    
    "scale": {
      "0": 0,
      "1": 4,
      "2": 8,
      "3": 12,
      "4": 16,
      "5": 20,
      "6": 24,
      "8": 32,
      "10": 40,
      "12": 48,
      "16": 64,
      "20": 80,
      "24": 96
    },
    
    "panel": {
      "padding": "4",
      "gap": "2",
      "header_height": 32,
      "footer_height": 24
    },
    
    "channel_strip": {
      "width_narrow": 48,
      "width_normal": 72,
      "width_wide": 120,
      "width_full": 200
    },
    
    "border_radius": {
      "none": 0,
      "sm": 2,
      "md": 4,
      "lg": 6,
      "xl": 8,
      "full": 9999
    }
  }
}
```

---

## 6. Component Styles

### 6.1. Component Overrides

```json
{
  "components": {
    "Button": {
      "default": {
        "backgroundColor": "{colors.bg.tertiary}",
        "textColor": "{colors.text.primary}",
        "borderRadius": "{spacing.border_radius.md}",
        "padding": "{spacing.scale.2} {spacing.scale.4}",
        "fontSize": "{typography.font_size.base}",
        "fontWeight": "{typography.font_weight.medium}",
        "border": "1px solid {colors.border.primary}",
        "hover": {
          "backgroundColor": "{colors.bg.elevated}",
          "borderColor": "{colors.accent.primary}"
        },
        "pressed": {
          "backgroundColor": "{colors.accent.pressed}",
          "transform": "scale(0.97)"
        },
        "disabled": {
          "opacity": 0.4,
          "cursor": "not-allowed"
        }
      },
      "primary": {
        "backgroundColor": "{colors.accent.primary}",
        "textColor": "{colors.text.inverse}",
        "border": "none",
        "hover": {
          "backgroundColor": "{colors.accent.hover}"
        },
        "pressed": {
          "backgroundColor": "{colors.accent.pressed}"
        }
      },
      "icon": {
        "backgroundColor": "transparent",
        "border": "none",
        "padding": "{spacing.scale.2}",
        "hover": {
          "backgroundColor": "{colors.bg.tertiary}"
        }
      }
    },
    
    "Slider": {
      "default": {
        "trackColor": "{colors.bg.tertiary}",
        "fillColor": "{colors.accent.primary}",
        "thumbColor": "{colors.text.primary}",
        "thumbSize": 12,
        "trackHeight": 4,
        "borderRadius": "{spacing.border_radius.full}"
      }
    },
    
    "Knob": {
      "default": {
        "trackColor": "{colors.bg.tertiary}",
        "fillColor": "{colors.accent.primary}",
        "indicatorColor": "{colors.text.primary}",
        "size": 32,
        "trackWidth": 3,
        "fillWidth": 3
      }
    },
    
    "ChannelStrip": {
      "default": {
        "backgroundColor": "{colors.bg.secondary}",
        "headerColor": "{colors.track.default}",
        "meterColor": "{colors.meter}",
        "faderColor": "{colors.text.primary}",
        "borderColor": "{colors.border.primary}"
      }
    },
    
    "PianoRoll": {
      "grid": {
        "backgroundColor": "#1A1A1A",
        "lineColor": "#2A2A2A",
        "beatColor": "#333333",
        "barColor": "#3D3D3D",
        "playheadColor": "{colors.accent.primary}",
        "selectionColor": "rgba(255, 255, 255, 0.15)"
      }
    }
  }
}
```

### 6.2. Color Reference Syntax

```typescript
// Theme values can reference other theme tokens using {path} syntax:
// "{colors.bg.primary}" → resolves to "#1A1A1A"
// "{spacing.scale.4}" → resolves to "16"
// "{typography.font_size.base}" → resolves to "12"
//
// This enables consistent theming without duplication.
```

---

## 7. Animation Timing

### 7.1. Animation Configuration

```json
{
  "animations": {
    "duration": {
      "instant": 0,
      "fast": 80,
      "normal": 150,
      "slow": 300,
      "slower": 500
    },
    
    "easing": {
      "default": "ease-out",
      "enter": "ease-out-back",
      "exit": "ease-in",
      "bounce": "ease-out-elastic",
      "smooth": "ease-in-out-cubic"
    },
    
    "transitions": {
      "hover": {
        "duration": "fast",
        "easing": "default",
        "properties": ["background-color", "border-color", "color", "opacity"]
      },
      "press": {
        "duration": "instant",
        "easing": "exit"
      },
      "panel_open": {
        "duration": "slow",
        "easing": "enter"
      },
      "page_transition": {
        "duration": "slower",
        "easing": "smooth"
      },
      "value_change": {
        "duration": "normal",
        "easing": "smooth",
        "properties": ["transform", "opacity"]
      }
    },
    
    "scroll": {
      "momentum_decay": 500,
      "easing": "ease-out-quart",
      "snap_duration": 200
    }
  }
}
```

---

## 8. Icon System

### 8.1. Icon Configuration

```json
{
  "icons": {
    "library": "material-symbols",
    "size": {
      "sm": 12,
      "md": 16,
      "lg": 20,
      "xl": 24
    },
    "color": {
      "default": "{colors.text.secondary}",
      "active": "{colors.accent.primary}",
      "disabled": "{colors.text.disabled}"
    },
    
    "mapping": {
      "play": "play_arrow",
      "stop": "stop",
      "record": "fiber_manual_record",
      "loop": "repeat",
      "mute": "volume_off",
      "solo": "headphones",
      "arm": "radio_button_checked",
      "save": "save",
      "undo": "undo",
      "redo": "redo",
      "metronome": "music_note",
      "snap": "magnet",
      "quantize": "grid_on",
      "pencil": "edit",
      "select": "near_me",
      "erase": "ink_eraser",
      "cut": "content_cut",
      "glue": "merge",
      "zoom": "search",
      "folder": "folder",
      "search": "search",
      "settings": "settings",
      "add": "add",
      "close": "close",
      "more": "more_horiz",
      "drag": "drag_indicator"
    }
  }
}
```

---

## 9. Theme Creation

### 9.1. Theme Creation UI

```
┌──────────────────────────────────────────────────────────────┐
│  Theme Editor                       [Import]  [Export]      │
├──────────────────────────────────────────────────────────────┤
│  Name: My Custom Theme                                       │
│  Base: ARIA Dark                                             │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ Colors │ Typography │ Spacing │ Components │ Animations  │ │
│  ├─────────────────────────────────────────────────────────┤ │
│  │ Background                              ██ #1A1A1A     │ │
│  │ Surface                                 ██ #222222     │ │
│  │ Elevated                                ██ #2A2A2A     │ │
│  │                                                          │ │
│  │ Accent                                  ██ #FF7A00     │ │
│  │ Accent Hover                            ██ #FF8C1A     │ │
│  │ Accent Pressed                          ██ #E66E00     │ │
│  │                                                          │ │
│  │ Text Primary                            ██ #FFFFFF     │ │
│  │ Text Secondary                          ██ #BBBBBB     │ │
│  │                                                          │ │
│  │ [Reset to Default]            [Apply Theme]              │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                              │
│  Preview:                                                     │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  [Button]  [Button]  ⬛             ┌── 50% ──┐        │ │
│  │  ┌────────────────────────────────┐ │                   │ │
│  │  │ Track 1  ████████░░░░░░░░░░░░ │ │                   │ │
│  │  └────────────────────────────────┘ │                   │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### 9.2. Theme Distribution

```typescript
class ThemeManager {
  // Load a theme from a JSON file
  bool loadTheme(const std::string& path);
  
  // Apply a theme
  bool applyTheme(const Theme& theme);
  
  // Get current theme
  Theme currentTheme() const;
  
  // List installed themes
  std::vector<ThemeInfo> installedThemes() const;
  
  // Install a theme from file
  bool installTheme(const std::string& path);
  
  // Uninstall a theme
  bool uninstallTheme(const std::string& name);
  
  // Export current theme
  bool exportTheme(const std::string& path);
  
  // Import theme
  bool importTheme(const std::string& path);
  
  // Theme marketplace (future)
  void openMarketplace();
  
  // Theme locations:
  //   Windows: %APPDATA%/ARIA/themes/
  //   macOS: ~/Library/Application Support/ARIA/themes/
  //   Linux: ~/.config/aria/themes/
  std::string themeDirectory() const;
}
```

---

## 10. Built-in Themes

### 10.1. Default Theme

```json
{
  "name": "ARIA Dark",
  "author": "ARIA Team",
  "version": "1.0",
  "description": "Default dark theme — optimized for long sessions"
}
```

### 10.2. ARIA Light

```json
{
  "name": "ARIA Light",
  "author": "ARIA Team",
  "version": "1.0",
  "description": "Light theme for bright environments"
}
```

### 10.3. High Contrast

```json
{
  "name": "ARIA High Contrast",
  "author": "ARIA Team",
  "version": "1.0",
  "description": "High contrast theme for accessibility"
}
```

### 10.4. Future Themes

```typescript
// Planned themes:
// - "ARIA Neon" — cyberpunk style with glow effects
// - "ARIA Classic" — FL Studio-inspired color scheme
// - "ARIA Ableton" — Ableton Live-inspired
// - "ARIA Minimal" — reduced UI chrome
// - "ARIA DAW" — console/SSL-style mixer theme
```

---

## 11. Live Reload

### 11.1. Development Mode

```typescript
// During development, themes support live reload:
// 1. Edit theme JSON file
// 2. File watcher detects change
// 3. Theme is recompiled and applied
// 4. Only changed properties trigger re-render
// 5. No application restart needed

class ThemeLiveReload {
  void watch(const std::string& path);
  void stop();
  
  bool isWatching() const;
  std::string watchedPath() const;
};
```

---

## 12. API Reference

### 12.1. Public API

```typescript
class ThemeEngine {
  // Theme management
  bool load(const std::string& path);
  bool apply(const Theme& theme);
  Theme current() const;
  
  // Theme tokens
  Color resolveColor(const std::string& path) const;
  double resolveSpacing(const std::string& path) const;
  Font resolveFont(const std::string& path) const;
  
  // Component style
  ComponentStyle styleFor(const std::string& component,
                           const std::string& variant = "default",
                           const ComponentState& state = {}) const;
  
  // Theme events
  void onThemeChanged(callback: (theme: Theme) => void);
  
  // Theme list
  std::vector<ThemeInfo> availableThemes() const;
  
  // Theme file management
  std::string themePath(const std::string& name) const;
  bool saveUserTheme(const Theme& theme);

private:
  Theme current_;
  std::unordered_map<string, Theme> installed_;
  
  // Token cache (for fast resolution)
  TokenCache tokenCache_;
  
  // Reference resolver
  string resolveReferences(const string& value, const Theme& theme) const;
}
```

---

## 13. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-TH-001 | Theme Model & JSON Schema | 🔲 Pending |
| RFC-TH-002 | Color System & Token Resolution | 🔲 Pending |
| RFC-TH-003 | Typography System | 🔲 Pending |
| RFC-TH-004 | Spacing Scale & Layout Tokens | 🔲 Pending |
| RFC-TH-005 | Component Style Definitions | 🔲 Pending |
| RFC-TH-006 | Theme Inheritance | 🔲 Pending |
| RFC-TH-007 | Reference Resolution ({path} syntax) | 🔲 Pending |
| RFC-TH-008 | Animation Timing & Easing Tokens | 🔲 Pending |
| RFC-TH-009 | Icon System & Mapping | 🔲 Pending |
| RFC-TH-010 | Theme Manager (Install/Export) | 🔲 Pending |
| RFC-TH-011 | Theme Editor UI | 🔲 Pending |
| RFC-TH-012 | Theme Live Reload | 🔲 Pending |
| RFC-TH-013 | Built-in Themes (Dark/Light/HC) | 🔲 Pending |
| RFC-TH-014 | Theme Preview System | 🔲 Pending |
| RFC-TH-015 | Theme Performance Optimization | 🔲 Pending |
| RFC-TH-016 | Theme Marketplace (Future) | 🔲 Pending |

---

## Appendix A: Theme File Size

| Theme | Size | Notes |
|---|---|---|
| Default themes | ~5KB each | JSON, compact format |
| Custom theme | ~3-15KB | Depends on overrides |
| Full theme (all overrides) | ~50KB | Every component customized |

## Appendix B: Theme Migration

```
Theme format version history:
  v1: Initial format

Migration path:
  v1 → v2: Color palette renamed (automatic converter)
  v2 → v3: Component style structure changed (script-based)
  
Themes are NOT backwards compatible across major versions.
A migration tool is provided for each version bump.
```
