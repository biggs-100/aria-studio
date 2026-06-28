#ifndef ARIA_CONTROL_MESSAGE_QUEUE_H
#define ARIA_CONTROL_MESSAGE_QUEUE_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>

namespace aria {

/// Maximum number of pending control messages.
static constexpr uint32_t kMaxControlMessages = 256;

/// Lock-free single-producer single-consumer control message queue.
///
/// Used to pass commands from the control thread (UI) to the audio thread.
/// The audio thread reads and processes messages in its process() callback.
///
/// Messages are simple function objects (std::function<void()>) that are
/// executed by the audio thread. This keeps the interface generic for all
/// kinds of control messages (parameter changes, plugin inserts, etc.).
///
/// Thread safety:
///   - push() is called from the control thread (SPSC producer).
///   - pop_all() / process_all() is called from the audio thread (SPSC consumer).
///   - No locks — uses a simple ring buffer with atomic head/tail.
class ControlMessageQueue {
public:
    ControlMessageQueue() = default;
    ~ControlMessageQueue() = default;

    ControlMessageQueue(const ControlMessageQueue&) = delete;
    ControlMessageQueue& operator=(const ControlMessageQueue&) = delete;

    using Message = std::function<void()>;

    /// Push a message onto the queue (control thread).
    /// @param msg  The message to execute on the audio thread.
    /// @return true if the message was queued, false if the queue is full.
    bool push(Message msg) {
        uint32_t current_tail = tail_.load(std::memory_order_relaxed);
        uint32_t next_tail = (current_tail + 1) % kMaxControlMessages;

        // Check if queue is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // queue full
        }

        messages_[current_tail] = std::move(msg);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /// Pop the next message from the queue (audio thread).
    /// @param msg  Output parameter for the message.
    /// @return true if a message was popped, false if queue is empty.
    bool pop(Message& msg) {
        uint32_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;  // queue empty
        }

        msg = std::move(messages_[current_head]);
        head_.store((current_head + 1) % kMaxControlMessages,
                     std::memory_order_release);
        return true;
    }

    /// Process all pending messages in order.
    /// Called from the audio thread.
    void process_all() {
        Message msg;
        while (pop(msg)) {
            if (msg) {
                msg();
            }
        }
    }

    /// Number of pending messages.
    uint32_t size() const {
        uint32_t h = head_.load(std::memory_order_acquire);
        uint32_t t = tail_.load(std::memory_order_acquire);

        if (t >= h) {
            return t - h;
        }
        return kMaxControlMessages - h + t;
    }

    /// Whether the queue is empty.
    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    /// Clear all pending messages.
    void clear() {
        while (!empty()) {
            Message msg;
            pop(msg);
        }
    }

private:
    // Ring buffer of messages
    Message messages_[kMaxControlMessages];

    // Aligned atomics to prevent false sharing
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
    alignas(64) std::atomic<uint32_t> head_{0};  // read position (audio thread)
    alignas(64) std::atomic<uint32_t> tail_{0};  // write position (control thread)
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
};

} // namespace aria

#endif // ARIA_CONTROL_MESSAGE_QUEUE_H
