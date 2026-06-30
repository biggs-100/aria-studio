# Design: P10c Theme System

## Technical Approach

Nested JSON theme files loaded by a shared `ThemeEngine` (C++/TS), tokens resolved via `{path.to.key}` syntax with caching. Widgets replace hardcoded colors with `theme().resolveColor(path)` calls. Live reload watches the theme file, re-parses, and broadcasts `ThemeChanged` via `EventBus`.

Maps to proposal approach and both specs: theme-engine covers loading/resolution/cache/live-reload; theme-widget-integration covers binding.

## Architecture Decisions

### Decision: ThemeEngine lifecycle — singleton via ServiceLocator

| Option | Tradeoff |
|--------|----------|
| Singleton `ThemeEngine::instance()` | Simple, but hidden global state |
| ServiceLocator registration | Matches existing pattern (GraphicsEngine, BrowserEngine) |
| **Chosen: ServiceLocator + static accessor** | Register at init; `ThemeEngine::get()` as convenience for widget access |

Widgets are deep in the tree without ServiceLocator refs. Static `ThemeEngine::get()` (backs to cached ServiceLocator pointer) gives zero-cost access from render paths.

### Decision: ThemedWidget — intermediate base class

| Option | Tradeoff |
|--------|----------|
| Free function `resolve_theme_color(path)` | No type safety, no dirty-on-change |
| Global ThemeEvents subscription per widget | Boilerplate in every widget render |
| **Chosen: ThemedWidget : public Widget** | Adds `theme()` accessor + auto `mark_dirty()` on `ThemeChanged`; existing widgets change parent from `Widget` to `ThemedWidget` |

### Decision: File watcher — new C++ FileWatcher (not Rust FFI)

| Option | Tradeoff |
|--------|----------|
| Reuse Rust watcher from BrowserEngine | Tied to browser module, FFI overhead |
| **Chosen: Dedicated `FileWatcher` using platform API** | Simple, focused, debounce + re-parse + EventBus broadcast |

### Decision: Token resolution — recursive with depth limit, no AST

| Option | Tradeoff |
|--------|----------|
| Full expression parser | Overkill for `{a.b.c}` syntax only |
| **Chosen: Dot-path walk on JSON tree** | Matches spec, cycle detection at 10 depth, ~20 LOC |

## Data Flow

```
theme.json ──→ ThemeEngine::load()
                  │
                  ├── parse JSON (nlohmann)
                  ├── resolve {path} refs → cache
                  └── store in Theme struct
                        │
ThemeChanged ◄── FileWatcher (debounce 200ms)
                        │
                        ▼
                  ThemedWidget::onThemeChanged()
                        │
                  mark_dirty() → next paint pass
                        │
                  render() → theme().resolveColor("colors.bg.primary")
```

