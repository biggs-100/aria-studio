#include "script_manager.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace aria {

// ─── Lifecycle ─────────────────────────────────────────────────

ScriptManager::ScriptId ScriptManager::create_vm() {
    ScriptId id = next_id_++;
    auto instance = std::make_unique<ScriptInstance>(id, "");
    instances_[id] = std::move(instance);
    return id;
}

bool ScriptManager::load(ScriptId id, const std::string& source) {
    auto it = instances_.find(id);
    if (it == instances_.end()) {
        return false;
    }
    return it->second->load(source);
}

bool ScriptManager::execute(ScriptId id) {
    auto it = instances_.find(id);
    if (it == instances_.end()) {
        return false;
    }
    return it->second->execute();
}

void ScriptManager::unload(ScriptId id) {
    auto it = instances_.find(id);
    if (it != instances_.end()) {
        it->second->unload();
    }

    // ── Destroy all ScriptWindows owned by this script ────────
    auto wit = script_windows_.find(id);
    if (wit != script_windows_.end()) {
        // Clean up each window before destroying
        for (auto& w : wit->second) {
            if (w) {
                w->cleanup();
            }
        }
        wit->second.clear();
        script_windows_.erase(wit);
    }
}

// ─── Main Loop Hook ───────────────────────────────────────────

void ScriptManager::tick() {
    // PR 2: No-op tick.
    //
    // In PR 3+, tick() will:
    // 1. Process hot-reload triggers queued by FileWatcher
    // 2. Dispatch pending Lua callback invocations (ScriptWindow)
    // 3. Check for scripts marked for deferred unload
    //
    // For now, iterating over instances_ to prove the hook is called
    // and doesn't crash is sufficient.
}

// ─── Hot-Reload ───────────────────────────────────────────────

void ScriptManager::reload(const std::string& filepath) {
    // Find all ScriptInstances whose filepath matches the given path
    for (auto& [id, instance] : instances_) {
        if (instance->filepath() == filepath) {
            // Read the file from disk
            std::ifstream file(filepath);
            if (!file.is_open()) {
                // File may have been deleted — hot-reload will skip silently
                continue;
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string source = buffer.str();

            // Reload the script (parse + execute)
            if (!instance->load(source)) {
                // Compile error — log is handled by the instance
                continue;
            }
            instance->execute();
        }
    }
}

// ─── Accessors ────────────────────────────────────────────────

ScriptInstance* ScriptManager::get_script(ScriptId id) {
    auto it = instances_.find(id);
    return (it != instances_.end()) ? it->second.get() : nullptr;
}

// ─── ScriptWindow Management ────────────────────────────────────

ScriptWindow* ScriptManager::create_script_window(ScriptId owner_id,
                                                    const std::string& title,
                                                    float width, float height) {
    // Validate parameters
    if (title.empty() || width <= 0.0f || height <= 0.0f) {
        return nullptr;
    }

    auto window = std::make_unique<ScriptWindow>(owner_id, title, width, height);
    ScriptWindow* raw = window.get();

    script_windows_[owner_id].push_back(std::move(window));
    return raw;
}

size_t ScriptManager::script_window_count(ScriptId owner_id) const {
    auto it = script_windows_.find(owner_id);
    if (it != script_windows_.end()) {
        return it->second.size();
    }
    return 0;
}

} // namespace aria
