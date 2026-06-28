#ifndef ARIA_BUFFER_POOL_H
#define ARIA_BUFFER_POOL_H

#include "audio_buffer.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace aria {

/// Lock-free pool of pre-allocated AudioBuffer objects.
///
/// Thread-safe for SPSC (single-producer, single-consumer) scenarios:
///   - Audio thread: calls acquire() to get a buffer
///   - Control thread: calls release() to return a buffer
///
/// Uses a lock-free ring buffer of buffer indices with atomic head/tail
/// cursors. All buffer memory is allocated at init() time — the audio
/// processing path performs zero dynamic allocations.
///
/// Default pool size: 1024 buffers. Must be a power of two.
class LockFreeBufferPool {
public:
    LockFreeBufferPool() = default;
    ~LockFreeBufferPool();

    LockFreeBufferPool(const LockFreeBufferPool&) = delete;
    LockFreeBufferPool& operator=(const LockFreeBufferPool&) = delete;

    LockFreeBufferPool(LockFreeBufferPool&& other) noexcept;
    LockFreeBufferPool& operator=(LockFreeBufferPool&& other) noexcept;

    /// Pre-allocate the pool.
    /// @param max_buffer_frames  Maximum frames per buffer (capacity).
    /// @param max_channels       Maximum channels per buffer.
    /// @param pool_size          Number of buffers in pool (default 1024).
    /// @return true on success, false if already initialized or allocation fails.
    bool init(uint32_t max_buffer_frames, uint32_t max_channels,
              uint32_t pool_size = 1024);

    /// Acquire a buffer from the pool.
    /// @return Pointer to an AudioBuffer, or nullptr if pool is empty.
    AudioBuffer* acquire();

    /// Return a buffer to the pool.
    /// @param buf  Pointer previously returned by acquire(). Must not be null.
    void release(AudioBuffer* buf);

    /// Number of available (free) buffers.
    uint32_t available() const;

    /// Total pool capacity.
    uint32_t capacity() const { return capacity_; }

    /// Whether the pool has been initialized.
    bool initialized() const { return initialized_; }

private:
    // Pre-allocated storage — one contiguous block per buffer.
    // Each buffer owns its channel data arrays.
    AudioBuffer* buffers_ = nullptr;

    // Ring buffer of free buffer indices (free_list_[i % capacity_]).
    uint32_t* free_list_ = nullptr;

    // Aligned to prevent false sharing between producer and consumer threads.
    // C4324: structure padded due to alignment specifier — expected on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
    alignas(64) std::atomic<uint32_t> head_{0};
    alignas(64) std::atomic<uint32_t> tail_{0};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    uint32_t capacity_ = 0;
    uint32_t max_frames_ = 0;
    uint32_t max_chans_ = 0;
    bool initialized_ = false;
};

} // namespace aria

#endif // ARIA_BUFFER_POOL_H
