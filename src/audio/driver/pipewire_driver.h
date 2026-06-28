#ifndef ARIA_PIPEWIRE_DRIVER_H
#define ARIA_PIPEWIRE_DRIVER_H

#include "audio_driver.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aria {

/// PipeWire audio driver for Linux.
///
/// Uses the PipeWire main-loop and spa_node APIs for audio I/O.
/// Provides integration with the modern Linux audio stack.
///
/// Platform: Linux only (compiled under __linux__).
class PipeWireDriver final : public AudioDriver {
public:
    PipeWireDriver();
    ~PipeWireDriver() override;

    PipeWireDriver(const PipeWireDriver&) = delete;
    PipeWireDriver& operator=(const PipeWireDriver&) = delete;

    // ─── AudioDriver interface ─────────────────────────────────

    std::vector<DeviceInfo> enumerate_devices() override;
    bool open(const DeviceInfo& dev, uint32_t sample_rate,
              uint32_t buffer_size) override;
    bool start() override;
    void stop() override;
    void close() override;

    uint32_t current_sample_rate() const override;
    uint32_t current_buffer_size() const override;
    bool is_open() const override;
    bool is_running() const override;

private:
    // ─── Internal helpers ──────────────────────────────────────
    void audio_thread_func();

    // ─── Opaque PipeWire handles ──────────────────────────────
    // Stored as void* to avoid requiring PipeWire headers here.
    void* pw_main_loop_  = nullptr;  // pw_main_loop*
    void* pw_context_    = nullptr;  // pw_context*
    void* pw_core_       = nullptr;  // pw_core*
    void* pw_stream_     = nullptr;  // pw_stream*

    std::string device_name_;
    uint32_t    input_channels_  = 0;
    uint32_t    output_channels_ = 0;

    std::atomic<uint32_t> sample_rate_{0};
    std::atomic<uint32_t> buffer_size_{0};
    std::atomic<bool>     open_{false};
    std::atomic<bool>     running_{false};

    // Audio thread
    std::unique_ptr<std::thread> audio_thread_;
    std::mutex                   thread_mutex_;

    // Scratch buffers
    float** output_scratch_ = nullptr;
    float** input_scratch_  = nullptr;

    // Stored device list (for enumeration cache)
    std::vector<DeviceInfo> cached_devices_;
};

} // namespace aria

#endif // ARIA_PIPEWIRE_DRIVER_H
