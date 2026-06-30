# Tasks: P10c Theme System

## Phase 1 — Theme Engine (core)

- [x] **T1.1** Create `theme_types.h` — `Theme`, `ColorPalette`, `TypographySettings`, `SpacingSystem`, `ComponentStyles` structs matching `15_Theme_System.md` JSON schema
- [x] **T1.2** Create `theme_engine.h/.cc` — `ThemeEngine` class: `load(path)` via nlohmann, `resolveColor(path)` / `resolveSpacing(path)` with recursive dot-path resolution (max depth 10, cycle detection via visited set), `unordered_map` token cache
- [x] **T1.3** Create `widget_themed.h/.cc` — `ThemedWidget : public Widget` with `theme()` accessor (`ThemeEngine::get()`), subscribe to `ThemeChanged` on mount, `mark_dirty()` on change
- [x] **T1.4** Create `file_watcher.h/.cc` — platform file watcher (Win32 `ReadDirectoryChangesW` first), 200ms debounce, re-parse JSON on change, publish `ThemeChanged` via `EventBus`
- [x] **T1.5** Register `ThemeEngine` in `ServiceLocator` during app init, set `ThemeEngine::get()` static pointer

## Phase 2 — C++ Widget Binding

- [x] **T2.1** `RectWidget` → inherit `ThemedWidget`, `render()` checks optional `theme_fill_` / `theme_stroke_` token before falling back to `paint_`
- [x] **T2.2** `TextWidget` → inherit `ThemedWidget`, `render()` resolves `color_` from theme if `theme_color_path_` set
- [x] **T2.3** `ButtonWidget` → inherit `ThemedWidget`, replace `normal_color_` etc. with theme paths (`colors.bg.tertiary`, `colors.bg.elevated`, etc.)
- [x] **T2.4** `ContainerWidget` → inherit `ThemedWidget`, render bg from theme token
- [x] **T2.5** `BrowserPanelGPU` → migrate all `Color::from_rgba8()` calls in `TreeNodeWidget`, `WaveformPreviewWidget`, and `BrowserPanelGPU::render()` to `theme().resolveColor()`

## Phase 3 — TS ThemeEngine + Component Binding

- [x] **T3.1** Create `ui/src/theme/ThemeEngine.ts` — load JSON, build flat token cache, `resolveColor(path)` → `{r,g,b,a}`, `resolveFloat(path)` → `number`
- [x] **T3.2** Create `ui/src/theme/file-watcher.ts` — bridge to C++ FileWatcher via FFI (or `fs.watch` in dev), callback on change
- [x] **T3.3** `Fader` → track/thumb/label colors from `ThemeEngine.resolveColor()`
- [x] **T3.4** `MeterBar` → zone colors from `colors.meter.*` tokens
- [x] **T3.5** `ChannelStrip` → header/track colors from `colors.track.*`, bg from `colors.bg.secondary`
- [x] **T3.6** `TimelineRuler` → grid/tick/loop overlay from `colors.waveform.*`, `colors.text.*`, `colors.selection.*`

## Phase 4 — Built-in Themes + Live Reload

- [x] **T4.1** Create `themes/aria-dark.json` — full theme mirroring `15_Theme_System.md` ARIA Dark palette
- [x] **T4.2** Create `themes/aria-light.json` — light variant (inverted bg, adjusted accents)
- [x] **T4.3** Create `themes/aria-high-contrast.json` — accessibility variant (max contrast, larger hit areas)
- [x] **T4.4** Wire live reload: `ThemeEngine::watch()` called at startup, `ThemeChanged` → C++ widgets dirty, TS bridge notified → components re-render

## Review Workload Forecast

| File | Risk | Est. Review Time |
|------|------|-----------------|
| `theme_engine.h/.cc` | MEDIUM — token resolution logic | 15 min |
| `widget_themed.h/.cc` | LOW — base class wiring | 5 min |
| `file_watcher.h/.cc` | MEDIUM — platform-specific APIs | 10 min |
| `theme_types.h` | LOW — data-only | 3 min |
| Each C++ widget (×5) | LOW — mechanical replacement | 3 min each |
| `ThemeEngine.ts` | LOW — direct C++ port | 5 min |
| Each TS component (×4) | LOW — VNode prop changes | 2 min each |
| Theme JSON files (×3) | LOW — data-only | 2 min each |

**Total estimated review: ~60 min**
