#include "plugin_sandbox.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace aria::plugin {

// ══════════════════════════════════════════════════════════════════════
//  Construction / Destruction
// ══════════════════════════════════════════════════════════════════════

PluginSandbox::PluginSandbox(std::unique_ptr<AudioPlugin> plugin)
    : plugin_(std::move(plugin))
{
    // Create a future from the initial promise
    result_future_ = result_promise_.get_future();
}

PluginSandbox::~PluginSandbox() {
    stop();
}

// ══════════════════════════════════════════════════════════════════════
//  Lifecycle
// ══════════════════════════════════════════════════════════════════════

bool PluginSandbox::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SandboxState::Idle && state_ != SandboxState::Terminated) {
        return false;
    }

    state_ = SandboxState::Running;
    stop_requested_ = false;
    has_work_ = false;
    crashed_.store(false, std::memory_order_release);

    // Reset the promise/future pair
    result_promise_ = std::promise<bool>{};
    result_future_ = result_promise_.get_future();
    result_ready_ = false;

    worker_thread_ = std::thread(&PluginSandbox::worker_loop, this);
    return true;
}

void PluginSandbox::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        has_work_ = false;
        cv_.notify_one();
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    state_ = SandboxState::Terminated;
}

// ══════════════════════════════════════════════════════════════════════
//  Audio Processing (async)
// ══════════════════════════════════════════════════════════════════════

bool PluginSandbox::process_async(const ProcessContext& ctx,
                                   const float* const* inputs,
                                   float* const* outputs) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != SandboxState::Running) {
        return false;
    }

    // Don't queue if previous work hasn't been consumed
    if (has_work_) {
        return false;
    }

    // Clear crash flag — new work is being attempted
    crashed_.store(false, std::memory_order_release);

    // Copy context and store buffer pointers
    current_ctx_ = ctx;
    current_inputs_ = inputs;
    current_outputs_ = outputs;
    has_work_ = true;

    // Reset result state
    result_promise_ = std::promise<bool>{};
    result_future_ = result_promise_.get_future();
    result_ready_ = false;

    cv_.notify_one();
    return true;
}

bool PluginSandbox::wait_for_result(std::chrono::milliseconds timeout) {
    // Wait on the future with timeout
    auto status = result_future_.wait_for(timeout);

    if (status == std::future_status::ready) {
        bool result = false;
        try {
            result = result_future_.get();
        } catch (...) {
            // Exception from the worker — treat as crash
            handle_crash();
            return false;
        }
        return result;
    }

    // Timeout — treat as crash
    handle_crash();
    return false;
}

// ══════════════════════════════════════════════════════════════════════
//  State / Configuration
// ══════════════════════════════════════════════════════════════════════

SandboxState PluginSandbox::state() const {
    return state_.load(std::memory_order_acquire);
}

void PluginSandbox::set_timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

std::chrono::milliseconds PluginSandbox::timeout() const {
    return timeout_;
}

AudioPlugin* PluginSandbox::plugin() {
    if (crashed_.load(std::memory_order_acquire)) {
        return nullptr;
    }
    return plugin_.get();
}

bool PluginSandbox::has_crashed() const {
    return crashed_.load(std::memory_order_acquire);
}

// ══════════════════════════════════════════════════════════════════════
//  Private helpers
// ══════════════════════════════════════════════════════════════════════

void PluginSandbox::worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait for work or stop signal
        cv_.wait(lock, [this]() { return has_work_ || stop_requested_; });

        if (stop_requested_) {
            break;
        }

        // Capture work item
        ProcessContext ctx = current_ctx_;
        const float* const* inputs = current_inputs_;
        float* const* outputs = current_outputs_;
        has_work_ = false;

        lock.unlock();

        // Execute the plugin's process
        bool result = false;
        try {
            plugin_->process(ctx, inputs, outputs);
            result = true;
        } catch (...) {
            result = false;
        }

        // Signal the result
        lock.lock();
        if (!stop_requested_) {
            try {
                result_promise_.set_value(result);
            } catch (...) {
                // Promise already satisfied — ignore
            }
            result_ready_ = true;
        }
    }
}

void PluginSandbox::handle_crash() {
    // Mark as crashed
    state_.store(SandboxState::Crashed, std::memory_order_release);
    crashed_.store(true, std::memory_order_release);

    // Stop the worker thread gracefully (if it's still running)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        has_work_ = false;
        cv_.notify_one();
    }

    if (worker_thread_.joinable()) {
        worker_thread_.detach();  // Can't join a potentially wedged thread
    }

    // Create a new worker thread for future work
    state_.store(SandboxState::Running, std::memory_order_release);
    stop_requested_ = false;
    has_work_ = false;

    result_promise_ = std::promise<bool>{};
    result_future_ = result_promise_.get_future();
    result_ready_ = false;

    worker_thread_ = std::thread(&PluginSandbox::worker_loop, this);
}

} // namespace aria::plugin
