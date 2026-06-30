#ifndef ARIA_MACRO_RECORDER_H
#define ARIA_MACRO_RECORDER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace aria {

// ── Forward declarations ────────────────────────────────────────

/// ActionRegistry — stores named DAW action handlers for macro capture.
///
/// Handlers are std::function<void(const nlohmann::json&)> keyed by
/// action name strings (e.g. "transport.play", "track.create").
///
/// When a capture hook is set (via set_capture_hook), every dispatch()
/// call also invokes the hook with the action name and params. This is
/// used by MacroRecorder to capture actions during a recording session.
class ActionRegistry {
public:
    using Handler = std::function<void(const nlohmann::json&)>;

    ActionRegistry() = default;
    ~ActionRegistry() = default;

    ActionRegistry(const ActionRegistry&) = delete;
    ActionRegistry& operator=(const ActionRegistry&) = delete;
    ActionRegistry(ActionRegistry&&) = default;
    ActionRegistry& operator=(ActionRegistry&&) = default;

    /// Register an action handler. If the name already exists, the
    /// handler is replaced (no error).
    void register_action(std::string_view name, Handler handler);

    /// Unregister an action. Safe to call on non-existent names.
    void unregister(std::string_view name);

    /// Find a handler by name. Returns nullptr if not found.
    Handler* find(std::string_view name);

    /// List all registered action names.
    std::vector<std::string> list() const;

    /// Set the capture hook for macro recording.
    /// The hook is called with (action_name, params) on every dispatch().
    /// Pass nullptr to disable capture.
    void set_capture_hook(
        std::function<void(std::string_view, nlohmann::json)> hook);

    /// Dispatch an action by name with the given params.
    /// Calls the handler if found. If a capture hook is set, also
    /// invokes the hook with the action name and params.
    void dispatch(std::string_view name, const nlohmann::json& params);

private:
    std::unordered_map<std::string, Handler,
                       std::hash<std::string>, std::equal_to<>>
        handlers_;
    std::function<void(std::string_view, nlohmann::json)> capture_hook_;
};

// ── Captured Action ─────────────────────────────────────────────

/// A single captured action with timestamp for macro serialization.
struct CapturedAction {
    std::string action_name;
    nlohmann::json params;
    /// Milliseconds since session start.
    std::chrono::milliseconds timestamp_ms{0};
};

// ── MacroRecorder ───────────────────────────────────────────────

/// MacroRecorder — captures DAW actions during a recording session and
/// serializes them to Lua scripts.
///
/// Usage:
///   MacroRecorder recorder(registry);
///   recorder.start();         // begins capturing
///   // ... user performs actions ...
///   recorder.stop();          // ends session
///   std::string lua = recorder.to_lua_script();  // serialize
///   recorder.save("MyMacro"); // persist to disk
class MacroRecorder {
public:
    explicit MacroRecorder(ActionRegistry& registry);
    ~MacroRecorder() = default;

    MacroRecorder(const MacroRecorder&) = delete;
    MacroRecorder& operator=(const MacroRecorder&) = delete;
    MacroRecorder(MacroRecorder&&) = default;
    MacroRecorder& operator=(MacroRecorder&&) = default;

    /// Start a new recording session. If already recording, the
    /// existing session is discarded and a new empty one begins.
    void start();

    /// Stop the current recording session.
    /// Safe to call when not recording.
    void stop();

    /// Whether a recording session is currently active.
    bool is_recording() const { return recording_; }

    /// Get the list of captured actions from the current/last session.
    const std::vector<CapturedAction>& actions() const { return actions_; }

    /// Serialize captured actions into a Lua script string.
    /// Inserts aria.timing.wait() calls between actions with non-zero
    /// gaps. Returns empty string if no actions captured.
    std::string to_lua_script() const;

    /// Save the captured actions as a named .lua file in the user
    /// macros directory. Returns true on success.
    bool save(const std::string& name) const;

    /// Load a named .lua file from the user macros directory and
    /// return its contents. Returns empty string with error logged
    /// if file does not exist or cannot be read.
    std::string load(const std::string& name) const;

    /// Get the user macros directory path.
    static std::string macros_directory();

private:
    ActionRegistry& registry_;
    bool recording_ = false;
    std::vector<CapturedAction> actions_;
    std::chrono::steady_clock::time_point session_start_;

    /// Capture hook callback — records an action.
    void on_action_captured(std::string_view name,
                            const nlohmann::json& params);
};

} // namespace aria

#endif // ARIA_MACRO_RECORDER_H
