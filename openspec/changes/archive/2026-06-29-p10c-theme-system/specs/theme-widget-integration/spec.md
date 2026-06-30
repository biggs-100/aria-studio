# Theme Widget Integration Specification

## Purpose

Migrate all hardcoded colors in C++ widgets and TypeScript components to resolve from the active theme. Every visual property — fill, stroke, text color, zone thresholds — SHALL use `ThemeEngine::resolveColor()` (C++) or `ThemeEngine.resolveColor()` (TS) instead of literal values.

## Requirements

### Requirement: ThemedWidget Mixin

A `ThemedWidget` mixin class SHALL extend the `Widget` base with a `theme()` accessor returning the current `ThemeEngine` instance. Every themed widget SHALL inherit from `ThemedWidget`. The mixin SHALL subscribe to `ThemeChanged` events and mark itself dirty when the theme changes.

#### Scenario: Widget marked dirty on theme change

- GIVEN a visible ThemedWidget
- WHEN `ThemeChanged` event fires
- THEN `is_dirty()` returns true
- AND the widget re-renders on the next paint pass

### Requirement: C++ Widget Color Replacement

The following C++ widgets SHALL replace hardcoded color literals with `theme().resolveColor(path)` calls:

| Widget | Tokens to Replace |
|--------|-------------------|
| `RectWidget` | `fill` color → theme fill token (optional binding) |
| `TextWidget` | `textColor` → `colors.text.primary` / `colors.text.secondary`; font → typography tokens |
| `ButtonWidget` | `bg` → `colors.bg.tertiary`; `hover` → `colors.bg.elevated`; `text` → `colors.text.primary`; `border` → `colors.border.primary`, `colors.accent.primary` (hover) |
| `ContainerWidget` | `bg` → `colors.bg.primary` / `colors.bg.secondary` |
| `BrowserPanelGPU` | All Skia draw color args → theme tokens |

#### Scenario: ButtonWidget renders with theme colors

- GIVEN a ButtonWidget and an active theme where `colors.bg.tertiary = "#2A2A2A"`
- WHEN the button renders in its default state
- THEN `SkiaCanvas::drawRect()` is called with fill color `0x2A2A2AFF`

#### Scenario: ButtonWidget hover changes color

- GIVEN a ButtonWidget with hover state triggered
- WHEN the button renders
- THEN the fill color is `theme().resolveColor("colors.bg.elevated")` instead of the default background

#### Scenario: TextWidget renders with theme text color

- GIVEN a TextWidget and active theme where `colors.text.primary = "#FFFFFF"`
- WHEN the text renders
- THEN `SkiaCanvas::drawTextBlob()` uses color `0xFFFFFFFF`

### Requirement: TS Component Color Replacement

The following TS components SHALL replace hardcoded VNode prop colors with `ThemeEngine.resolveColor()`:

| Component | Tokens to Replace |
|-----------|-------------------|
| `Fader` | `fillR/fillG/fillB` → `colors.bg.tertiary`; thumb → `colors.text.primary` |
| `MeterBar` | Zone colors → `colors.meter.cold`, `colors.meter.warm`, `colors.meter.hot`, `colors.meter.critical`, `colors.meter.clip` |
| `ChannelStrip` | Header → `colors.track.default` (or per-type); bg → `colors.bg.secondary`; border → `colors.border.primary` |
| `TimelineRuler` | Grid → `colors.waveform.grid`; bar ticks → `colors.text.secondary`; loop overlay → `colors.selection.fill` |

#### Scenario: Fader renders with theme-derived fill

- GIVEN a Fader and active theme where `colors.bg.tertiary = "#2A2A2A"`
- WHEN the Fader renders
- THEN the track fill VNode has R=42, G=42, B=42 in its Skia draw props

#### Scenario: MeterBar zone colors from theme

- GIVEN a MeterBar and active theme where `colors.meter.warm = "#7AFF7A"`
- WHEN the meter renders at -12 dB
- THEN the green-yellow threshold uses the theme's warm zone color
- AND no CSS-color literals are present in the render path

#### Scenario: ChannelStrip header color per track type

- GIVEN a ChannelStrip with `trackType = "audio"`
- WHEN the header renders
- THEN the header color is resolved from `colors.track.audio`

### Requirement: Fallback Behavior

If `ThemeEngine` is not initialized when a widget renders, each widget SHALL use a built-in fallback color matching the ARIA Dark palette. No widget SHALL crash due to a missing theme.

#### Scenario: Theme engine not initialized

- GIVEN a ThemedWidget before `ThemeEngine` is loaded
- WHEN the widget renders
- THEN it uses hardcoded fallback colors matching the ARIA Dark palette
- AND no exception or assertion is triggered

### Requirement: No Hardcoded Color Literals Remain

After migration, a grep for `from_rgba8` and `Color::from` in widget source files SHALL return zero results (except in the fallback code path).

#### Scenario: Verify no hardcoded widget colors

- GIVEN the fully migrated widget source tree
- WHEN searching for `from_rgba8\(` in `src/graphics/widget_*.cc` and `src/graphics/widget_*.h`
- THEN zero matches are found
