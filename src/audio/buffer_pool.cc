#include "buffer_pool.h"
#include "simd_ops.h"
#include <cstdlib>
#include <cstring>
#include <new>

namespace aria {

LockFreeBufferPool::~LockFreeBufferPool() {
    if (!buffers_) return;

    for (uint32_t i = 0; i < capacity_; ++i) {
        for (uint32_t c = 0; c < max_chans_; ++c) {
            delete[] buffers_[i].data[c];
        }
    }

    // `buffers_` and `free_list_` were allocated with new[]
    delete[] buffers_;
    delete[] free_list_;
    buffers_ = nullptr;
    free_list_ = nullptr;
    capacity_ = 0;
    initialized_ = false;
}

LockFreeBufferPool::LockFreeBufferPool(LockFreeBufferPool&& other) noexcept
    : buffers_(other.buffers_)
    , free_list_(other.free_list_)
    , head_(other.head_.load())
    , tail_(other.tail_.load())
    , capacity_(other.capacity_)
    , max_frames_(other.max_frames_)
    , max_chans_(other.max_chans_)
    , initialized_(other.initialized_)
{
    other.buffers_ = nullptr;
    other.free_list_ = nullptr;
    other.capacity_ = 0;
    other.initialized_ = false;
}

LockFreeBufferPool& LockFreeBufferPool::operator=(LockFreeBufferPool&& other) noexcept {
    if (this != &other) {
        // Destroy current state
        this->~LockFreeBufferPool();

        // Move
        buffers_ = other.buffers_;
        free_list_ = other.free_list_;
        head_.store(other.head_.load());
        tail_.store(other.tail_.load());
        capacity_ = other.capacity_;
        max_frames_ = other.max_frames_;
        max_chans_ = other.max_chans_;
        initialized_ = other.initialized_;

        other.buffers_ = nullptr;
        other.free_list_ = nullptr;
        other.capacity_ = 0;
        other.initialized_ = false;
    }
    return *this;
}

bool LockFreeBufferPool::init(uint32_t max_buffer_frames, uint32_t max_channels,
                               uint32_t pool_size) {
    if (initialized_) return false;
    if (pool_size == 0) return false;
    if (max_buffer_frames == 0 || max_channels == 0) return false;
    if (max_channels > kMaxChannels) return false;

    // Round pool_size up to power of 2 for efficient modulo
    uint32_t size = 1;
    while (size < pool_size) size <<= 1;

    max_frames_ = max_buffer_frames;
    max_chans_ = max_channels;
    capacity_ = size;

    // Allocate buffer array
    buffers_ = new AudioBuffer[capacity_]();

    // Allocate channel data for each buffer
    for (uint32_t i = 0; i < capacity_; ++i) {
        auto& buf = buffers_[i];
        buf.frames = 0;
        buf.channels = max_channels;
        buf.capacity = max_buffer_frames;
        for (uint32_t c = 0; c < max_channels; ++c) {
            buf.data[c] = new float[max_buffer_frames]();
        }
    }

    // Allocate free-list ring buffer
    free_list_ = new uint32_t[capacity_];

    // Initialize free list with all buffer indices (FIFO order)
    for (uint32_t i = 0; i < capacity_; ++i) {
        free_list_[i] = i;
    }

    // head == tail + 1 (mod capacity) means all buffers are free
    // We set tail to capacity_ - 1 so that tail - head = capacity_ - 1
    // which means capacity_ slots are available.
    // Actually with the ring: available = tail - head (mod overflow).
    // We want all buffers available: tail - head >= capacity_
    // So we start with head = 0, tail = UINT32_MAX (which wraps to 0 after
    // adding capacity_).
    // Simpler: head at 0, tail at capacity_ - 1.
    head_.store(0, std::memory_order_release);
    tail_.store(capacity_ - 1, std::memory_order_release);

    initialized_ = true;
    return true;
}

AudioBuffer* LockFreeBufferPool::acquire() {
    if (!initialized_) return nullptr;

    uint32_t h;
    uint32_t t;

    for (;;) {
        h = head_.load(std::memory_order_relaxed);
        t = tail_.load(std::memory_order_acquire);

        // Ring buffer: if head > tail (considering wrapping), the pool is empty.
        // With monotonically increasing counters that can overflow:
        // Empty when head - 1 == tail (no available slots)
        // Actually: available = (tail - head + 1) (mod overflow arithmetic).
        // When tail - head == UINT32_MAX (i.e., head == tail + 1), the pool is empty.
        // We initialized with tail = capacity_-1, head = 0.
        // After N acquires: head = N, tail = capacity_-1.
        // After N releases: tail = capacity_-1 + N.
        // Pool is empty when head > tail.

        // Since counters wrap: empty when (int32_t)(tail - head) < 0
        // i.e., when the signed difference is negative.
        int32_t available = static_cast<int32_t>(t) - static_cast<int32_t>(h);
        if (available < 0) {
            return nullptr;  // pool empty
        }

        if (head_.compare_exchange_weak(h, h + 1,
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
            break;
        }
    }

    // h is the index BEFORE the CAS succeeded — use it modulo capacity_
    uint32_t idx = free_list_[h % capacity_];
    AudioBuffer* buf = &buffers_[idx];

    // Reset buffer state
    buf->frames = 0;
    buf->channels = max_chans_;
    buf->capacity = max_frames_;

    return buf;
}

void LockFreeBufferPool::release(AudioBuffer* buf) {
    if (!initialized_ || !buf) return;

    uint32_t idx = static_cast<uint32_t>(buf - buffers_);
    if (idx >= capacity_) return;  // not one of ours

    uint32_t t;
    for (;;) {
        t = tail_.load(std::memory_order_relaxed);

        // Check if pool is full: (t + 1) - head > capacity_
        // This means release would overflow — spin (shouldn't happen
        // with properly sized pool)
        uint32_t h = head_.load(std::memory_order_acquire);
        uint32_t next_t = t + 1;
        uint32_t used = next_t - h;  // monotonically increasing
        if (used > capacity_) {
            // Pool is completely full — spin (very rare)
            continue;
        }

        if (tail_.compare_exchange_weak(t, next_t,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
            break;
        }
    }

    // Write the buffer index into the claimed slot
    uint32_t slot = (t + 1) % capacity_;
    free_list_[slot] = idx;
}

uint32_t LockFreeBufferPool::available() const {
    uint32_t h = head_.load(std::memory_order_relaxed);
    uint32_t t = tail_.load(std::memory_order_acquire);
    // When tail - head >= capacity, at most capacity_ are available
    if (t >= h) {
        uint32_t diff = t - h + 1;
        return (diff > capacity_) ? capacity_ : diff;
    }
    return 0;
}

} // namespace aria
