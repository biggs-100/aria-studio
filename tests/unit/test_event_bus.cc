#include <catch2/catch_test_macros.hpp>
#include "kernel/event_bus.h"

using namespace aria;

struct TestEvent : public Event {
    int value;
};

TEST_CASE("Event bus basic operations", "[kernel][event]") {
    EventBus bus;
    bus.init();

    SECTION("Publish and receive event") {
        int received = 0;
        bus.subscribe<TestEvent>([&](const TestEvent& e) {
            received = e.value;
        });

        TestEvent event{1000, 0, 42};
        bus.publish(event);

        REQUIRE(received == 42);
    }
}
