#include "lua_runtime.h"

// Use lua.hpp (C++ wrapper) which wraps all Lua C-API includes in
// extern "C" {} — matching sol2's own extern "C" wrapping of the
// same headers at sol.hpp:2957. This ensures C-linkage across all
// consumers of the Lua API symbols exported by lualib.
#include <lua.hpp>

#include "macro_recorder.h"
#include "api_bindings.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace aria {

// C-callable hook for lua_sethook (must be a free function for C ABI compat).
void lua_runtime_instruction_hook(lua_State* L, lua_Debug* /*ar*/) {
    // This callback fires after every N instructions (count = instruction_limit_).
    // We terminate with an error rather than silently killing, so pcall
    // can catch it and the error message is informative.
    luaL_error(L, "sandbox: instruction limit exceeded — script terminated by watchdog");
    // luaL_error never returns — it longjmps to the nearest pcall frame.
}

// C-callable allocator for lua_setallocf.
void* lua_runtime_memory_allocf(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* self = static_cast<LuaRuntime*>(ud);

    if (nsize == 0) {
        // Free operation
        if (ptr) {
            self->allocated_bytes_ -= static_cast<int64_t>(osize);
            std::free(ptr);
        }
        return nullptr;
    }

    if (ptr == nullptr) {
        // Allocate new block
        if (self->memory_limit_ > 0 &&
            self->allocated_bytes_ + static_cast<int64_t>(nsize) >
                static_cast<int64_t>(self->memory_limit_)) {
            // Deny allocation — returns nullptr, Lua will raise a memory error
            return nullptr;
        }
        void* newptr = std::malloc(nsize);
        if (newptr) {
            self->allocated_bytes_ += static_cast<int64_t>(nsize);
        }
        return newptr;
    }

    // Realloc (resize existing block)
    int64_t delta = static_cast<int64_t>(nsize) - static_cast<int64_t>(osize);
    if (self->memory_limit_ > 0 &&
        self->allocated_bytes_ + delta > static_cast<int64_t>(self->memory_limit_)) {
        // Deny realloc — return nullptr, Lua will raise a memory error
        return nullptr;
    }
    void* newptr = std::realloc(ptr, nsize);
    if (newptr) {
        self->allocated_bytes_ += delta;
    }
    return newptr;
}

// ─── Construction ──────────────────────────────────────────────

LuaRuntime::LuaRuntime() {
    // Open only safe Lua libraries:
    //   base    — print, pairs, ipairs, type, tostring, tonumber, error, pcall, etc.
    //   table   — table.insert, table.remove, table.sort, etc.
    //   string  — string.len, string.sub, string.rep, string.format, etc.
    //   math    — math.abs, math.sin, math.max, math.min, etc.
    // Intentionally excluded for sandbox safety:
    //   io      — file I/O
    //   os      — OS commands, environment
    //   package — require, module
    //   debug   — debugging hooks, introspection (security risk)
    //   coroutine — yields break the main thread model
    state_.open_libraries(
        sol::lib::base,
        sol::lib::table,
        sol::lib::string,
        sol::lib::math
    );

    // Apply sandbox AFTER opening libraries — nils out globals that
    // might have been loaded or could be accessed indirectly.
    install_sandbox();

    // Install resource limits
    install_instruction_hook();
    install_memory_allocator();
}

// ─── Script Execution ──────────────────────────────────────────

bool LuaRuntime::load_script(const std::string& source) {
    last_error_.clear();

    if (source.empty()) {
        last_error_ = "empty script source";
        return false;
    }

    // safe_script may throw sol::error for compilation errors even though
    // it uses protected mode internally (sol2 v3.3 behavior).
    // We catch and convert to a return value for a consistent API.
    try {
        auto result = state_.safe_script(source);
        if (!result.valid()) {
            sol::error err = result;
            last_error_ = err.what();
            return false;
        }
    } catch (const sol::error& e) {
        last_error_ = e.what();
        return false;
    }

    return true;
}

// ─── Sandbox ───────────────────────────────────────────────────

void LuaRuntime::install_sandbox() {
    lua_State* L = state_.lua_state();

    // Nil out entire io table (file I/O)
    lua_pushnil(L);
    lua_setglobal(L, "io");

    // Nil out entire os table (OS commands)
    lua_pushnil(L);
    lua_setglobal(L, "os");

    // Nil out file-loading functions from base library
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");

    lua_pushnil(L);
    lua_setglobal(L, "dofile");

    // Nil out module/require system
    lua_pushnil(L);
    lua_setglobal(L, "require");

    lua_pushnil(L);
    lua_setglobal(L, "module");

    // Nil out package table (if somehow loaded)
    lua_pushnil(L);
    lua_setglobal(L, "package");
}

// ─── Instruction Hook ──────────────────────────────────────────

