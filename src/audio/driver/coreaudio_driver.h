#ifndef ARIA_COREAUDIO_DRIVER_H
#define ARIA_COREAUDIO_DRIVER_H

#include "audio_driver.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aria {

/// CoreAudio audio driver for macOS.
///
/// Uses the AudioUnit (RemoteIO) API for low-latency audio I/O.
/// Device enumeration uses AudioObjectGetPropertyData.
///
/// Platform: macOS only (compiled under __APPLE__).
///
/// Note: This header is C++ compatible. The .mm implementation file
/// mixes Objective-C++ for AudioUnit API access.
class CoreAudioDriver final : public AudioDriver {
public:
    CoreAudioDriver();
    ~CoreAudioDriver() override;

    CoreAudioDriver(const CoreAudioDriver&) = delete;
    CoreAudioDriver& operator=(const CoreAudioDriver&) = delete;

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
    // ─── Internal state (opaque AudioUnit handle, C-compatible) ─
    // The actual AudioUnit is a typedef for OpaqueAudioUnit*.
    // We store it as void* to avoid requiring AudioUnit.h in C++ headers.
    void* audio_unit_ = nullptr;

    std::string device_uid_;
    uint32_t    input_channels_  = 0;
    uint32_t    output_channels_ = 0;

    std::atomic<uint32_t> sample_rate_{0};
    std::atomic<uint32_t> buffer_size_{0};
    std::atomic<bool>     open_{false};
    std::atomic<bool>     running_{false};

    // Scratch buffers for format conversion.
    float** input_scratch_  = nullptr;
    float** output_scratch_ = nullptr;

    // ─── Render callback (C-compatible, registered with AudioUnit) ─
    // The .mm file implements this and bridges to the C++ instance.
    friend struct CoreAudioRenderBridge;
};

} // namespace aria

#endif // ARIA_COREAUDIO_DRIVER_H
