#ifndef ARIA_AUDIO_DRIVER_H
#define ARIA_AUDIO_DRIVER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aria {

/// Abstract base class for all platform audio drivers.
///
/// Each driver implementation (ASIO, WASAPI, CoreAudio, ALSA, PipeWire)
/// subclasses this interface and provides device enumeration, stream
/// lifecycle management, and a real-time-safe process callback path.
class AudioDriver {
public:
    /// Information about a single audio device.
    struct DeviceInfo {
        std::string id;                      ///< Persistent unique device ID.
        std::string name;                    ///< Human-readable device name.
        uint32_t    input_channels  = 0;     ///< Number of input channels.
        uint32_t    output_channels = 0;     ///< Number of output channels.
        std::vector<uint32_t> sample_rates;  ///< Supported sample rates (Hz).
        std::vector<uint32_t> buffer_sizes;  ///< Supported buffer sizes (frames).
        bool        is_default = false;      ///< Whether this is the system default.
    };

    virtual ~AudioDriver() = default;

    AudioDriver(const AudioDriver&) = delete;
    AudioDriver& operator=(const AudioDriver&) = delete;

    // ─── Device enumeration ────────────────────────────────────
    /// Enumerate all available audio devices.
    virtual std::vector<DeviceInfo> enumerate_devices() = 0;

    // ─── Stream lifecycle ──────────────────────────────────────
    /// Open a stream on the given device with the given parameters.
    /// @return true if the device was successfully opened.
    virtual bool open(const DeviceInfo& dev, uint32_t sample_rate,
                      uint32_t buffer_size) = 0;

    /// Start audio processing (calls the process callback).
    virtual bool start() = 0;

    /// Stop audio processing.
    virtual void stop() = 0;

    /// Close the stream and release all associated resources.
    virtual void close() = 0;

    // ─── Stream state ──────────────────────────────────────────
    /// Current sample rate of the opened stream.
    virtual uint32_t current_sample_rate() const = 0;

    /// Current buffer size in frames of the opened stream.
    virtual uint32_t current_buffer_size() const = 0;

    /// Whether a stream is currently open.
    virtual bool is_open() const = 0;

    /// Whether the stream is currently running.
    virtual bool is_running() const = 0;

    // ─── Process callback ──────────────────────────────────────
    /// Signature for the audio processing callback.
    /// Called from the audio thread — MUST be real-time safe.
    /// @param input   Array of input channel pointers (can be nullptr).
    /// @param output  Array of output channel pointers (can be nullptr).
    /// @param frames  Number of frames to process.
    using ProcessCallback = std::function<void(float** input, float** output,
                                                uint32_t frames)>;

    /// Set the real-time audio processing callback.
    void set_process_callback(ProcessCallback cb) { callback_ = std::move(cb); }

protected:
    AudioDriver() = default;
    ProcessCallback callback_;
};

} // namespace aria

#endif // ARIA_AUDIO_DRIVER_H
