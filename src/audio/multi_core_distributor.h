#ifndef ARIA_MULTI_CORE_DISTRIBUTOR_H
#define ARIA_MULTI_CORE_DISTRIBUTOR_H

#include "audio_buffer.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace aria {

// ─── Forward declarations ─────────────────────────────────────
class TrackProcessor;

/// Multi-core distributor for parallel track processing.
///
/// Distributes per-track processing across available CPU cores:
///
///   Phase 1 (serial, main thread):
///     - Process input nodes
///     - Process mixer bus input
///
///   Phase 2 (parallel, worker threads):
///     - Distribute TrackProcessor::process() across core groups
///     - Balance load by estimated processing time
///
///   Phase 3 (serial, main thread):
///     - Sum results into master bus
///
/// Thread pool is created at initialization and reused across callbacks —
/// no thread creation/destruction in the audio path.
///
/// Single-core fallback: processes everything serially when disabled
/// or when only 1 core is available.
class MultiCoreDistributor {
public:
    MultiCoreDistributor();
    ~MultiCoreDistributor();

    MultiCoreDistributor(const MultiCoreDistributor&) = delete;
    MultiCoreDistributor& operator=(const MultiCoreDistributor&) = delete;

    /// Initialize the thread pool.
    /// @param num_threads  Number of worker threads (0 = auto-detect).
    /// @return true on success.
    bool init(uint32_t num_threads = 0);

    /// Shut down the thread pool.
    void shutdown();

    /// Enable or disable multi-core distribution.
    void set_enabled(bool enabled) {
        enabled_.store(enabled, std::memory_order_release);
    }

    /// Whether multi-core distribution is enabled.
    bool is_enabled() const {
        return enabled_.load(std::memory_order_relaxed) && num_workers_ > 0;
    }

    /// Distribute track processing across cores.
    /// Must be called from the audio thread.
    /// @param tracks       Vector of track processors to process.
    /// @param frames       Number of frames in this block.
    /// @param sample_pos   Global sample position.
    /// @param phase1_fn    Serial phase 1 callback (input/mixer node processing).
    /// @param phase3_fn    Serial phase 3 callback (master bus summing).
    void distribute(
        const std::vector<TrackProcessor*>& tracks,
        uint32_t frames,
        uint64_t sample_pos,
        std::function<void()> phase1_fn,
        std::function<void()> phase3_fn);

    /// Rebalance track-to-core assignment when topology changes.
    /// @param tracks  Updated vector of track processors.
    void rebalance(const std::vector<TrackProcessor*>& tracks);

    /// Number of worker threads.
    uint32_t num_workers() const { return num_workers_; }

private:
    // ─── Internal types ────────────────────────────────────────

    struct WorkerState {
        std::thread thread;
        std::mutex mtx;
        std::condition_variable cv;

        // Work assignment (protected by mtx)
        bool ready = false;
        bool done = false;
        TrackProcessor* const* tracks = nullptr;
        uint32_t track_count = 0;
        uint32_t frames = 0;
        uint64_t sample_pos = 0;
    };

    // ─── Worker thread entry point ─────────────────────────────
    static void worker_loop(MultiCoreDistributor* distributor,
                            WorkerState* state);

    // ─── State ─────────────────────────────────────────────────
    std::vector<std::unique_ptr<WorkerState>> workers_;
    std::atomic<bool> shutdown_{false};
    std::atomic<bool> enabled_{true};
    uint32_t num_workers_ = 0;
};

} // namespace aria

#endif // ARIA_MULTI_CORE_DISTRIBUTOR_H
