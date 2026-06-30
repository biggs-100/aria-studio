#ifndef ARIA_WIDGET_THEMED_H
#define ARIA_WIDGET_THEMED_H

#include "graphics/widget.h"
#include "graphics/theme_engine.h"

#include <cstdint>
#include <vector>

namespace aria {

/// Intermediate widget base class that adds theme() accessor and
/// auto-mark-dirty on ThemeChanged event.
///
/// All themed widgets should inherit from ThemedWidget instead of Widget
/// to gain:
///   - theme() accessor (backs to ThemeEngine::get())
///   - onThemeChanged() → mark_dirty() on theme switch
///   - resolve_color() and resolve_float() convenience methods
///
/// Lifecycle:
///   - ThemedWidget does NOT auto-subscribe to EventBus in its constructor
///     because EventBus is registered via ServiceLocator which may not be
///     available at widget construction time.
///   - The application calls ThemedWidget::subscribe_all() during the theme
///     system init phase to wire up all existing ThemedWidget instances.
///   - Or, the ThemeEngine broadcasts ThemeChanged via EventBus and each
///     ThemedWidget looks up the bus lazily.
class ThemedWidget : public Widget {
public:
    ThemedWidget();
    ~ThemedWidget() override;

    ThemedWidget(const ThemedWidget&) = delete;
    ThemedWidget& operator=(const ThemedWidget&) = delete;

    // ── Theme accessor ──────────────────────────────────────────

    /// Returns the application-wide ThemeEngine instance.
    /// Returns nullptr if no ThemeEngine is registered.
    ThemeEngine* theme() const noexcept { return ThemeEngine::get(); }

    // ── Theme change handler ────────────────────────────────────

    /// Called when the theme changes (via EventBus subscription).
    /// Marks this widget as dirty so it re-renders on next paint pass.
    virtual void onThemeChanged();

    // ── Convenience token resolvers ─────────────────────────────

    /// Resolve a color by dot path. Returns fallback color (black)
    /// if theme is not available or path is invalid.
    Color resolve_color(std::string_view path) {
        auto* t = theme();
        return t ? t->resolveColor(path) : Color{};
    }

    /// Resolve a float by dot path. Returns 0.0f if theme is not
    /// available or path is invalid.
    float resolve_float(std::string_view path) {
        auto* t = theme();
        return t ? t->resolveFloat(path) : 0.0f;
    }

    // ── Static lifecycle helpers ────────────────────────────────

    /// Register a ThemedWidget for automatic ThemeChanged notifications.
    /// Called during widget construction (from a central registry).
    /// The widget is automatically unregistered on destruction.
    static void register_widget(ThemedWidget* widget);

    /// Unregister a ThemedWidget (called from destructor).
    static void unregister_widget(ThemedWidget* widget);

    /// Notify all registered ThemedWidgets that the theme has changed.
    /// Called by the ThemeEngine or EventBus handler.
    static void notify_all();

private:
    /// Static registry of all ThemedWidget instances.
    static std::vector<ThemedWidget*>* registry();
};

} // namespace aria

#endif // ARIA_WIDGET_THEMED_H
