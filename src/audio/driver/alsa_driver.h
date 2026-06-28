#ifndef ARIA_ALSA_DRIVER_H
#define ARIA_ALSA_DRIVER_H

#include "audio_driver.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace aria {

/// ALSA audio driver for Linux.
///
/// Uses the ALSA PCM API for device enumeration and audio I/O.
/// Supports mmap-based and read/write access modes.
///
/// Platform: Linux only (compiled under __linux__).
class AlsaDriver final : public AudioDriver {
public:
    AlsaDriver();
    ~AlsaDriver() override;

    AlsaDriver(const AlsaDriver&) = delete;
    AlsaDriver& operator=(const AlsaDriver&) = delete;

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

    /// Set whether to attempt mmap access (default: true).
    void set_use_mmap(bool use_mmap) { use_mmap_ = use_mmap; }

private:
    // ─── Internal ──────────────────────────────────────────────
    void audio_thread_func();

    // ─── Opaque ALSA handles (stored as void* to avoid
    //     including ALSA headers in this header) ────────────────
    void* pcm_handle_ = nullptr;   // snd_pcm_t*

    std::string device_name_;
    uint32_t    input_channels_  = 0;
    uint32_t    output_channels_ = 0;

    std::atomic<uint32_t> sample_rate_{0};
    std::atomic<uint32_t> buffer_size_{0};
    std::atomic<bool>     open_{false};
    std::atomic<bool>     running_{false};
    std::atomic<bool>     use_mmap_{true};

    // Audio thread
    std::unique_ptr<std::thread> audio_thread_;
    std::mutex                   thread_mutex_;

    // Scratch buffers
    float** input_scratch_  = nullptr;
    float** output_scratch_ = nullptr;
};

} // namespace aria

#endif // ARIA_ALSA_DRIVER_H
