#ifndef ARIA_EVENT_BUS_H
#define ARIA_EVENT_BUS_H

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace aria {

/// Event ID type.
using EventType = uint32_t;

/// Base event structure.
struct Event {
    EventType type;
    uint64_t timestamp;
};

/// Typed event bus — publish/subscribe inter-module communication.
class EventBus {
public:
    EventBus();
    ~EventBus();

    void init();

    /// Subscribe to an event type.
    template<typename T>
    uint64_t subscribe(std::function<void(const T&)> handler);

    /// Publish an event (immediate dispatch to sync subscribers).
    template<typename T>
    void publish(const T& event);

    /// Queue an event for later dispatch.
    template<typename T>
    void queue(const T& event);

    /// Dispatch all queued events.
    void dispatch_pending();

    /// Unsubscribe by subscription ID.
    void unsubscribe(uint64_t subscription_id);

private:
    struct Subscription {
        uint64_t id;
        EventType type;
        std::function<void(const Event&)> handler;
    };

    std::vector<Subscription> subscriptions_;
    std::vector<std::unique_ptr<Event>> pending_queue_;
    uint64_t next_id_ = 1;
};

} // namespace aria

#endif // ARIA_EVENT_BUS_H
