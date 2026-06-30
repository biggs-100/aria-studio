#ifndef ARIA_SCRIPT_INSTANCE_H
#define ARIA_SCRIPT_INSTANCE_H

#include <cstdint>
#include <string>

#include "lua_runtime.h"

namespace aria {

/// ScriptInstance — represents a single loaded Lua script.
///
/// Each instance owns its own LuaRuntime for full isolation.
/// The lifecycle is:
///   construct(id, filepath) → load(source) → execute() → unload()
///
/// After unload(), the VM retains its sol::state but all globals are cleared.
/// load() can be called again to reload new source.
class ScriptInstance {
public:
    using Id = uint64_t;

    explicit ScriptInstance(Id id, std::string filepath);
    ~ScriptInstance() = default;

    ScriptInstance(const ScriptInstance&) = delete;
    ScriptInstance& operator=(const ScriptInstance&) = delete;
    ScriptInstance(ScriptInstance&&) = default;
    ScriptInstance& operator=(ScriptInstance&&) = default;

    /// Unique instance identifier.
    Id id() const { return id_; }

    /// File path of the script (used for display and hot-reload matching).
    const std::string& filepath() const { return filepath_; }

    /// Load Lua source code. Returns true on success.
    bool load(const std::string& source);

    /// Execute the loaded script. Returns true on success.
    /// load() must be called before execute().
    bool execute();

    /// Unload the script: reset VM state and clear loaded source.
    void unload();

    /// Whether a script is currently loaded.
    bool is_loaded() const { return !source_.empty(); }

    /// Get the last error message from load() or execute().
    const std::string& last_error() const { return last_error_; }

    /// Access the underlying LuaRuntime (for binding installation).
    LuaRuntime& runtime() { return runtime_; }

private:
    Id id_;
    std::string filepath_;
    std::string source_;
    std::string last_error_;
    LuaRuntime runtime_;
};

} // namespace aria

#endif // ARIA_SCRIPT_INSTANCE_H
