#ifndef ARIA_PLUGIN_PLUGIN_SANDBOX_H
#define ARIA_PLUGIN_PLUGIN_SANDBOX_H

#include "audio_plugin.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace aria::plugin {

/// Sandbox states for the per-plugin worker thread.
enum class SandboxState {
    Idle,        ///< Thread not started or inactive
    Running,     ///< Processing or ready for work
    Crashed,     ///< Plugin crashed or timed out
    Terminated   ///< Explicitly stopped
};

/// Level 1 sandbox isolation for a single AudioPlugin.
///
/// Each plugin runs on its own dedicated worker thread. The main audio
/// thread queues process() requests and waits with a timeout. If the
/// timeout expires, the plugin is marked crashed and bypassed — the
/// audio thread never blocks indefinitely.
///
/// Thread safety: start(), stop(), process_async(), and wait_for_result()
/// are designed for single-threaded callers (the audio thread). The
/// worker thread processes plugin work exclusively.
class PluginSandbox {
public:
    /// Construct a sandbox that wraps the given plugin.
    /// Takes ownership of the plugin immediately.
    explicit PluginSandbox(std::unique_ptr<AudioPlugin> plugin);

    /// Destructor stops the worker thread and destroys the plugin.
    ~PluginSandbox();

    PluginSandbox(const PluginSandbox&) = delete;
    PluginSandbox& operator=(const PluginSandbox&) = delete;
    PluginSandbox(PluginSandbox&&) = delete;
    PluginSandbox& operator=(PluginSandbox&&) = delete;

    // ── Lifecycle ──────────────────────────────────────────────

    /// Start the worker thread. Must be called before process_async().
    /// Returns false if already running.
    bool start();

    /// Stop the worker thread gracefully. If a process is in flight,
    /// waits for it to finish (up to the current timeout) then stops.
    void stop();

    // ── Audio Processing (async) ───────────────────────────────

    /// Queue a process request on the worker thread. Returns immediately.
    /// @param ctx     Processing context (copied internally)
    /// @param inputs  Per-channel input arrays — MUST remain valid
    ///                until wait_for_result() returns.
    /// @param outputs Per-channel output arrays — MUST remain valid
    ///                until wait_for_result() returns.
    /// @return true if the request was queued, false if sandbox is
    ///         crashed or terminated.
    bool process_async(const ProcessContext& ctx,
                       const float* const* inputs,
                       float* const* outputs);

    /// Wait for the most recent process_async() to complete.
    /// @param timeout Maximum time to wait.
    /// @return The plugin's process() return value, or false if:
    ///         - The timeout expired (plugin marked crashed)
    ///         - The plugin threw an exception
    ///         - The sandbox was stopped
    bool wait_for_result(std::chrono::milliseconds timeout);

    // ── State / Configuration ──────────────────────────────────

    /// Current sandbox state.
    SandboxState state() const;

    /// Set the watchdog timeout for this plugin instance.
    void set_timeout(std::chrono::milliseconds timeout);

    /// Get the current watchdog timeout.
    std::chrono::milliseconds timeout() const;

    /// Access the wrapped plugin (e.g. for parameter queries).
    /// Returns nullptr if the last operation crashed.
    AudioPlugin* plugin();

    /// Whether the last operation resulted in a crash/timeout.
    bool has_crashed() const;

private:
    /// Worker thread entry point.
    void worker_loop();

    /// Handle a crash/timeout — mark state, detach old thread, create new.
    void handle_crash();

    std::unique_ptr<AudioPlugin> plugin_;
    std::atomic<SandboxState> state_{SandboxState::Idle};
    std::chrono::milliseconds timeout_{100};

    // Flag set on crash, cleared on start() or next process_async()
    std::atomic<bool> crashed_{false};

    // Worker thread
    std::thread worker_thread_;

    // Synchronisation for work queue
    std::mutex mutex_;
    std::condition_variable cv_;
    bool has_work_ = false;
    bool stop_requested_ = false;

    // Current work item (set by process_async, consumed by worker)
    ProcessContext current_ctx_{};
    const float* const* current_inputs_ = nullptr;
    float* const* current_outputs_ = nullptr;

    // Result synchronisation
    std::promise<bool> result_promise_{};
    std::future<bool> result_future_{};
    bool result_ready_ = false;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_PLUGIN_SANDBOX_H
