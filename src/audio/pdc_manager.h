#ifndef ARIA_PDC_MANAGER_H
#define ARIA_PDC_MANAGER_H

#include "audio_buffer.h"
#include "audio_node.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace aria {

/// Plugin Delay Compensation (PDC) manager.
///
/// Compensates for plugin latency across parallel signal chains so that
/// all chains remain sample-aligned at the mix bus.
///
/// Each AudioNode can report its latency via `latency()`. The PDC manager
/// finds the maximum latency across all chains and delays shorter chains
/// by `max_delay - chain_delay` samples using delay buffers (circular).
///
/// Thread safety:
///   - recalculate() is called from the control thread when the graph changes.
///   - apply_compensation() is called from the audio thread (real-time safe).
///   - No allocations in the audio path after initialization.
class PDCManager {
public:
    PDCManager() = default;
    ~PDCManager() = default;

    PDCManager(const PDCManager&) = delete;
    PDCManager& operator=(const PDCManager&) = delete;

    PDCManager(PDCManager&& other) noexcept;
    PDCManager& operator=(PDCManager&& other) noexcept;

    /// Recalculate chain latencies and compensation amounts.
    /// Called when the plugin chain changes.
    /// @param chain  The current plugin chain (vector of AudioNode*).
    void recalculate(const std::vector<AudioNode*>& chain);

    /// Get the total delay of a chain in samples.
    /// Sums the latency of each node in the chain.
    /// @param chain  Vector of AudioNode pointers.
    /// @return Total latency in samples.
    uint32_t chain_delay(const std::vector<AudioNode*>& chain) const;

    /// Maximum delay across all chains.
    uint32_t max_delay() const {
        return max_delay_.load(std::memory_order_relaxed);
    }

    /// Apply delay compensation to a buffer.
    /// Delays the buffer by `delay_samples` samples (circular delay).
    /// If delay_samples is 0, this is a no-op.
    /// @param buffer         Buffer to delay (in-place).
    /// @param delay_samples  Number of samples of delay to apply.
    /// @param chain_id       Unique ID for this chain's delay buffer.
    void apply_compensation(AudioBuffer& buffer, uint32_t delay_samples,
                            uint64_t chain_id);

    /// Reset all delay buffers.
    void reset();

    /// Whether compensation is active (max_delay > 0).
    bool is_active() const {
        return max_delay_.load(std::memory_order_relaxed) > 0;
    }

private:
    /// Internal circular delay buffer per chain.
    struct DelayBuffer {
        std::vector<float> data;
        uint32_t write_pos = 0;
        uint32_t size = 0;
        uint32_t channels = 1;

        /// Read from delay line, filling @p output.
        void read(float* output, uint32_t frames);

        /// Write into delay line from @p input.
        void write(const float* input, uint32_t frames);

        /// Initialize with given size and channels.
        void init(uint32_t capacity, uint32_t ch);

        /// Clear all data.
        void clear();
    };

    // Per-chain delay buffers, keyed by chain_id.
    std::unordered_map<uint64_t, DelayBuffer> delay_buffers_;

    /// Maximum delay across all chains (atomic for audio thread access).
    std::atomic<uint32_t> max_delay_{0};

    /// Capacity of delay buffers — grown as needed.
    uint32_t buffer_capacity_ = 0;
};

} // namespace aria

#endif // ARIA_PDC_MANAGER_H
