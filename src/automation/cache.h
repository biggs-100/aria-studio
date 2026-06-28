#ifndef ARIA_AUTOMATION_CACHE_H
#define ARIA_AUTOMATION_CACHE_H

#include "automation_types.h"

#include <atomic>
#include <unordered_map>

namespace aria::automation {

/// Lock-free, double-buffered parameter cache.
///
/// Provides sample-accurate parameter reads from the audio thread
/// without mutex contention. The control thread writes to the write
/// buffer; the audio thread reads from the read buffer. Calling
/// `swap_buffers()` atomically exchanges the two, giving the audio
/// thread a consistent snapshot for the duration of one callback.
class ParameterCache {
public:
    ParameterCache() = default;

    // ─── Write API (control thread) ───────────────────────────

    /// Update a parameter's value in the write buffer.
    void update_value(ParameterID id, double value);

    /// Update a parameter's range in the write buffer.
    void update_range(ParameterID id, double min, double max);

    /// Atomically swap write and read buffers.
    /// After this call, the read buffer contains a consistent snapshot
    /// of all values written since the last swap.
    void swap_buffers();

    // ─── Read API (audio thread) ─────────────────────────────

    /// Read a parameter's value from the read buffer.
    /// Returns 0.0 if the parameter is not present.
    double read_value(ParameterID id) const;

    // ─── Utilities ────────────────────────────────────────────

    /// Clear both buffers.
    void clear();

private:
    struct Entry {
        std::atomic<double> value{0.0};
        double min = 0.0;
        double max = 1.0;
    };

    using Buffer = std::unordered_map<ParameterID, Entry>;

    Buffer write_buffer_;
    Buffer read_buffer_;
    std::atomic<size_t> pending_swaps_{0};
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_CACHE_H
