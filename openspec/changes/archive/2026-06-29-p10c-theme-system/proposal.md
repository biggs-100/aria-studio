# Proposal: P10c Theme System

## Intent

Every C++ widget and TS component in ARIA DAW hardcodes colors. This prevents visual customization, makes theme switching impossible, and creates technical debt as the widget library grows. Replace hardcoded colors with a JSON theme system — the foundation for all future theming.

## Scope

### In Scope
- Theme JSON format (colors, typography, spacing, component styles)
- `ThemeEngine` C++ class — JSON loader, token resolution (`{colors.bg.primary}` → `#1A1A1A`), token cache with cycle detection
- TS `ThemeEngine` — same token resolution for VNode prop values
- Theme binding to all C++ widgets (Rect, Text, Button, Container, BrowserPanelGPU)
- Theme binding to all TS components (Fader, MeterBar, ChannelStrip, TimelineRuler)
- Built-in themes: aria-dark.json, aria-light.json, aria-high-contrast.json
- Live reload — file watcher triggers theme recompile + targeted re-render

### Out of Scope
- Animation engine & easing curves (→ `p10c-animation-engine`)
- Input system extensions (→ `p10c-input-system`)
- Font loading / font file management
- Theme Editor UI (future P11+)
- Theme marketplace (future)

## Capabilities

### New Capabilities
- `theme-engine`: Theme JSON format, ThemeEngine C++/TS classes, token resolution, token cache, live reload file watcher
- `theme-widget-integration`: Theme binding to C++ widgets + TS components, migration from hardcoded colors

### Modified Capabilities
- None

## Approach

1. Define C++ theme types (`theme_types.h`) mirroring `15_Theme_System.md` JSON schema — ColorPalette, TypographySettings, SpacingSystem, ComponentStyles
2. Implement `ThemeEngine` — loads JSON via nlohmann/json, resolves `{path}` references (recursive descent, cycle detection, max 10 depth), caches resolved tokens
3. TS `ThemeEngine` — mirrors C++ resolution, applies to VNode prop maps before rendering
4. `WidgetThemed` base — widget mixin that resolves color/font/spacing from active theme
5. Migrate each widget — replace `Color::from_rgba8(60,60,60)` with `theme().resolveColor("colors.bg.tertiary")`
6. Live reload — hook into existing `file-watcher` spec, debounce theme file changes, recompile + re-render dirty widgets
7. Distribute three built-in themes as JSON assets

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/graphics/theme_engine.h/.cc` | New | ThemeEngine core |
| `src/graphics/theme_types.h` | New | Theme data structures |
| `src/graphics/widget_themed.h/.cc` | New | ThemedWidget mixin |
| `src/graphics/widget_rect.h/.cc` | Modified | Theme token binding |
| `src/graphics/widget_text.h/.cc` | Modified | Font/color from theme |
| `src/graphics/widget_button.h/.cc` | Modified | Colors from theme |
| `ui/src/theme/ThemeEngine.ts` | New | TS token resolver |
| `ui/src/theme/theme-live-reload.ts` | New | Live reload handler |
| `ui/src/components/fader.ts` | Modified | VNode colors from theme |
| `ui/src/components/meter-bar.ts` | Modified | Zone colors from theme |
| `ui/src/components/channel-strip.ts` | Modified | Track colors from theme |
| `ui/src/components/timeline-ruler.ts` | Modified | Grid colors from theme |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Theme `{path}` reference cycles | Low | Cycle detection, max 10 depth limit |
| Live reload race condition | Low | Debounce + file watcher lock |
| Widget migration misses a color | Medium | Visual audit checklist in spec scenarios |

## Rollback Plan

Revert all files modified by this change. Existing spec files (gpu-widget-system, gpu-rendering, ts-component-library) remain unchanged — only widget implementations are modified. The ThemeEngine itself is purely additive (new files only).

## Dependencies

- nlohmann/json (already in project)
- Existing `file-watcher` spec for live reload integration

## Success Criteria

- [ ] All C++ widgets resolve colors from theme, not hardcoded values
- [ ] All TS components resolve VNode colors from theme
- [ ] Live reload detects theme file change and re-renders affected widgets in <100ms
- [ ] Switching theme changes all visible widget colors in one frame
- [ ] Unknown token resolves to default (does not crash)
