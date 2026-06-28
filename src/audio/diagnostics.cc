#include "diagnostics.h"

#include <algorithm>
#include <cmath>

namespace aria {

void AudioDiagnostics::begin_callback() {
    callback_start_ = std::chrono::high_resolution_clock::now();
    performing_.store(true, std::memory_order_release);
}

void AudioDiagnostics::end_callback() {
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - callback_start_).count();

    // Update statistics
    callback_count_.fetch_add(1, std::memory_order_relaxed);
    total_callback_time_us_.fetch_add(static_cast<uint64_t>(elapsed_us),
                                       std::memory_order_relaxed);

    // Track max
    double elapsed_ms = static_cast<double>(elapsed_us) / 1000.0;
    double current_max = max_callback_time_ms_.load(std::memory_order_relaxed);
    while (elapsed_ms > current_max) {
        if (max_callback_time_ms_.compare_exchange_weak(
                current_max, elapsed_ms,
                std::memory_order_release,
                std::memory_order_relaxed)) {
            break;
        }
    }

    // Detect x-run
    detect_x_run(static_cast<uint64_t>(elapsed_us));

    performing_.store(false, std::memory_order_release);
}

double AudioDiagnostics::avg_callback_time_ms() const {
    uint64_t count = callback_count_.load(std::memory_order_relaxed);
    if (count == 0) return 0.0;

    uint64_t total = total_callback_time_us_.load(std::memory_order_relaxed);
    return static_cast<double>(total) / static_cast<double>(count) / 1000.0;
}

void AudioDiagnostics::reset() {
    x_run_count_.store(0, std::memory_order_release);
    max_callback_time_ms_.store(0.0, std::memory_order_release);
    total_callback_time_us_.store(0, std::memory_order_release);
    callback_count_.store(0, std::memory_order_release);
    performing_.store(false, std::memory_order_release);
    burst_detected_.store(false, std::memory_order_release);
    x_run_index_ = 0;
    for (auto& ts : x_run_timestamps_) {
        ts = 0;
    }
}

void AudioDiagnostics::detect_x_run(uint64_t callback_us) {
    uint64_t buffer_dur = buffer_duration_us_.load(std::memory_order_relaxed);

    // No buffer duration configured — treat as no x-run detection
    if (buffer_dur == 0) return;

    if (callback_us <= buffer_dur) {
        // No x-run
        return;
    }

    // X-run detected
    x_run_count_.fetch_add(1, std::memory_order_relaxed);

    // Burst detection: record timestamp of this x-run
    // We use a simple sliding window of the last kBurstThreshold x-runs.
    // If the window covers < 1 second, we have a burst.
    uint32_t idx = x_run_index_ % kBurstThreshold;
    uint64_t now_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count());

    // If we have filled the window and the oldest x-run is within 1s, burst
    if (x_run_index_ >= kBurstThreshold) {
        uint64_t oldest = x_run_timestamps_[idx];
        if (now_us - oldest < kBurstWindowUs) {
            burst_detected_.store(true, std::memory_order_release);
        }
    }

    x_run_timestamps_[idx] = now_us;
    x_run_index_++;
}

} // namespace aria
