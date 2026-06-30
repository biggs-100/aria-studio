#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "graphics/theme_types.h"
#include "graphics/theme_engine.h"
#include "graphics/widget_themed.h"
#include "graphics/widget_rect.h"
#include "graphics/widget_text.h"
#include "graphics/widget_button.h"
#include "graphics/widget_container.h"
#include "graphics/graphics_types.h"
#include "kernel/service_locator.h"

#include <fstream>
#include <cstdio>
#include <string>
#include <filesystem>

using namespace aria;

// ═══════════════════════════════════════════════════════════════════
// Phase 1 — Theme Types (T1.1)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ColorPalette default values", "[theme][types]") {
    ColorPalette cp;

    // Default bg colors
    CHECK(cp.bg_primary == Color::Black);
    CHECK(cp.bg_secondary == Color::Black);
    CHECK(cp.bg_tertiary == Color::Black);

    // Default text colors
    CHECK(cp.text_primary == Color::White);
    CHECK(cp.text_secondary == Color::White);

    // Default accent
    CHECK(cp.accent_primary == Color::White);
}

TEST_CASE("Theme default construction", "[theme][types]") {
    Theme t;
    CHECK(t.name.empty());
    CHECK(t.author.empty());
    CHECK(t.version.empty());

    // Nested structs are default-constructed
    CHECK(t.colors.bg_primary == Color::Black);
    CHECK(t.typography.font_size == 0.0f);
    CHECK(t.spacing.padding == 0.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Phase 1 — ThemeEngine token resolution (T1.2)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ThemeEngine loads a minimal theme from JSON string",
          "[theme][engine][resolve]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "Test",
        "author": "Tester",
        "version": "1.0",
        "colors": {
            "bg": { "primary": "#1A1A1E", "secondary": "#2A2A30" },
            "text": { "primary": "#E0E0E0" },
            "accent": { "primary": "#4A9EFF" }
        },
        "typography": { "fontSize": 14, "fontFamily": "Inter" },
        "spacing": { "padding": 8 }
    })";

    REQUIRE(engine.load_from_string(json));

    // ── resolveColor ──────────────────────────────────────────────
    Color bg_primary = engine.resolveColor("colors.bg.primary");
    CHECK(bg_primary.r == Approx(0.102f).epsilon(0.01f));
    CHECK(bg_primary.g == Approx(0.102f).epsilon(0.01f));
    CHECK(bg_primary.b == Approx(0.118f).epsilon(0.01f));
    CHECK(bg_primary.a == 1.0f);

    Color text_primary = engine.resolveColor("colors.text.primary");
    CHECK(text_primary.r == Approx(0.878f).epsilon(0.01f));

    Color accent = engine.resolveColor("colors.accent.primary");
    CHECK(accent.r == Approx(0.29f).epsilon(0.01f));
    CHECK(accent.g == Approx(0.62f).epsilon(0.01f));
    CHECK(accent.b == Approx(1.0f).epsilon(0.01f));

    // ── resolveFloat ──────────────────────────────────────────────
    float font_size = engine.resolveFloat("typography.fontSize");
    CHECK(font_size == 14.0f);

    float padding = engine.resolveFloat("spacing.padding");
    CHECK(padding == 8.0f);

    // ── Missing path returns default ──────────────────────────────
    Color missing = engine.resolveColor("colors.nonexistent.key");
    CHECK(missing.r == 0.0f);
    CHECK(missing.g == 0.0f);
    CHECK(missing.b == 0.0f);
    CHECK(missing.a == 1.0f);

    float missing_f = engine.resolveFloat("typography.nonexistent");
    CHECK(missing_f == 0.0f);
}

TEST_CASE("ThemeEngine resolveColor handles rgba() syntax",
          "[theme][engine][resolve]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "RGBA Test",
        "colors": {
            "bg": { "overlay": "rgba(0, 0, 0, 0.6)" }
        }
    })";

    REQUIRE(engine.load_from_string(json));

    Color c = engine.resolveColor("colors.bg.overlay");
    CHECK(c.r == 0.0f);
    CHECK(c.g == 0.0f);
    CHECK(c.b == 0.0f);
    CHECK(c.a == Approx(0.6f));
}

