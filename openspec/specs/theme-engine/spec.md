# Theme Engine Specification

## Purpose

Provide a central theme engine that loads JSON theme files, resolves `{path}` token references, and exposes typed accessors (color, spacing, font, component style) for both C++ widgets and TypeScript components. A live reload hook SHALL detect theme file changes and trigger targeted re-renders.

## Requirements

### Requirement: Theme JSON Format

The system SHALL define C++ types that mirror the `15_Theme_System.md` JSON schema: `ColorPalette`, `TypographySettings`, `SpacingSystem`, `ComponentStyles`, `AnimationTheme`, and `IconTheme`. The root `Theme` struct SHALL contain metadata (`name`, `author`, `version`, `description`) and all token sections.

#### Scenario: Deserialize valid theme JSON

- GIVEN a valid theme JSON file matching the 15_Theme_System.md schema
- WHEN `ThemeEngine::load(path)` is called
- THEN a `Theme` struct is populated with all token sections
- AND `theme.name` matches the `"name"` field in JSON

#### Scenario: Missing optional fields

- GIVEN a theme JSON with no `description` field
- WHEN `ThemeEngine::load(path)` is called
- THEN `theme.description` is an empty string
- AND no error is raised

### Requirement: Token Resolution with {path} Syntax

The engine SHALL resolve `"{colors.bg.primary}"` references by traversing the theme data structure. Resolution SHALL support dot-separated paths, nested objects, and type coercion (string → float for spacing values). Circular references SHALL be detected and return the default value.

#### Scenario: Resolve color token

- GIVEN a loaded theme with `colors.bg.primary = "#1A1A1A"`
- WHEN `engine.resolveColor("colors.bg.primary")` is called
- THEN the returned Color is `0x1A1A1AFF` (RGBA)

#### Scenario: Resolve spacing token with type coercion

- GIVEN a loaded theme with `spacing.scale.4 = 16`
- WHEN `engine.resolveSpacing("spacing.scale.4")` is called
- THEN the returned float is `16.0`

#### Scenario: Circular reference returns default

- GIVEN a theme where token A references token B which references token A
- WHEN `engine.resolveColor("colors.bg.primary")` is called via the chain
- THEN the circular reference is detected at max 10 depth
- AND the engine returns the configured default color

#### Scenario: Unknown token returns default

- GIVEN a loaded theme with no `colors.bg.nonexistent` token
- WHEN `engine.resolveColor("colors.bg.nonexistent")` is called
- THEN the engine returns the configured default color
- AND a warning is logged

### Requirement: Token Cache

The engine SHALL cache resolved token values in a `unordered_map<string, ResolvedToken>`. The cache SHALL be invalidated when `ThemeEngine::apply(newTheme)` is called. Cache lookups SHALL be O(1).

#### Scenario: Cached resolution returns without traversal

- GIVEN a token `"colors.bg.primary"` resolved once
- WHEN `resolveColor("colors.bg.primary")` is called again
- THEN the cached value is returned
- AND the path traversal is skipped

### Requirement: Live Reload

The engine SHALL expose a `watch(path)` method that starts monitoring the theme file via the existing `file-watcher` infrastructure. On file change, the engine SHALL reload the theme, re-resolve all tokens, and fire a `ThemeChanged` event.

#### Scenario: Theme file changed triggers reload

- GIVEN an active file watcher on `aria-dark.json`
- WHEN the file is modified and saved
- THEN the theme is reloaded
- AND `ThemeChanged` event fires with the new theme

#### Scenario: Invalid theme file on live reload

- GIVEN a live-reloaded theme file
- WHEN the file is saved with invalid JSON (syntax error)
- THEN the previous theme remains active
- AND an error is logged
- AND no `ThemeChanged` event fires

### Requirement: Theme Availability

The engine SHALL ship with three built-in themes: `ARIA Dark`, `ARIA Light`, and `ARIA High Contrast`. Each theme SHALL be stored as a JSON file in a `themes/` directory distributed with the application.

#### Scenario: Built-in themes loadable

- GIVEN the application is installed with theme files
- WHEN `ThemeEngine::availableThemes()` is called
- THEN the result contains "ARIA Dark", "ARIA Light", and "ARIA High Contrast"
- AND each can be loaded via `load(themes[i].path)`
