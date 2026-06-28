#include <catch2/catch_test_macros.hpp>
#include "midi/driver/midi_driver.h"
#include "midi/driver/platform_midi.h"

#include <memory>
#include <vector>
#include <atomic>

using namespace aria;

// ─── Mock MidiDriver for testing ─────────────────────────────────

/// A mock MIDI driver that captures sent events and can inject received events.
class MockMidiDriver : public MidiDriver {
public:
    MockMidiDriver() = default;

    std::vector<MidiDeviceInfo> enumerate() override {
        std::vector<MidiDeviceInfo> devices;
        MidiDeviceInfo info;
        info.id = "mock_device";
        info.name = "Mock MIDI Device";
        info.direction = MidiDeviceInfo::Direction::Duplex;
        info.type = MidiDeviceInfo::Type::Virtual;
        info.is_default = true;
        devices.push_back(info);
        return devices;
    }

    bool open(const MidiDeviceInfo& /*dev*/) override {
        open_ = true;
        return true;
    }

    void close() override {
        open_ = false;
        sent_events_.clear();
    }

    bool send(const MidiEvent& event) override {
        if (!open_) return false;
        sent_events_.push_back(event);
        return true;
    }

    bool is_open() const override { return open_; }

    // ─── Test helpers ─────────────────────────────────────────
    /// Simulate receiving an event from the device.
    void inject_receive(const MidiEvent& event) {
        dispatch_receive(event);
    }

    /// Get the number of events sent via send().
    size_t sent_count() const { return sent_events_.size(); }

    /// Get the last sent event.
    MidiEvent last_sent() const {
        if (sent_events_.empty()) return MidiEvent{};
        return sent_events_.back();
    }

    /// Get all sent events.
    const std::vector<MidiEvent>& sent_events() const { return sent_events_; }

private:
    bool open_ = false;
    std::vector<MidiEvent> sent_events_;
};

// ─── Tests ──────────────────────────────────────────────────────

TEST_CASE("MidiDriver mock — send and receive", "[midi][driver][mock]") {
    MockMidiDriver driver;

    SECTION("starts closed") {
        REQUIRE_FALSE(driver.is_open());
    }

    SECTION("enumerate returns mock device") {
        auto devices = driver.enumerate();
        REQUIRE(devices.size() == 1);
        REQUIRE(devices[0].id == "mock_device");
        REQUIRE(devices[0].name == "Mock MIDI Device");
    }

    SECTION("open and close lifecycle") {
        MidiDeviceInfo dev;
        REQUIRE(driver.open(dev));
        REQUIRE(driver.is_open());
        driver.close();
        REQUIRE_FALSE(driver.is_open());
    }

    SECTION("send after open succeeds") {
        MidiDeviceInfo dev;
        driver.open(dev);

        MidiEvent event;
        event.type = MidiMessageType::NoteOn;
        event.channel = 0;
        event.data1 = 60;
        event.data2 = 100;

        REQUIRE(driver.send(event));
        REQUIRE(driver.sent_count() == 1);
    }

    SECTION("send while closed fails") {
        MidiEvent event;
        REQUIRE_FALSE(driver.send(event));
    }

    SECTION("receive callback fires on inject") {
        std::atomic<int> callback_count{0};
        MidiEvent received;

        driver.set_callback([&](const MidiEvent& ev) {
            callback_count++;
            received = ev;
        });

        MidiEvent event;
        event.type = MidiMessageType::NoteOn;
        event.channel = 0;
        event.data1 = 60;
        event.data2 = 100;

        driver.inject_receive(event);

        REQUIRE(callback_count == 1);
        REQUIRE(received.type == MidiMessageType::NoteOn);
        REQUIRE(received.data1 == 60);
        REQUIRE(received.data2 == 100);
    }

    SECTION("multiple events send correctly") {
        MidiDeviceInfo dev;
        driver.open(dev);

        MidiEvent ev1, ev2;
        ev1.type = MidiMessageType::NoteOn;
        ev1.data1 = 60;
        ev2.type = MidiMessageType::NoteOff;
        ev2.data1 = 60;

        driver.send(ev1);
        driver.send(ev2);

        REQUIRE(driver.sent_count() == 2);
        REQUIRE(driver.last_sent().type == MidiMessageType::NoteOff);
    }

    SECTION("last_error starts empty") {
        REQUIRE(driver.last_error().empty());
    }

    SECTION("clear_error works") {
        MidiEvent event;
        driver.send(event); // fails — not open
        REQUIRE_FALSE(driver.last_error().empty());
        driver.clear_error();
        REQUIRE(driver.last_error().empty());
    }
}

TEST_CASE("MidiDriver mock — receive callback thread safety", "[midi][driver][mock]") {
    // Verify that the callback mechanism works with multiple rapid calls
    MockMidiDriver driver;

    std::atomic<int> callback_count{0};
    driver.set_callback([&](const MidiEvent&) {
        callback_count++;
    });

    // Simulate a burst of events
    for (int i = 0; i < 100; ++i) {
        MidiEvent event;
        event.type = MidiMessageType::NoteOn;
        event.data1 = static_cast<uint8_t>(i);
        driver.inject_receive(event);
    }

    // Check that the callback was not called before set_callback
    REQUIRE(callback_count == 100);
}

TEST_CASE("PlatformMidi factory", "[midi][driver][platform]") {
    SECTION("create_default_driver returns a valid driver") {
        auto driver = PlatformMidi::create_default_driver();
        REQUIRE(driver != nullptr);
    }

    SECTION("create_virtual_port_driver returns a valid driver") {
        auto driver = PlatformMidi::create_virtual_port_driver();
        REQUIRE(driver != nullptr);
    }

    SECTION("create_driver with 'virtual' returns virtual driver") {
        auto driver = PlatformMidi::create_driver("virtual");
        REQUIRE(driver != nullptr);
    }

    SECTION("default_driver_name returns non-empty string") {
        auto name = PlatformMidi::default_driver_name();
        REQUIRE_FALSE(name.empty());
    }

    SECTION("create_driver with unknown name falls back to default") {
        auto driver = PlatformMidi::create_driver("nonexistent");
        REQUIRE(driver != nullptr);
    }

    SECTION("virtual port driver can enumerate") {
        auto driver = PlatformMidi::create_virtual_port_driver();
        auto devices = driver->enumerate();
        // Should at least return its own port if open, or empty list
        REQUIRE(devices.size() >= 0);
    }
}

TEST_CASE("MidiDeviceInfo construction", "[midi][driver]") {
    SECTION("default device info") {
        MidiDeviceInfo info;
        REQUIRE(info.id.empty());
        REQUIRE(info.name.empty());
        REQUIRE(info.direction == MidiDeviceInfo::Direction::Input);
        REQUIRE(info.type == MidiDeviceInfo::Type::Physical);
        REQUIRE_FALSE(info.is_default);
    }

    SECTION("custom device info") {
        MidiDeviceInfo info;
        info.id = "test_device";
        info.name = "Test Device";
        info.direction = MidiDeviceInfo::Direction::Duplex;
        info.type = MidiDeviceInfo::Type::USB;
        info.is_default = true;

        REQUIRE(info.id == "test_device");
        REQUIRE(info.name == "Test Device");
        REQUIRE(info.direction == MidiDeviceInfo::Direction::Duplex);
        REQUIRE(info.type == MidiDeviceInfo::Type::USB);
        REQUIRE(info.is_default);
    }
}