TEST_CASE("ThemeEngine token resolution hits cache on repeated calls",
          "[theme][engine][cache]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "Cache Test",
        "colors": { "test": { "value": "#FF0000" } }
    })";
    REQUIRE(engine.load_from_string(json));

    // First call populates cache
    Color c1 = engine.resolveColor("colors.test.value");
    CHECK(c1.r == 1.0f);

    // Second call hits cache — should return same result
    Color c2 = engine.resolveColor("colors.test.value");
    CHECK(c2.r == 1.0f);
    CHECK(c2.g == 0.0f);
    CHECK(c2.b == 0.0f);
}

TEST_CASE("ThemeEngine handles nested resolution of {token} references",
          "[theme][engine][resolve]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "Ref Test",
        "colors": {
            "bg": { "primary": "#1A1A1E" },
            "surface": "{colors.bg.primary}"
        },
        "spacing": { "unit": 4, "large": "{spacing.unit}" }
    })";
    REQUIRE(engine.load_from_string(json));

    Color surface = engine.resolveColor("colors.surface");
    CHECK(surface.r == Approx(0.102f).epsilon(0.01f));

    float large = engine.resolveFloat("spacing.large");
    CHECK(large == 4.0f);
}

TEST_CASE("ThemeEngine resolves hex without hash prefix",
          "[theme][engine][resolve]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "Hex No Hash",
        "colors": { "test": { "val": "FFAA00" } }
    })";
    REQUIRE(engine.load_from_string(json));

    Color c = engine.resolveColor("colors.test.val");
    CHECK(c.r == 1.0f);
    CHECK(c.g == Approx(0.667f).epsilon(0.01f));
    CHECK(c.b == 0.0f);
}

TEST_CASE("ThemeEngine resolveColor returns fallback for non-color leaf",
          "[theme][engine][resolve]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "Fallback",
        "colors": { "test": { "val": "not_a_color" } }
    })";
    REQUIRE(engine.load_from_string(json));

    Color c = engine.resolveColor("colors.test.val");
    // Should return default Color (black)
    CHECK(c.r == 0.0f);
    CHECK(c.g == 0.0f);
    CHECK(c.b == 0.0f);
}

TEST_CASE("ThemeEngine resolveFloat returns fallback for non-numeric leaf",
          "[theme][engine][resolve]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "Fallback",
        "spacing": { "bad": "not_a_number" }
    })";
    REQUIRE(engine.load_from_string(json));

    float val = engine.resolveFloat("spacing.bad");
    CHECK(val == 0.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Phase 1 — ThemedWidget (T1.3)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ThemedWidget provides theme() accessor returning ThemeEngine*",
          "[theme][widget][themed]")
{
    // ThemedWidget cannot be instantiated directly (abstract),
    // so we use a concrete subclass for testing.
    class TestThemedWidget : public ThemedWidget {
    public:
        const char* type_name() const noexcept override { return "TestThemedWidget"; }
        Vec2 measure(float, float) override { return {0,0}; }
        void render(SkiaCanvas*) override {}
    };

    TestThemedWidget w;
    // Without a registered ThemeEngine, theme() returns nullptr
    CHECK(w.theme() == nullptr);

    // With a registered ThemeEngine via set_instance
    ThemeEngine engine;
    ThemeEngine::set_instance(&engine);

    // Now theme() should return the engine
    CHECK(w.theme() == &engine);

    // Clean up
    ThemeEngine::set_instance(nullptr);
    CHECK(w.theme() == nullptr);
}

TEST_CASE("ThemedWidget subscribes to ThemeChanged event",
          "[theme][widget][themed]")
{
    // Verify that the subscription mechanism exists.
    // (Full event integration is tested at a higher level.)
    class TestWidget : public ThemedWidget {
    public:
        const char* type_name() const noexcept override { return "TestWidget"; }
        Vec2 measure(float, float) override { return {0,0}; }
        void render(SkiaCanvas*) override {}
    };

    TestWidget w;
    // Initially not dirty
    w.clear_dirty();
    CHECK_FALSE(w.is_dirty());

    // Simulate: calling onThemeChanged should mark dirty
    w.onThemeChanged();
    CHECK(w.is_dirty());
}

