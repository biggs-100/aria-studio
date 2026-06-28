#ifndef ARIA_COMMAND_QUEUE_H
#define ARIA_COMMAND_QUEUE_H

#include <functional>
#include <queue>
#include <mutex>

namespace aria {

/// Command queue — thread-safe dispatch for mutation operations.
class CommandQueue {
public:
    CommandQueue() = default;
    ~CommandQueue() = default;

    using Command = std::function<void()>;

    /// Dispatch a command (thread-safe).
    void dispatch(Command cmd);

    /// Process all pending commands.
    void process_pending();

    /// Number of pending commands.
    size_t pending_count() const;

private:
    std::queue<Command> queue_;
    mutable std::mutex mutex_;
};

} // namespace aria

#endif // ARIA_COMMAND_QUEUE_H