void LuaRuntime::install_instruction_hook() {
    if (instruction_limit_ > 0) {
        lua_sethook(state_.lua_state(), lua_runtime_instruction_hook,
                    LUA_MASKCOUNT, instruction_limit_);
    }
}

// ─── Memory Allocator ──────────────────────────────────────────

void LuaRuntime::install_memory_allocator() {
    if (memory_limit_ > 0) {
        // lua_setallocf replaces the default allocator with our tracking one.
        // The 'ud' parameter (user data) is set to this runtime instance.
        lua_setallocf(state_.lua_state(), lua_runtime_memory_allocf, this);
    }
}

// ─── API Bindings with ActionRegistry ──────────────────────────

void LuaRuntime::install_bindings(ActionRegistry& registry) {
    // Register all DAW action handlers in the ActionRegistry.
    // These are invoked by the Lua bindings through dispatch().
    // The ActionRegistry handles macro capture via its hook.

    // Transport actions
    registry.register_action("transport.play",
        [](const nlohmann::json& /*params*/) {});
    registry.register_action("transport.stop",
        [](const nlohmann::json& /*params*/) {});
    registry.register_action("transport.record",
        [](const nlohmann::json& /*params*/) {});
    registry.register_action("transport.is_playing",
        [](const nlohmann::json& /*params*/) {});

    // Track actions
    registry.register_action("tracks.list",
        [](const nlohmann::json& /*params*/) {});
    registry.register_action("tracks.get",
        [](const nlohmann::json& /*params*/) {});

    // Clip actions
    registry.register_action("clips.move",
        [](const nlohmann::json& /*params*/) {});
    registry.register_action("clips.trim",
        [](const nlohmann::json& /*params*/) {});

    // Timing query actions
    registry.register_action("timing.position",
        [](const nlohmann::json& /*params*/) {});
    registry.register_action("timing.tempo",
        [](const nlohmann::json& /*params*/) {});
    registry.register_action("timing.beats_to_seconds",
        [](const nlohmann::json& /*params*/) {});

    // Create TransportAPI callbacks that dispatch through ActionRegistry.
    // Owned by unique_ptr members so they outlive the sol2 bindings.
    transport_api_ = std::make_unique<TransportAPI>();
    transport_api_->play = [&registry]() {
        registry.dispatch("transport.play", nlohmann::json::object());
    };
    transport_api_->stop = [&registry]() {
        registry.dispatch("transport.stop", nlohmann::json::object());
    };
    transport_api_->record = [&registry]() {
        registry.dispatch("transport.record", nlohmann::json::object());
    };
    transport_api_->is_playing = [&registry]() -> bool {
        registry.dispatch("transport.is_playing", nlohmann::json::object());
        return false;
    };

    track_api_ = std::make_unique<TrackAPI>();
    track_api_->list = [&registry]() -> std::vector<TrackInfo> {
        registry.dispatch("tracks.list", nlohmann::json::object());
        return {};
    };
    track_api_->get = [&registry](const std::string& name)
        -> std::optional<TrackInfo> {
        nlohmann::json params = {{"name", name}};
        registry.dispatch("tracks.get", params);
        return std::nullopt;
    };

    clip_api_ = std::make_unique<ClipAPI>();
    clip_api_->move = [&registry](int64_t clip_id, int64_t track,
                                   double beat) -> bool {
        nlohmann::json params = {{"clip_id", clip_id},
                                 {"track", track},
                                 {"beat", beat}};
        registry.dispatch("clips.move", params);
        return track >= 0;
    };
    clip_api_->trim = [&registry](int64_t clip_id, double start,
                                   double end) -> bool {
        nlohmann::json params = {{"clip_id", clip_id},
                                 {"start", start},
                                 {"end", end}};
        registry.dispatch("clips.trim", params);
        return start < end;
    };
    clip_api_->move_by_name = [&registry](const std::string& name,
                                           int64_t track,
                                           double beat) -> bool {
        nlohmann::json params = {{"name", name},
                                 {"track", track},
                                 {"beat", beat}};
        registry.dispatch("clips.move", params);
        return track >= 0;
    };

    timing_api_ = std::make_unique<TimingAPI>();
    timing_api_->position = [&registry]() -> double {
        registry.dispatch("timing.position", nlohmann::json::object());
        return 0.0;
    };
    timing_api_->tempo = [&registry]() -> double {
        registry.dispatch("timing.tempo", nlohmann::json::object());
        return 120.0;
    };
    timing_api_->beats_to_seconds = [&registry](double beats) -> double {
        nlohmann::json params = {{"beats", beats}};
        registry.dispatch("timing.beats_to_seconds", params);
        return (beats / 120.0) * 60.0;
    };

    // Install the sol2 bindings using owned API structs
    register_api_bindings(state_, *transport_api_, *track_api_,
                          *clip_api_, *timing_api_);
}

} // namespace aria
