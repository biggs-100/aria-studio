#ifndef ARIA_SCRIPT_MANAGER_H
#define ARIA_SCRIPT_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "script_instance.h"
#include "script_window.h"

namespace aria {

/// ScriptManager — owns all ScriptInstances and manages their lifecycle.
///
/// Responsibilities:
/// - Create and destroy script VMs (each isolated in its own LuaRuntime)
/// - Load, execute, unload, and reload scripts
/// - Tick in the main loop for future hot-reload checks
/// - Match instances by filepath for reload events from FileWatcher
///
/// Thread safety: NOT thread-safe. Scripts run on the main thread.
class ScriptManager {
public:
    using ScriptId = ScriptInstance::Id;

    ScriptManager() = default;
    ~ScriptManager() = default;

    ScriptManager(const ScriptManager&) = delete;
    ScriptManager& operator=(const ScriptManager&) = delete;
    ScriptManager(ScriptManager&&) = delete;
    ScriptManager& operator=(ScriptManager&&) = delete;

    /// Create a new script VM with a unique ID.
    /// Returns the new ScriptId (> 0), or 0 on failure.
    ScriptId create_vm();

    /// Load Lua source into the VM identified by id.
    /// Returns true if the script compiles successfully.
    bool load(ScriptId id, const std::string& source);

    /// Execute the loaded script in the VM identified by id.
    /// load() must be called first.
    bool execute(ScriptId id);

    /// Unload the script and reset the VM state.
    void unload(ScriptId id);

    /// Called every frame from the main loop.
    /// Stub for PR 2 — will handle hot-reload checks and pending callbacks.
    void tick();

    /// Trigger reload by filepath. Called by FileWatcher (PR 3+).
    void reload(const std::string& filepath);

    /// Get a pointer to a ScriptInstance by ID (for inspection/testing).
    ScriptInstance* get_script(ScriptId id);

    /// Number of active script VMs.
    size_t instance_count() const { return instances_.size(); }

    // ── ScriptWindow Management (Phase 6: UI) ─────────────────

    /// Create and register a ScriptWindow owned by the given script.
    /// Returns a pointer to the new window, or nullptr if validation fails
    /// (empty title, invalid dimensions).
    ScriptWindow* create_script_window(ScriptId owner_id,
                                       const std::string& title,
                                       float width, float height);

    /// Get the number of windows owned by a script.
    size_t script_window_count(ScriptId owner_id) const;

private:
    std::unordered_map<ScriptId, std::unique_ptr<ScriptInstance>> instances_;
    ScriptId next_id_ = 1;

    // ── ScriptWindow storage ──────────────────────────────────
    // Windows are stored per-script and destroyed on unload.
    using WindowPtr = std::unique_ptr<ScriptWindow>;
    std::unordered_map<ScriptId, std::vector<WindowPtr>> script_windows_;
};

} // namespace aria

#endif // ARIA_SCRIPT_MANAGER_H
