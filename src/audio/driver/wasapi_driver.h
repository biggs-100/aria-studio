#ifndef ARIA_WASAPI_DRIVER_H
#define ARIA_WASAPI_DRIVER_H

#include "audio_driver.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace aria {

/// WASAPI audio driver for Windows.
///
/// Supports both exclusive (low-latency) and shared (compatibility) modes.
/// Uses event-driven callbacks via IAudioClient::Initialize with
/// AUDCLNT_STREAMFLAGS_EVENTCALLBACK.
///
/// Platform: Windows only (compiled under _WIN32).
class WasapiDriver final : public AudioDriver {
public:
    WasapiDriver();
    ~WasapiDriver() override;

    WasapiDriver(const WasapiDriver&) = delete;
    WasapiDriver& operator=(const WasapiDriver&) = delete;

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

    /// Set whether to use exclusive mode (default: true).
    /// Must be called before open().
    void set_exclusive_mode(bool exclusive) { exclusive_ = exclusive; }

    /// Whether exclusive mode is enabled.
    bool exclusive_mode() const { return exclusive_; }

private:
    // ─── Internal: Audio thread processing ─────────────────────
    void audio_thread_func();

    // ─── COM helpers ───────────────────────────────────────────
    bool initialize_com();
    void uninitialize_com();

    // ─── Device state ──────────────────────────────────────────
    std::string device_id_;
    uint32_t    input_channels_  = 0;
    uint32_t    output_channels_ = 0;

    std::atomic<uint32_t> sample_rate_{0};
    std::atomic<uint32_t> buffer_size_{0};
    std::atomic<bool>     open_{false};
    std::atomic<bool>     running_{false};
    std::atomic<bool>     exclusive_{true};

    // ─── Audio thread ──────────────────────────────────────────
    std::unique_ptr<std::thread> audio_thread_;
    std::mutex                   thread_mutex_;

    // ─── COM pointers (opaque handles, stored as void* to avoid
    //     requiring Windows.h in the header) ────────────────────
    void* mm_device_enumerator_ = nullptr;  // IMMDeviceEnumerator*
    void* mm_device_            = nullptr;  // IMMDevice*
    void* audio_client_         = nullptr;  // IAudioClient*
    void* render_client_        = nullptr;  // IAudioRenderClient*
    void* capture_client_       = nullptr;  // IAudioCaptureClient*

    // Event handle for callback synchronization.
    void* event_handle_         = nullptr;  // HANDLE

    // Scratch buffers for format conversion.
    float** input_scratch_  = nullptr;
    float** output_scratch_ = nullptr;
};

} // namespace aria

#endif // ARIA_WASAPI_DRIVER_H
