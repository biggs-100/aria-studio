#include "event_bus.h"

namespace aria {

EventBus::EventBus() = default;
EventBus::~EventBus() = default;
void EventBus::init() {}

void EventBus::dispatch_pending() {
    auto pending = std::move(pending_queue_);
    pending_queue_.clear();
    for (auto& event : pending) {
        for (auto& sub : subscriptions_) {
            if (sub.type == event->type) {
                sub.handler(*event);
            }
        }
    }
}

void EventBus::unsubscribe(uint64_t subscription_id) {
    auto it = std::remove_if(subscriptions_.begin(), subscriptions_.end(),
        [subscription_id](const Subscription& s) { return s.id == subscription_id; });
    subscriptions_.erase(it, subscriptions_.end());
}

} // namespace aria