```
TS side:
  theme.json ──→ ThemeEngine.ts ::load()
                    │
                    ├── parse JSON
                    ├── build token cache
                    └── resolveColor(path) → {r,g,b,a}
                          │
                    render() → set VNode props from theme
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/graphics/theme_types.h` | Create | `Theme`, `ColorPalette`, `TypographySettings`, `SpacingSystem`, `ComponentStyles` structs |
| `src/graphics/theme_engine.h` | Create | `ThemeEngine` class — load, resolve, cache, watch |
| `src/graphics/theme_engine.cc` | Create | Implementation: JSON parsing, token resolution, cycle detection |
| `src/graphics/widget_themed.h` | Create | `ThemedWidget` base — `theme()` accessor, `ThemeChanged` subscriber |
| `src/graphics/widget_themed.cc` | Create | Event subscription, dirty propagation |
| `src/graphics/file_watcher.h` | Create | Platform file watcher with debounce |
| `src/graphics/file_watcher.cc` | Create | Implementation (Win32 `ReadDirectoryChangesW`, macOS `kqueue`, Linux `inotify`) |
| `src/graphics/widget_rect.h` | Modify | Inherit from `ThemedWidget`, add `theme_fill` / `theme_stroke` optional tokens |
| `src/graphics/widget_rect.cc` | Modify | `render()` resolves fill from theme if token set |
| `src/graphics/widget_text.h` | Modify | Inherit from `ThemedWidget`, add `theme_color` token |
| `src/graphics/widget_text.cc` | Modify | `render()` resolves text color from theme |
| `src/graphics/widget_button.h` | Modify | Inherit from `ThemedWidget`, replace hardcoded colors with token paths |
| `src/graphics/widget_button.cc` | Modify | `resolve_fill_color()` uses `theme().resolveColor()` |
| `src/graphics/widget_container.h` | Modify | Inherit from `ThemedWidget`, add theme background token |
| `src/graphics/widget_container.cc` | Modify | `render()` draws background from theme if token set |
| `src/browser/browser_panel_gpu.h` | Modify | Replace `Color::from_rgba8` members with theme token paths |
| `src/browser/browser_panel_gpu.cc` | Modify | All Skia draw calls use `theme().resolveColor()` |
| `ui/src/theme/ThemeEngine.ts` | Create | TS token resolver — load JSON, `resolveColor(path)` → `{r,g,b,a}` |
| `ui/src/theme/file-watcher.ts` | Create | TS file watcher bridge (calls C++ watcher or `fs.watch` in Node) |
| `ui/src/components/fader.ts` | Modify | VNode colors from `ThemeEngine.resolveColor()` |
| `ui/src/components/meter-bar.ts` | Modify | Zone colors from theme meter tokens |
| `ui/src/components/channel-strip.ts` | Modify | Header/bg/border from theme track tokens |
| `ui/src/components/timeline-ruler.ts` | Modify | Grid/tick/loop colors from theme tokens |
| `themes/aria-dark.json` | Create | Built-in dark theme |
| `themes/aria-light.json` | Create | Built-in light theme |
| `themes/aria-high-contrast.json` | Create | Built-in high-contrast theme |

## Interfaces / Contracts

```cpp
// Theme types — mirrors 15_Theme_System.md JSON schema
struct ColorPalette { /* bg, text, accent, border, track, etc. */ };
struct TypographySettings { /* font_family, font_size, text_styles */ };
struct SpacingSystem { /* scale, panel, channel_strip, border_radius */ };
struct ComponentStyles { /* Button, Slider, Knob, ChannelStrip, PianoRoll */ };
struct Theme { std::string name; ColorPalette colors; /* ... */ };

class ThemeEngine {
public:
    static ThemeEngine* get();                     // ServiceLocator convenience
    bool load(std::string_view path);              // Parse JSON theme file
    Color resolveColor(std::string_view path);     // "colors.bg.primary" → Color
    float resolveSpacing(std::string_view path);   // "spacing.scale.4" → 16.0f
    void watch(std::string_view path);             // Start live reload
    void apply(Theme theme);                       // Swap theme, invalidate cache
    std::vector<ThemeInfo> availableThemes() const;
};

class ThemedWidget : public Widget {
protected:
    ThemeEngine* theme() const noexcept;           // → ThemeEngine::get()
    void onThemeChanged();                         // Called by EventBus subscription, marks dirty
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | Theme JSON parsing, token resolution, cycle detection, cache | C++ tests with `nlohmann::json` fixtures; TS tests with mock JSON |
| Unit | ThemedWidget dirty-on-theme-change | Mock EventBus, fire ThemeChanged, assert is_dirty() |
| Unit | FileWatcher debounce | Synthetic file write, assert single ThemeChanged event within 200ms window |
| Integration | Widget renders with theme colors | Load aria-dark, render ButtonWidget, assert SkiaCanvas receive correct color |
| Integration | Theme switch re-renders all widgets | Apply theme B, assert all registered ThemedWidgets are dirty |
| E2E | Live reload: edit theme file → widgets update | Write theme file, wait 300ms, assert re-render |

## Migration / Rollout

No data migration. Widgets that haven't been migrated yet continue to use hardcoded fallback colors equal to ARIA Dark palette. Each widget migration is independent — per spec, `grep from_rgba8` on migrated files returns zero.

## Open Questions

- [ ] Should `theme()` return const ThemeEngine& or pointer? Pointer allows null-check for fallback.
- [ ] FileWatcher platform impl: start with Win32 `ReadDirectoryChangesW` only, stub other platforms?