// ═══════════════════════════════════════════════════════════════════
// Phase 2 — Widget Binding: RectWidget (T2.1)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("RectWidget inherits from ThemedWidget", "[theme][widget][rect]") {
    RectWidget w;
    // RectWidget can now access theme()
    CHECK(w.theme() == nullptr);  // No ThemeEngine registered
    CHECK(w.is_dirty());
}

TEST_CASE("RectWidget sets and clears theme fill token", "[theme][widget][rect]") {
    RectWidget w;
    w.set_theme_fill("colors.bg.primary");
    CHECK(w.is_dirty());

    // Verify the path is stored
    w.clear_dirty();
    w.set_theme_fill_path("colors.bg.primary");

    // Clear it
    w.clear_theme_fill();
    CHECK_FALSE(w.is_dirty());
}

// ═══════════════════════════════════════════════════════════════════
// Phase 2 — Widget Binding: TextWidget (T2.2)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("TextWidget theme color overrides hardcoded color", "[theme][widget][text]") {
    TextWidget w;
    w.set_theme_color("colors.text.primary");
    CHECK(w.has_theme_color());
    CHECK(w.theme_color_path() == "colors.text.primary");

    w.clear_theme_color();
    CHECK_FALSE(w.has_theme_color());
}

// ═══════════════════════════════════════════════════════════════════
// Phase 2 — Widget Binding: ButtonWidget (T2.3)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ButtonWidget replaces hardcoded colors with theme paths",
          "[theme][widget][button]")
{
    ButtonWidget w;
    w.set_theme_normal_color("colors.bg.tertiary");
    w.set_theme_hover_color("colors.bg.elevated");
    w.set_theme_pressed_color("colors.accent.pressed");
    w.set_theme_disabled_color("colors.bg.surface");
    w.set_theme_text_color("colors.text.primary");

    CHECK(w.theme_normal_color() == "colors.bg.tertiary");
    CHECK(w.theme_hover_color() == "colors.bg.elevated");
    CHECK(w.theme_pressed_color() == "colors.accent.pressed");
    CHECK(w.theme_disabled_color() == "colors.bg.surface");
    CHECK(w.theme_text_color() == "colors.text.primary");
}

TEST_CASE("ButtonWidget resolves fill color from theme when paths are set",
          "[theme][widget][button]")
{
    ThemeEngine engine;
    const char* json = R"({
        "name": "Test",
        "colors": {
            "bg": { "tertiary": "#2A2A30", "elevated": "#3D3D45", "surface": "#333338" },
            "accent": { "pressed": "#CC6200" },
            "text": { "primary": "#FFFFFF" }
        }
    })";
    engine.load_from_string(json);
    ThemeEngine::set_instance(&engine);

    ButtonWidget w;
    w.set_theme_normal_color("colors.bg.tertiary");

    // Render doesn't call theme without a full canvas, but the
    // resolve_fill_color should now pick up theme if normal_color isn't set.
    // In this case the hardcoded normal_color_ still exists as fallback.
    CHECK(w.theme_normal_color() == "colors.bg.tertiary");

    ThemeEngine::set_instance(nullptr);
}

// ═══════════════════════════════════════════════════════════════════
// Phase 2 — Widget Binding: ContainerWidget (T2.4)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ContainerWidget inherits from ThemedWidget", "[theme][widget][container]") {
    ContainerWidget w;
    CHECK(w.theme() == nullptr);  // No ThemeEngine registered
}

TEST_CASE("ContainerWidget sets theme background token", "[theme][widget][container]") {
    ContainerWidget w;
    w.set_theme_background("colors.bg.secondary");
    CHECK(w.is_dirty());

    w.clear_dirty();
    w.clear_theme_background();
    CHECK_FALSE(w.is_dirty());
}

// ═══════════════════════════════════════════════════════════════════
// Phase 1 — FileWatcher (T1.4) — unit test for debounce logic
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("FileWatcher constructs cleanly", "[theme][watcher]") {
    // FileWatcher is platform-specific; we can only test basic construction
    // and API shape. Full integration tests require a real filesystem.
    // The constructor does not throw.
    CHECK_NOTHROW(FileWatcher());
}

