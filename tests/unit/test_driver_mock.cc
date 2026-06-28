#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "audio/driver/audio_driver.h"

#include <algorithm>
#include <atomic>
#include <vector>

using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Mock driver for verifying the AudioDriver interface contract.
// ═══════════════════════════════════════════════════════════════

class MockDriver final : public AudioDriver {
public:
    MockDriver() = default;

    // ─── AudioDriver interface ─────────────────────────────────

    std::vector<DeviceInfo> enumerate_devices() override {
        enumerated_ = true;
        return devices_;
    }

    bool open(const DeviceInfo& dev, uint32_t sample_rate,
              uint32_t buffer_size) override
    {
        if (open_) return false;  // already open

        opened_device_ = dev;
        open_sample_rate_ = sample_rate;
        open_buffer_size_ = buffer_size;
        open_ = true;
        return true;
    }

    bool start() override {
        if (!open_ || running_) return false;
        running_ = true;
        return true;
    }

    void stop() override {
        running_ = false;
    }

    void close() override {
        if (!open_) return;
        stop();
        open_ = false;
        opened_device_ = DeviceInfo{};
    }

    uint32_t current_sample_rate() const override {
        return open_sample_rate_;
    }

    uint32_t current_buffer_size() const override {
        return open_buffer_size_;
    }

    bool is_open() const override { return open_; }
    bool is_running() const override { return running_; }

    // ─── Mock helpers ──────────────────────────────────────────

    void add_device(const DeviceInfo& dev) {
        devices_.push_back(dev);
    }

    bool was_enumerated() const { return enumerated_; }

    DeviceInfo last_opened_device() const { return opened_device_; }

    // Simulated process (test calls this instead of real audio thread).
    void simulate_process(uint32_t frames) {
        if (!callback_ || !running_) return;

        // Allocate mock buffers
        float* in_mem  = nullptr;
        float* out_mem = nullptr;
        float* in_ptr  = nullptr;
        float* out_ptr = nullptr;

        if (opened_device_.input_channels > 0) {
            in_mem  = new float[opened_device_.input_channels * frames]();
            in_ptr  = &in_mem[0];
        }
        if (opened_device_.output_channels > 0) {
            out_mem = new float[opened_device_.output_channels * frames]();
            out_ptr = &out_mem[0];
        }

        callback_(&in_ptr, &out_ptr, frames);

        delete[] in_mem;
        delete[] out_mem;
    }

private:
    std::vector<DeviceInfo> devices_;
    bool enumerated_ = false;
    bool open_ = false;
    bool running_ = false;
    uint32_t open_sample_rate_ = 0;
    uint32_t open_buffer_size_ = 0;
    DeviceInfo opened_device_;
};

// ═══════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("MockDriver — lifecycle", "[audio][driver][mock]") {
    MockDriver driver;

    SECTION("initial state") {
        REQUIRE_FALSE(driver.is_open());
        REQUIRE_FALSE(driver.is_running());
        REQUIRE_FALSE(driver.was_enumerated());
    }

    SECTION("enumerate returns configured devices") {
        AudioDriver::DeviceInfo dev;
        dev.id = "mock-1";
        dev.name = "Mock Device 1";
        dev.output_channels = 2;
        dev.sample_rates = {44100, 48000};
        dev.buffer_sizes = {64, 128, 256};
        driver.add_device(dev);

        auto devices = driver.enumerate_devices();
        REQUIRE(driver.was_enumerated());
        REQUIRE(devices.size() == 1);
        REQUIRE(devices[0].id == "mock-1");
        REQUIRE(devices[0].output_channels == 2);
    }

    SECTION("open and start") {
        AudioDriver::DeviceInfo dev;
        dev.id = "mock-1";
        dev.name = "Mock";
        dev.output_channels = 2;

        REQUIRE(driver.open(dev, 48000, 128));
        REQUIRE(driver.is_open());
        REQUIRE(driver.current_sample_rate() == 48000);
        REQUIRE(driver.current_buffer_size() == 128);

        REQUIRE(driver.start());
        REQUIRE(driver.is_running());

        driver.stop();
        REQUIRE_FALSE(driver.is_running());
        REQUIRE(driver.is_open());  // still open

        driver.close();
        REQUIRE_FALSE(driver.is_open());
    }

    SECTION("cannot open twice") {
        AudioDriver::DeviceInfo dev;
        dev.id = "mock";
        REQUIRE(driver.open(dev, 48000, 128));
        REQUIRE_FALSE(driver.open(dev, 48000, 128));  // second open fails
    }

    SECTION("cannot start when closed") {
        REQUIRE_FALSE(driver.start());
    }

    SECTION("cannot start twice") {
        AudioDriver::DeviceInfo dev;
        dev.id = "mock";
        driver.open(dev, 48000, 128);
        driver.start();
        REQUIRE_FALSE(driver.start());  // second start fails
    }

    SECTION("close also stops") {
        AudioDriver::DeviceInfo dev;
        dev.id = "mock";
        driver.open(dev, 48000, 128);
        driver.start();
        driver.close();
        REQUIRE_FALSE(driver.is_open());
        REQUIRE_FALSE(driver.is_running());
    }
}

TEST_CASE("MockDriver — process callback", "[audio][driver][mock]") {
    MockDriver driver;

    AudioDriver::DeviceInfo dev;
    dev.id = "mock";
    dev.output_channels = 2;
    dev.input_channels = 0;

    driver.open(dev, 48000, 128);
    driver.start();

    int callback_count = 0;
    driver.set_process_callback(
        [&](float** input, float** output, uint32_t frames) {
            callback_count++;
            REQUIRE(frames == 128);
            REQUIRE(input == nullptr);   // no input channels
            REQUIRE(output != nullptr);
            // Fill output with a known pattern
            for (uint32_t ch = 0; ch < 2; ++ch) {
                if (output[ch]) {
                    output[ch][0] = 1.0f;
                }
            }
        });

    driver.simulate_process(128);
    REQUIRE(callback_count == 1);

    driver.simulate_process(128);
    REQUIRE(callback_count == 2);
}

TEST_CASE("MockDriver — enumeration returns multiple devices", "[audio][driver][mock]") {
    MockDriver driver;

    AudioDriver::DeviceInfo dev1;
    dev1.id = "hw:0";
    dev1.name = "Device 0";
    dev1.output_channels = 2;
    dev1.input_channels = 2;
    dev1.is_default = true;

    AudioDriver::DeviceInfo dev2;
    dev2.id = "hw:1";
    dev2.name = "Device 1";
    dev2.output_channels = 8;
    dev2.input_channels = 0;

    driver.add_device(dev1);
    driver.add_device(dev2);

    auto devices = driver.enumerate_devices();
    REQUIRE(devices.size() == 2);
    REQUIRE(devices[0].is_default == true);
    REQUIRE(devices[0].input_channels == 2);
    REQUIRE(devices[1].output_channels == 8);
}

TEST_CASE("MockDriver — multiple open/close cycles", "[audio][driver][mock]") {
    MockDriver driver;

    AudioDriver::DeviceInfo dev;
    dev.id = "mock";

    for (int i = 0; i < 5; ++i) {
        REQUIRE(driver.open(dev, 48000, 128));
        REQUIRE(driver.start());
        driver.stop();
        driver.close();
        REQUIRE_FALSE(driver.is_open());
    }
}
