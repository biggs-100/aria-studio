#ifndef ARIA_THEME_ENGINE_H
#define ARIA_THEME_ENGINE_H

#include "graphics/theme_types.h"
#include "graphics/file_watcher.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aria {

/// Theme engine — loads, resolves, and caches theme tokens.
///
/// Registered with ServiceLocator for application-wide access.
/// Static ThemeEngine::get() provides zero-cost access from widget render paths.
///
/// Token resolution:
///   - Dot-path notation: "colors.bg.primary" → walks nested JSON objects
///   - Recursive descent with max depth of 10 (cycle protection)
///   - Results cached in flat unordered_map for O(1) repeated access
///   - Cache invalidated on re-load
class ThemeEngine {
public:
    ThemeEngine();
    ~ThemeEngine();

    ThemeEngine(const ThemeEngine&) = delete;
    ThemeEngine& operator=(const ThemeEngine&) = delete;
    ThemeEngine(ThemeEngine&&) = delete;
    ThemeEngine& operator=(ThemeEngine&&) = delete;

    // ── Static accessor ──────────────────────────────────────────

    /// Convenience static accessor. Backs to ServiceLocator.
    /// Returns nullptr if ThemeEngine is not registered.
    static ThemeEngine* get();

    /// Set the static pointer (called once during ServiceLocator registration).
    static void set_instance(ThemeEngine* instance);

    // ── Loading ──────────────────────────────────────────────────

    /// Load a theme from a JSON file on disk.
    /// Returns true on success, false on I/O or parse error.
    bool load_from_file(const std::string& path);

    /// Load a theme from an in-memory JSON string (for testing).
    bool load_from_string(std::string_view json);

    // ── Token resolution ────────────────────────────────────────

    /// Resolve a color by dot path (e.g. "colors.bg.primary").
    /// Returns the resolved Color, or fallback Color (black) if the path
    /// is invalid or the value is not a valid color.
    Color resolveColor(std::string_view path);

    /// Resolve a float by dot path (e.g. "typography.fontSize").
    /// Returns the resolved float, or 0.0f if the path is invalid.
    float resolveFloat(std::string_view path);

    // ── Theme enumeration ───────────────────────────────────────

    /// List available (built-in) themes.
    std::vector<ThemeInfo> availableThemes() const;

    // ── Live reload ──────────────────────────────────────────────

    /// Start watching a theme file for changes.
    void watch(const std::string& path);

    /// Get the current theme.
    const Theme& current() const noexcept { return current_; }

private:
    // ── Internal helpers ─────────────────────────────────────────

    /// Parse a JSON string into the current theme.
    /// Populates the flat token cache.
    void parse_json(std::string_view json);

    /// Walk nested JSON to resolve a dot path.
    /// Used by both resolveColor and resolveFloat.
    std::string resolve_token(std::string_view path);

    /// Resolve a {token} reference within a string value.
    std::string resolve_references(const std::string& value);

    /// Parse a hex color string (#RRGGBB or RRGGBB) into Color.
    static Color parse_hex_color(const std::string& hex);

    /// Parse an rgba() string into Color.
    static Color parse_rgba_color(const std::string& rgba);

    /// Parse a string value into a float.
    static float parse_float(const std::string& value);

    // ── State ────────────────────────────────────────────────────

    Theme current_;

    /// Flat token cache: "colors.bg.primary" → "#1A1A1E"
    std::unordered_map<std::string, std::string> token_cache_;

    /// Color cache: "colors.bg.primary" → Color{0.1, 0.1, 0.12, 1.0}
    /// Separated for type safety and to avoid re-parsing.
    std::unordered_map<std::string, Color> color_cache_;

    /// Float cache: "typography.fontSize" → 14.0f
    std::unordered_map<std::string, float> float_cache_;

    /// Raw JSON for re-parsing on reference resolution.
    std::string raw_json_;

    /// File path currently loaded (for live reload).
    std::string current_path_;

    /// File watcher instance for live reload.
    std::unique_ptr<FileWatcher> watcher_;

    /// Whether a theme is currently loaded.
    bool loaded_ = false;

    /// The single static instance pointer.
    static ThemeEngine* instance_;

    /// Rebuild the flat token cache from the current theme.
    void rebuild_cache();
};

} // namespace aria

#endif // ARIA_THEME_ENGINE_H