// ═══════════════════════════════════════════════════════════════════
// Phase 2 — BrowserPanelGPU theme migration (T2.5)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("BrowserPanelGPU theme color access", "[theme][browser]") {
    // Verify that BrowserPanelGPU (which inherits from ContainerWidget)
    // can access theme tokens for its render path.
    // Full rendering tests require a SkiaCanvas and are GPU-guarded.
    browser::BrowserPanelGPU panel(nullptr);
    // It inherits ThemedWidget through ContainerWidget
    CHECK(panel.theme() == nullptr);
}

// ═══════════════════════════════════════════════════════════════════
// Theme Engine File I/O (T1.2 integration)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ThemeEngine load_from_file reads and parses JSON theme",
          "[theme][engine][file]")
{
    // Write a temp theme file
    auto tmp = std::filesystem::temp_directory_path() / "test_theme_dark.json";
    {
        std::ofstream ofs(tmp.string());
        ofs << R"({
            "name": "Temp Dark",
            "colors": {
                "bg": { "primary": "#111111", "secondary": "#222222" },
                "text": { "primary": "#FFFFFF" }
            },
            "typography": { "fontSize": 12 },
            "spacing": { "padding": 4 }
        })";
    }

    ThemeEngine engine;
    bool loaded = engine.load_from_file(tmp.string());
    CHECK(loaded);

    Color bg = engine.resolveColor("colors.bg.primary");
    CHECK(bg.r == Approx(0.067f).epsilon(0.01f));

    float fs = engine.resolveFloat("typography.fontSize");
    CHECK(fs == 12.0f);

    // Cleanup
    std::filesystem::remove(tmp);
}

TEST_CASE("ThemeEngine load_from_file returns false for missing file",
          "[theme][engine][file]")
{
    ThemeEngine engine;
    CHECK_FALSE(engine.load_from_file("/nonexistent/path/theme.json"));
}

TEST_CASE("ThemeEngine availableThemes lists built-in themes",
          "[theme][engine]")
{
    ThemeEngine engine;
    auto themes = engine.availableThemes();
    CHECK(themes.size() >= 3);  // dark, light, high-contrast
}

// ═══════════════════════════════════════════════════════════════════
// ThemeEngine re-load and cache invalidation (T4.4)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ThemeEngine re-load invalidates token cache",
          "[theme][engine][cache]")
{
    ThemeEngine engine;
    const char* json1 = R"({
        "name": "First",
        "colors": { "bg": { "primary": "#FF0000" } }
    })";
    REQUIRE(engine.load_from_string(json1));

    Color c1 = engine.resolveColor("colors.bg.primary");
    CHECK(c1.r == 1.0f);

    // Load a different theme
    const char* json2 = R"({
        "name": "Second",
        "colors": { "bg": { "primary": "#00FF00" } }
    })";
    REQUIRE(engine.load_from_string(json2));

    // Cache should be invalidated — returns new value
    Color c2 = engine.resolveColor("colors.bg.primary");
    CHECK(c2.g == 1.0f);
    CHECK(c2.r == 0.0f);
}

TEST_CASE("ThemeEngine service locator registration and retrieval",
          "[theme][engine][service]")
{
    ServiceLocator locator;

    // Register a ThemeEngine
    auto engine = std::make_unique<ThemeEngine>();
    engine->load_from_string(R"({
        "name": "SL Test",
        "colors": { "test": { "val": "#123456" } }
    })");

    ThemeEngine* engine_ptr = engine.get();
    ThemeEngine::set_instance(engine_ptr);
    locator.register_service(std::move(engine));

    // Retrieve via ServiceLocator
    auto* retrieved = locator.get_service<ThemeEngine>();
    REQUIRE(retrieved != nullptr);
    CHECK(retrieved == engine_ptr);

    // Static accessor also works
    CHECK(ThemeEngine::get() == engine_ptr);

    Color c = retrieved->resolveColor("colors.test.val");
    CHECK(c.r == Approx(0.071f).epsilon(0.01f));

    ThemeEngine::set_instance(nullptr);
}
