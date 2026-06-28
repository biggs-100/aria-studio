#include "multi_core_distributor.h"
#include "track_processor.h"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

namespace aria {

MultiCoreDistributor::MultiCoreDistributor() = default;

MultiCoreDistributor::~MultiCoreDistributor() {
    shutdown();
}

bool MultiCoreDistributor::init(uint32_t num_threads) {
    // If already initialized, shut down first
    if (!workers_.empty()) {
        shutdown();
    }

    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads > 1) {
            num_threads -= 1;
        }
    }

    if (num_threads == 0) {
        num_threads = 1;
    }

    num_workers_ = num_threads;
    workers_.reserve(num_workers_);

    // Create all worker objects first, then launch threads
    for (uint32_t i = 0; i < num_workers_; ++i) {
        workers_.push_back(std::make_unique<WorkerState>());
    }

    for (uint32_t i = 0; i < num_workers_; ++i) {
        WorkerState* ws = workers_[i].get();
        ws->thread = std::thread([this, ws]() { worker_loop(this, ws); });
    }

    return true;
}

void MultiCoreDistributor::shutdown() {
    // Set shutdown flag BEFORE waking workers.
    // The sequential consistency ensures visibility across threads.
    shutdown_.store(true, std::memory_order_seq_cst);

    // Wake all workers — they'll check the shutdown flag in their wait predicate
    for (auto& w : workers_) {
        if (w) {
            {
                std::lock_guard<std::mutex> lock(w->mtx);
                w->ready = true;
            }
            w->cv.notify_one();
        }
    }

    // Join all worker threads
    for (auto& w : workers_) {
        if (w && w->thread.joinable()) {
            w->thread.join();
        }
    }

    workers_.clear();
    num_workers_ = 0;
}

void MultiCoreDistributor::distribute(
    const std::vector<TrackProcessor*>& tracks,
    uint32_t frames,
    uint64_t sample_pos,
    std::function<void()> phase1_fn,
    std::function<void()> phase3_fn)
{
    // Phase 1 (serial): input/mixer node processing
    if (phase1_fn) {
        phase1_fn();
    }

    if (is_enabled() && tracks.size() > 1 && !workers_.empty()) {
        // Distribute tracks across workers
        uint32_t num_tracks = static_cast<uint32_t>(tracks.size());
        uint32_t tracks_per_worker = num_tracks / num_workers_;
        uint32_t remainder = num_tracks % num_workers_;
        uint32_t track_idx = 0;

        for (uint32_t w = 0; w < num_workers_; ++w) {
            auto& worker = workers_[w];
            if (!worker) continue;

            uint32_t count = tracks_per_worker + (w < remainder ? 1 : 0);

            {
                std::lock_guard<std::mutex> lock(worker->mtx);
                worker->tracks = tracks.data() + track_idx;
                worker->track_count = count;
                worker->frames = frames;
                worker->sample_pos = sample_pos;
                worker->done = false;     // reset completion flag for this cycle
                worker->ready = true;
            }
            worker->cv.notify_one();

            track_idx += count;
        }

        // Process any remaining tracks on main thread
        while (track_idx < num_tracks) {
            TrackProcessor* tp = tracks[track_idx];
            if (tp) {
                tp->process(frames, sample_pos, nullptr, nullptr);
            }
            track_idx++;
        }

        // Wait for all workers to complete
        for (auto& w : workers_) {
            if (w) {
                std::unique_lock<std::mutex> lock(w->mtx);
                w->cv.wait(lock, [&w]() { return !w->ready || w->done; });
            }
        }
    } else {
        // Single-core fallback: process all tracks serially
        for (auto* tp : tracks) {
            if (tp) {
                tp->process(frames, sample_pos, nullptr, nullptr);
            }
        }
    }

    // Phase 3 (serial): master bus summing
    if (phase3_fn) {
        phase3_fn();
    }
}

void MultiCoreDistributor::rebalance(const std::vector<TrackProcessor*>& /*tracks*/) {
    // Simple round-robin — assigned per-call in distribute()
}

void MultiCoreDistributor::worker_loop(MultiCoreDistributor* distributor,
                                        WorkerState* state)
{
    while (true) {
        // Wait for work OR shutdown.
        // The predicate includes shutdown_ so that even if shutdown() sets the
        // flag after we release the mutex but before we re-acquire it, we don't
        // deadlock by going back to sleep.
        {
            std::unique_lock<std::mutex> lock(state->mtx);
            state->cv.wait(lock, [&state, &distributor]() {
                return state->ready ||
                       distributor->shutdown_.load(std::memory_order_seq_cst);
            });
        }

        // Check for shutdown
        if (distributor->shutdown_.load(std::memory_order_seq_cst)) {
            return;
        }

        // Process assigned tracks
        for (uint32_t i = 0; i < state->track_count; ++i) {
            TrackProcessor* tp = state->tracks[i];
            if (tp) {
                tp->process(state->frames, state->sample_pos, nullptr, nullptr);
            }
        }

        // Mark done and signal main thread
        {
            std::lock_guard<std::mutex> lock(state->mtx);
            state->done = true;
            state->ready = false;
            state->track_count = 0;
        }
        state->cv.notify_one();
    }
}

} // namespace aria
