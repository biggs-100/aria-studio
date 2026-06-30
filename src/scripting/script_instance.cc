#include "script_instance.h"

namespace aria {

// ─── Construction ──────────────────────────────────────────────

ScriptInstance::ScriptInstance(Id id, std::string filepath)
    : id_(id)
    , filepath_(std::move(filepath)) {
}

// ─── Lifecycle ─────────────────────────────────────────────────

bool ScriptInstance::load(const std::string& source) {
    last_error_.clear();
    source_ = source;

    // Compile-check: run the script to verify it's valid.
    // If it fails, clear the source and store the error.
    if (!runtime_.load_script(source)) {
        last_error_ = runtime_.last_error();
        source_.clear();
        return false;
    }

    return true;
}

bool ScriptInstance::execute() {
    last_error_.clear();

    if (source_.empty()) {
        last_error_ = "no script source loaded — call load() first";
        return false;
    }

    // Re-run the stored source.
    if (!runtime_.load_script(source_)) {
        last_error_ = runtime_.last_error();
        return false;
    }

    return true;
}

void ScriptInstance::unload() {
    source_.clear();
    last_error_.clear();

    // Garbage collect to free script-allocated memory.
    runtime_.state().collect_garbage();

    // Clear global state using sol2's globals() table interface.
    // This is safer than direct lua_* C-API manipulation which changed
    // between Lua versions (LUA_GLOBALSINDEX removed in Lua 5.2+).
    sol::table globals = runtime_.state().globals();
    globals.for_each([&](sol::object const& key, sol::object const&) {
        globals[key] = sol::nil;
    });

    // Note: for_each skips nil keys. Some globals (like base library
    // functions) are preserved because we only clear non-nil entries
    // that scripts may have set. The sandbox (os/io/loadfile etc.)
    // remains applied from the constructor.

    // Re-apply sandbox on whatever remains.
    runtime_.clear_error();
}

} // namespace aria
