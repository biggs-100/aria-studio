#ifndef ARIA_SCRIPT_WINDOW_H
#define ARIA_SCRIPT_WINDOW_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <sol/sol.hpp>

namespace aria {

// ── ScriptWidgetType ────────────────────────────────────────────

/// Widget types that can be created by scripts via ScriptWindow.
enum class ScriptWidgetType {
    kButton,
    kSlider,
    kTextInput,
};

// ── ScriptWidgetInfo ─────────────────────────────────────────────

/// Descriptor for a widget created by a script.
struct ScriptWidgetInfo {
    uint64_t id = 0;
    ScriptWidgetType type = ScriptWidgetType::kButton;
    std::string label;

    // Slider-specific
    float min_val = 0.0f;
    float max_val = 100.0f;
    float default_val = 50.0f;

    // Text-input-specific
    std::string default_text;
};

// ── ScriptWindow ─────────────────────────────────────────────────

/// ScriptWindow — proxy window created by Lua scripts via aria.ui.window().
///
/// Stores a list of script-created widgets (buttons, sliders, text inputs)
/// and their associated Lua callbacks. Provides safe callback dispatch
/// with error resilience: Lua callback errors are caught and logged,
/// the window remains functional.
///
/// Windows are tracked by ScriptManager for auto-cleanup on script unload.
///
/// NOTE: This class does NOT inherit from Widget to avoid a link-time
/// dependency on aria_core (graphics/widget.h). In production, the
/// window manager creates a native Widget subtree from the ScriptWindow's
/// widget descriptors.
class ScriptWindow {
public:
    using Id = uint64_t;
    using ScriptId = uint64_t;

    ScriptWindow(ScriptId owner_id, std::string title, float width, float height);
    ~ScriptWindow() = default;

    ScriptWindow(const ScriptWindow&) = delete;
    ScriptWindow& operator=(const ScriptWindow&) = delete;
    ScriptWindow(ScriptWindow&&) = default;
    ScriptWindow& operator=(ScriptWindow&&) = default;

    // ── Widget creation ────────────────────────────────────────

    /// Add a button widget. Returns the widget ID, or 0 on failure.
    uint64_t add_button(std::string_view label, sol::protected_function on_click);

    /// Add a slider widget. Returns the widget ID, or 0 on failure.
    uint64_t add_slider(std::string_view label,
                        float min, float max, float def,
                        sol::protected_function on_change);

    /// Add a text input widget. Returns the widget ID, or 0 on failure.
    uint64_t add_text_input(std::string_view label,
                            std::string default_text,
                            sol::protected_function on_change);

    // ── Event dispatch ─────────────────────────────────────────

    /// Dispatch an event for the widget with the given ID.
    /// For buttons, the callback is called with no arguments.
    /// For sliders/text inputs, the callback receives the value.
    /// Returns true if the widget was found and callback dispatched.
    /// If the callback throws, the error is caught and logged.
    bool on_widget_event(uint64_t widget_id, sol::object value = sol::nil);

    // ── Lifecycle ──────────────────────────────────────────────

    /// Release all resources and clear widget list.
    void cleanup();

    // ── Accessors ──────────────────────────────────────────────

    ScriptId owner_id() const noexcept { return owner_id_; }
    const std::string& title() const noexcept { return title_; }
    float width() const noexcept { return width_; }
    float height() const noexcept { return height_; }
    Id id() const noexcept { return id_; }
    size_t widget_count() const noexcept { return widgets_.size(); }

    /// Get info about a widget by ID. Returns nullptr if not found.
    const ScriptWidgetInfo* get_widget_info(uint64_t widget_id) const;

private:
    Id id_;
    ScriptId owner_id_;
    std::string title_;
    float width_ = 0.0f;
    float height_ = 0.0f;

    // Per-widget callback storage
    struct CallbackEntry {
        sol::protected_function callback;
    };

    std::vector<ScriptWidgetInfo> widgets_;
    std::unordered_map<uint64_t, CallbackEntry> callbacks_;

    static Id next_id();
};

// ── UIAPI ────────────────────────────────────────────────────────

/// UIAPI — callbacks that the UI bindings invoke.
/// Tests provide mock implementations to verify binding correctness.
struct UIAPI {
    /// Create a ScriptWindow. Returns the window as a sol::object,
    /// or sol::nil on validation failure.
    /// The factory is responsible for validating title/dimensions.
    std::function<sol::object(const std::string& title,
                               float width, float height)> create_window;
};

// ── Binding functions ───────────────────────────────────────────

/// Register ScriptWindow as a Lua usertype in the given sol::state.
/// This must be called before register_ui_bindings().
void register_script_window_usertype(sol::state& state);

/// Register all `aria.ui.*` Lua API bindings.
void register_ui_bindings(sol::state& state, UIAPI& ui);

} // namespace aria

#endif // ARIA_SCRIPT_WINDOW_H
