#include "command_queue.h"

namespace aria {

void CommandQueue::dispatch(Command cmd) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(cmd));
}

void CommandQueue::process_pending() {
    while (true) {
        Command cmd;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) break;
            cmd = std::move(queue_.front());
            queue_.pop();
        }
        if (cmd) cmd();
    }
}

size_t CommandQueue::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

} // namespace aria
