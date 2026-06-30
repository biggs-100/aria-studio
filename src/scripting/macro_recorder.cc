#include "macro_recorder.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace aria {

// ══════════════════════════════════════════════════════════════════
// ActionRegistry
// ══════════════════════════════════════════════════════════════════

void ActionRegistry::register_action(std::string_view name, Handler handler) {
    handlers_[std::string(name)] = std::move(handler);
}

void ActionRegistry::unregister(std::string_view name) {
    handlers_.erase(std::string(name));
}

ActionRegistry::Handler* ActionRegistry::find(std::string_view name) {
    auto it = handlers_.find(std::string(name));
    if (it != handlers_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> ActionRegistry::list() const {
    std::vector<std::string> names;
    names.reserve(handlers_.size());
    for (const auto& [name, _] : handlers_) {
        names.push_back(name);
    }
    return names;
}

void ActionRegistry::set_capture_hook(
    std::function<void(std::string_view, nlohmann::json)> hook) {
    capture_hook_ = std::move(hook);
}

void ActionRegistry::dispatch(std::string_view name,
                               const nlohmann::json& params) {
    // Invoke capture hook if set (before handler to record intent)
    if (capture_hook_) {
        capture_hook_(name, params);
    }

    // Call the handler if registered
    auto it = handlers_.find(std::string(name));
    if (it != handlers_.end()) {
        it->second(params);
    }
}

// ══════════════════════════════════════════════════════════════════
// MacroRecorder
// ══════════════════════════════════════════════════════════════════

MacroRecorder::MacroRecorder(ActionRegistry& registry)
    : registry_(registry) {
}

void MacroRecorder::start() {
    // Discard existing session and begin a new one
    actions_.clear();
    recording_ = true;
    session_start_ = std::chrono::steady_clock::now();

    // Register the capture hook
    registry_.set_capture_hook(
        [this](std::string_view name, const nlohmann::json& params) {
            on_action_captured(name, params);
        });
}

void MacroRecorder::stop() {
    recording_ = false;
    // Detach the capture hook
    registry_.set_capture_hook(nullptr);
}

void MacroRecorder::on_action_captured(std::string_view name,
                                        const nlohmann::json& params) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - session_start_);

    CapturedAction action;
    action.action_name = std::string(name);
    action.params = params;
    action.timestamp_ms = elapsed;
    actions_.push_back(std::move(action));
}

// ─── Serialization ──────────────────────────────────────────────

/// Convert a JSON value to a Lua literal string representation.
/// Supports null, bool, number, string, array, and object types.
static std::string json_to_lua(const nlohmann::json& j) {
    if (j.is_null()) {
        return "nil";
    }
    if (j.is_boolean()) {
        return j.get<bool>() ? "true" : "false";
    }
    if (j.is_number_integer()) {
        return std::to_string(j.get<int64_t>());
    }
    if (j.is_number_float()) {
        return std::to_string(j.get<double>());
    }
    if (j.is_string()) {
        std::string s = j.get<std::string>();
        // Escape Lua string characters
        size_t pos = 0;
        while ((pos = s.find('\\', pos)) != std::string::npos) {
            s.replace(pos, 1, "\\\\");
            pos += 2;
        }
        pos = 0;
        while ((pos = s.find('"', pos)) != std::string::npos) {
            s.replace(pos, 1, "\\\"");
            pos += 2;
        }
        return "\"" + s + "\"";
    }
    if (j.is_array()) {
        std::string result = "{";
        for (size_t i = 0; i < j.size(); ++i) {
            if (i > 0) result += ", ";
            result += json_to_lua(j[i]);
        }
        result += "}";
        return result;
    }
    if (j.is_object()) {
        std::string result = "{";
        bool first = true;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (!first) result += ", ";
            first = false;
            // Lua 5.4 string key syntax
            result += "[\"" + it.key() + "\"] = ";
            result += json_to_lua(it.value());
        }
        result += "}";
        return result;
    }
    return "nil";
}

/// Convert an action name like "transport.play" to a Lua call like
/// "aria.transport.play(...)".
static std::string action_name_to_lua(const std::string& action_name) {
    // The action name is the dotted path within the aria namespace.
    // E.g., "transport.play" → "aria.transport.play"
    //       "clip.move"     → "aria.clips.move"
    return "aria." + action_name;
}

std::string MacroRecorder::to_lua_script() const {
    if (actions_.empty()) {
        return "";
    }

    std::ostringstream ss;
    ss << "-- Macro recorded " << actions_.size() << " actions\n\n";

    std::chrono::milliseconds last_time{0};

    for (const auto& action : actions_) {
        // Insert timing wait if there's a gap
        auto gap = action.timestamp_ms - last_time;
        if (gap.count() > 5) {
            ss << "  aria.timing.wait(" << gap.count() << ")\n";
        }

        // Build the Lua function call
        ss << "  " << action_name_to_lua(action.action_name);

        // Append params if present
        if (!action.params.is_null() && !action.params.empty()) {
            ss << "(" << json_to_lua(action.params) << ")";
        } else {
            ss << "()";
        }
        ss << "\n";

        last_time = action.timestamp_ms;
    }

    return ss.str();
}

// ─── Storage ────────────────────────────────────────────────────

static std::filesystem::path default_macros_dir() {
    // Use an environment variable if set, otherwise a reasonable default
    const char* env = std::getenv("ARIA_MACROS_DIR");
    if (env && env[0] != '\0') {
        return std::filesystem::path(env);
    }

    // Default: <user_home>/ARIA/Macros/
    const char* home = std::getenv("HOME");
#ifdef _WIN32
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        return std::filesystem::path(userprofile) / "ARIA" / "Macros";
    }
#else
    if (home) {
        return std::filesystem::path(home) / "ARIA" / "Macros";
    }
#endif
    // Last resort fallback
    return std::filesystem::path(".") / "ARIA_Macros";
}

std::string MacroRecorder::macros_directory() {
    return default_macros_dir().string();
}

bool MacroRecorder::save(const std::string& name) const {
    auto dir = default_macros_dir();
    std::error_code ec;

    // Create directory if it doesn't exist
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return false;
    }

    auto filepath = dir / (name + ".lua");
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::string script = to_lua_script();
    file.write(script.data(), static_cast<std::streamsize>(script.size()));
    return file.good();
}

std::string MacroRecorder::load(const std::string& name) const {
    auto dir = default_macros_dir();
    auto filepath = dir / (name + ".lua");

    std::ifstream file(filepath);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

} // namespace aria
