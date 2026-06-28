#ifndef ARIA_ASIO_DRIVER_H
#define ARIA_ASIO_DRIVER_H

#include "audio_driver.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace aria {

/// ASIO audio driver for Windows.
///
/// Loads asiodrivers.dll / asio.dll at runtime and enumerates available
/// ASIO devices. Provides low-latency audio I/O via the ASIO protocol.
///
/// Platform: Windows only (compiled under _WIN32).
///
/// ASIO SDK types are declared as stubs — no ASIO SDK headers are required.
class AsioDriver final : public AudioDriver {
public:
    AsioDriver();
    ~AsioDriver() override;

    AsioDriver(const AsioDriver&) = delete;
    AsioDriver& operator=(const AsioDriver&) = delete;

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
    // ─── ASIO SDK Stub Types ───────────────────────────────────
    // These mirror the ASIO SDK types enough to load and call ASIO
    // functions via dynamic linking. The actual struct layouts match
    // the ASIO 2.3 specification.

    enum class AsioSampleType {
        Int16MSB   = 0,
        Int24MSB   = 1,
        Int32MSB   = 2,
        Float32MSB = 3,
        Float64MSB = 4,
        Int16LSB   = 8,
        Int24LSB   = 9,
        Int32LSB   = 10,
        Float32LSB = 11,
        Float64LSB = 12,
    };

    struct AsioBufferInfo {
        bool       is_input;     // true = input, false = output
        uint32_t   channel_num;
        void*      buffers[2];   // double-buffered
    };

    struct AsioTimeInfo {
        double     speed;
        uint64_t   sample_pos;
        double     system_time;
        uint32_t   flags;
        uint32_t   future[12];
    };

    struct AsioTime {
        int32_t    reserved[4];
        uint64_t   sample_pos;
        double     system_time;
        uint32_t   sample_rate;
        AsioTimeInfo time_info;
        uint32_t   future[16];
    };

    // ─── ASIA callbacks ────────────────────────────────────────
    struct AsioCallbacks {
        void* buffer_switch;
        void* sample_rate_did_change;
        void* async_sample_rate;
        void* buffer_switch_time_info;
        void* message;
    };

    // ─── Internal helpers ──────────────────────────────────────

    /// Attempt to load the ASIO DLL and retrieve the driver entry point.
    bool load_asio_dll();

    /// Unload the ASIO DLL.
    void unload_asio_dll();

    /// Actual buffer switch handler — called from the real-time ASIO thread.
    static void buffer_switch_handler(long double_buffer_index,
                                      AsioDriver* self);

    /// Buffer switch with time info (preferred on modern ASIO).
    static AsioTime* buffer_switch_time_info_handler(
        AsioTime* params, long double_buffer_index, AsioDriver* self);

    /// Internal: invoke the process callback from the ASIO buffer switch.
    void process_buffers(long double_buffer_index);

    // ─── DLL state ─────────────────────────────────────────────
    void*          asio_dll_ = nullptr;

    // Function pointers resolved from the DLL.
    using AsioInitFunc         = long (*)(void*);
    using AsioGetDriverNameFunc = long (*)(char*, long*);
    using AsioGetChannelsFunc  = long (*)(long*, long*);
    using AsioCreateBuffersFunc= long (*)(AsioBufferInfo*, long, long,
                                           AsioCallbacks*);
    using AsioStartFunc        = long (*)();
    using AsioStopFunc         = long (*)();
    using AsioDisposeBuffersFunc = long (*)();
    using AsioExitFunc         = long (*)();
    using AsioGetSampleRateFunc = long (*)(double*);
    using AsioOutputReadyFunc  = long (*)();

    AsioInitFunc             asio_init_             = nullptr;
    AsioGetDriverNameFunc     asio_get_driver_name_  = nullptr;
    AsioGetChannelsFunc       asio_get_channels_     = nullptr;
    AsioCreateBuffersFunc     asio_create_buffers_   = nullptr;
    AsioStartFunc             asio_start_            = nullptr;
    AsioStopFunc              asio_stop_             = nullptr;
    AsioDisposeBuffersFunc    asio_dispose_buffers_  = nullptr;
    AsioExitFunc              asio_exit_             = nullptr;
    AsioGetSampleRateFunc     asio_get_sample_rate_  = nullptr;
    AsioOutputReadyFunc       asio_output_ready_     = nullptr;

    // ─── Device state ──────────────────────────────────────────
    std::string  device_name_;
    uint32_t     input_channels_  = 0;
    uint32_t     output_channels_ = 0;

    std::atomic<uint32_t> sample_rate_{0};
    std::atomic<uint32_t> buffer_size_{0};
    std::atomic<bool>     open_{false};
    std::atomic<bool>     running_{false};

    // Buffers created via ASIO CreateBuffers.
    AsioBufferInfo* buffer_infos_ = nullptr;
    long            num_buffers_  = 0;

    // Scratch buffers for converting ASIO format to float planar.
    // Allocated once on open() — real-time safe.
    float**         input_scratch_  = nullptr;
    float**         output_scratch_ = nullptr;

    // Synchronization for create_buffers / dispose_buffers.
    std::mutex      buffer_mutex_;
};

} // namespace aria

#endif // ARIA_ASIO_DRIVER_H
