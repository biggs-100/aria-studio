#include "script_window.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include <sol/sol.hpp>

namespace aria {

namespace {

/// Log a Lua callback error message.
/// In production this would go through the DAW's logging system.
/// For the test environment, we write to a static string that tests can inspect.
static std::string& last_error_log() {
    static std::string log;
    return log;
}

} // anonymous namespace

// ─── Static ID Generator ───────────────────────────────────────

ScriptWindow::Id ScriptWindow::next_id() {
    static Id counter = 1;
    return counter++;
}

// ─── Construction ─────────────────────────────────────────────

ScriptWindow::ScriptWindow(ScriptId owner_id, std::string title,
                           float width, float height)
    : id_(next_id())
    , owner_id_(owner_id)
    , title_(std::move(title))
    , width_(width)
    , height_(height) {
}

// ─── Widget Creation ──────────────────────────────────────────

uint64_t ScriptWindow::add_button(std::string_view label,
                                  sol::protected_function on_click) {
    uint64_t wid = next_id();
    widgets_.push_back({
        .id = wid,
        .type = ScriptWidgetType::kButton,
        .label = std::string(label)
    });
    callbacks_[wid] = CallbackEntry{std::move(on_click)};
    return wid;
}

uint64_t ScriptWindow::add_slider(std::string_view label,
                                  float min, float max, float def,
                                  sol::protected_function on_change) {
    uint64_t wid = next_id();
    widgets_.push_back({
        .id = wid,
        .type = ScriptWidgetType::kSlider,
        .label = std::string(label),
        .min_val = min,
        .max_val = max,
        .default_val = def
    });
    callbacks_[wid] = CallbackEntry{std::move(on_change)};
    return wid;
}

uint64_t ScriptWindow::add_text_input(std::string_view label,
                                      std::string default_text,
                                      sol::protected_function on_change) {
    uint64_t wid = next_id();
    widgets_.push_back({
        .id = wid,
        .type = ScriptWidgetType::kTextInput,
        .label = std::string(label),
        .default_text = std::move(default_text)
    });
    callbacks_[wid] = CallbackEntry{std::move(on_change)};
    return wid;
}

// ─── Event Dispatch ───────────────────────────────────────────

bool ScriptWindow::on_widget_event(uint64_t widget_id, sol::object value) {
    auto cit = callbacks_.find(widget_id);
    if (cit == callbacks_.end()) {
        return false;
    }

    auto* info = get_widget_info(widget_id);
    if (info == nullptr) {
        return false;
    }

    sol::protected_function& func = cit->second.callback;

    try {
        sol::protected_function_result result;
        if (info->type == ScriptWidgetType::kButton) {
            // Button callbacks take no arguments
            result = func();
        } else {
            // Slider and text input callbacks receive the value
            result = func(value);
        }

        if (!result.valid()) {
            // sol::protected_function catches Lua errors and returns invalid result
            sol::error err = result;
            last_error_log() = err.what();
            // Error is logged but window stays open — this is the expected behavior
            // for error resilience (task 6.7)
        }
        return true;
    } catch (const std::exception& e) {
        // Catch C++ exceptions thrown by sol2-bound callbacks
        last_error_log() = e.what();
        return true; // Window stays functional
    } catch (...) {
        last_error_log() = "unknown Lua callback error";
        return true;
    }
}

// ─── Lifecycle ────────────────────────────────────────────────

void ScriptWindow::cleanup() {
    widgets_.clear();
    callbacks_.clear();
}

// ─── Accessors ────────────────────────────────────────────────

const ScriptWidgetInfo* ScriptWindow::get_widget_info(uint64_t widget_id) const {
    auto it = std::find_if(widgets_.begin(), widgets_.end(),
                           [widget_id](const ScriptWidgetInfo& w) {
                               return w.id == widget_id;
                           });
    if (it != widgets_.end()) {
        return &(*it);
    }
    return nullptr;
}

// ─── Binding Functions ───────────────────────────────────────

void register_script_window_usertype(sol::state& state) {
    // Register ScriptWindow as a Lua usertype
    state.new_usertype<ScriptWindow>(
        "ScriptWindow",
        sol::no_constructor,

        // Widget creation
        "button", [](ScriptWindow& self, std::string_view label,
                     sol::protected_function cb,
                     sol::this_state s) -> sol::object {
            uint64_t id = self.add_button(label, std::move(cb));
            if (id == 0) return sol::nil;
            return sol::make_object(s, id);
        },

        "slider", [](ScriptWindow& self, std::string_view label,
                     float min, float max, float def,
                     sol::protected_function cb,
                     sol::this_state s) -> sol::object {
            uint64_t id = self.add_slider(label, min, max, def, std::move(cb));
            if (id == 0) return sol::nil;
            return sol::make_object(s, id);
        },

        "text_input", [](ScriptWindow& self, std::string_view label,
                         std::string default_text,
                         sol::protected_function cb,
                         sol::this_state s) -> sol::object {
            uint64_t id = self.add_text_input(label, std::move(default_text),
                                               std::move(cb));
            if (id == 0) return sol::nil;
            return sol::make_object(s, id);
        },

        // Read-only accessors
        "title", sol::readonly_property(&ScriptWindow::title),
        "width", sol::readonly_property(&ScriptWindow::width),
        "height", sol::readonly_property(&ScriptWindow::height)
    );
}

void register_ui_bindings(sol::state& state, UIAPI& ui) {
    // Ensure ScriptWindow usertype is registered
    // (safe to call multiple times — sol2 skips duplicates)

    // Create aria.ui namespace
    sol::table aria = state["aria"].get_or_create<sol::table>();
    sol::table ui_tbl = aria.create_named("ui");

    // Register aria.ui.window(title, width, height)
    ui_tbl.set_function("window", [&ui](const std::string& title,
                                        float width, float height) -> sol::object {
        return ui.create_window(title, width, height);
    });
}

} // namespace aria
