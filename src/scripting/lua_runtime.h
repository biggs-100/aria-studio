#ifndef ARIA_LUA_RUNTIME_H
#define ARIA_LUA_RUNTIME_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sol/sol.hpp>
#include <string>

#include "api_bindings.h"
#include "macro_recorder.h"

namespace aria {

/// LuaRuntime — wraps sol::state with sandbox hooks, instruction limit,
/// and memory limit for safe Lua script execution.
///
/// Design:
/// - Instruction limit via lua_sethook (1M default, watchdog on violation)
/// - Memory limit via lua_setallocf (16 MB default, terminate on exceed)
/// - Sandbox: strips os/io/loadfile/dofile/require globals
/// - Every script gets its own runtime for isolation
class LuaRuntime {
public:
    LuaRuntime();
    ~LuaRuntime() = default;

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;
    LuaRuntime(LuaRuntime&&) = delete;
    LuaRuntime& operator=(LuaRuntime&&) = delete;

    /// Execute a Lua script string under the sandbox.
    /// Returns true if the script compiles and runs without error.
    /// On failure, stores the error in last_error().
    bool load_script(const std::string& source);

    /// Get the last error message from a failed load_script() call.
    const std::string& last_error() const { return last_error_; }

    /// Clear the last error.
    void clear_error() { last_error_.clear(); }

    /// Access the underlying sol::state (for binding installation).
    sol::state& state() { return state_; }

    /// Access the raw lua_State* for C-API operations.
    lua_State* lua_state() { return state_.lua_state(); }

    /// Install all aria.* API bindings and register them in the
    /// ActionRegistry for macro capture.
    void install_bindings(ActionRegistry& registry);

    /// Set the instruction limit (0 to disable). Default: 1,000,000.
    void set_instruction_limit(int limit) { instruction_limit_ = limit; }

    /// Get current instruction limit.
    int instruction_limit() const { return instruction_limit_; }

    /// Set the memory limit in bytes (0 to disable). Default: 16 MB.
    void set_memory_limit(size_t max_bytes) { memory_limit_ = max_bytes; }

    /// Get current memory limit.
    size_t memory_limit() const { return memory_limit_; }

    /// Get current tracked allocated bytes.
    int64_t allocated_bytes() const { return allocated_bytes_; }

private:
    sol::state state_;
    std::string last_error_;
    int instruction_limit_ = 1000000;
    size_t memory_limit_ = 16 * 1024 * 1024;
    int64_t allocated_bytes_ = 0;

    /// Owned API callback structs — must outlive sol2 bindings.
    std::unique_ptr<TransportAPI> transport_api_;
    std::unique_ptr<TrackAPI> track_api_;
    std::unique_ptr<ClipAPI> clip_api_;
    std::unique_ptr<TimingAPI> timing_api_;

    /// Install sandbox: nil out dangerous globals (io, os, loadfile, etc.).
    void install_sandbox();

    /// Install the instruction count hook via lua_sethook.
    void install_instruction_hook();

    /// Install the custom memory allocator via lua_setallocf.
    void install_memory_allocator();

    // Callbacks are free functions in lua_runtime.cc (needed for C ABI compatibility
    // with lua_Hook and lua_Alloc function pointer types).
    friend void lua_runtime_instruction_hook(lua_State* L, lua_Debug* ar);
    friend void* lua_runtime_memory_allocf(void* ud, void* ptr, size_t osize, size_t nsize);
};

} // namespace aria

#endif // ARIA_LUA_RUNTIME_H
