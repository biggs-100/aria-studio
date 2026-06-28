#include "asio_driver.h"

#if defined(_WIN32)

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// ASIO-Specific Constants
// ═══════════════════════════════════════════════════════════════

static constexpr long kAsioTrue  = 1;
static constexpr long kAsioFalse = 0;

// Callback flag bits (AsioTimeInfo.flags)
static constexpr uint32_t kAsioSystemTimeValid    = 1u << 0;
static constexpr uint32_t kAsioSamplePositionValid = 1u << 1;
static constexpr uint32_t kAsioSampleRateValid     = 1u << 2;

// Message IDs (simplified)
static constexpr uint32_t kAsioMessageResetRequest = 1;
static constexpr uint32_t kAsioMessageBufferSizeChange = 3;
static constexpr uint32_t kAsioMessageResyncRequest = 4;

// ═══════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

AsioDriver::AsioDriver() = default;

AsioDriver::~AsioDriver() {
    close();
}

// ═══════════════════════════════════════════════════════════════
// DLL Loading
// ═══════════════════════════════════════════════════════════════

bool AsioDriver::load_asio_dll() {
    if (asio_dll_) return true;  // already loaded

    // Try common ASIO DLL names
    static const char* kDllNames[] = {
        "asiodrivers.dll",
        "asio.dll",
        nullptr
    };

    for (const char* name : kDllNames) {
        if (!name) break;
        asio_dll_ = reinterpret_cast<void*>(
            LoadLibraryA(name));
        if (asio_dll_) break;
    }

    if (!asio_dll_) return false;

    // Resolve exported functions from the ASIO DLL.
    // ASIO exports its API via ordinal or named exports.
    // We try both conventions.
    auto resolve = [&](const char* name) -> FARPROC {
        return GetProcAddress(static_cast<HMODULE>(asio_dll_), name);
    };

#define RESOLVE_ORDINAL(name, ordinal)                                  \
    do {                                                                \
        (name) = reinterpret_cast<decltype(name)>(                      \
            GetProcAddress(static_cast<HMODULE>(asio_dll_),             \
                           MAKEINTRESOURCEA(ordinal)));                 \
        if (!(name)) (name) = reinterpret_cast<decltype(name)>(         \
            resolve(#name));                                            \
    } while(0)

    RESOLVE_ORDINAL(asio_init_,             1);
    RESOLVE_ORDINAL(asio_get_driver_name_,  2);
    RESOLVE_ORDINAL(asio_get_channels_,     3);
    RESOLVE_ORDINAL(asio_create_buffers_,   4);
    RESOLVE_ORDINAL(asio_start_,            5);
    RESOLVE_ORDINAL(asio_stop_,             6);
    RESOLVE_ORDINAL(asio_dispose_buffers_,  7);
    RESOLVE_ORDINAL(asio_exit_,             8);
    RESOLVE_ORDINAL(asio_get_sample_rate_,  9);
    RESOLVE_ORDINAL(asio_output_ready_,     10);

#undef RESOLVE_ORDINAL

    if (!asio_init_) {
        FreeLibrary(static_cast<HMODULE>(asio_dll_));
        asio_dll_ = nullptr;
        return false;
    }

    return true;
}

void AsioDriver::unload_asio_dll() {
    if (asio_dll_) {
        FreeLibrary(static_cast<HMODULE>(asio_dll_));
        asio_dll_ = nullptr;
    }

    asio_init_             = nullptr;
    asio_get_driver_name_  = nullptr;
    asio_get_channels_     = nullptr;
    asio_create_buffers_   = nullptr;
    asio_start_            = nullptr;
    asio_stop_             = nullptr;
    asio_dispose_buffers_  = nullptr;
    asio_exit_             = nullptr;
    asio_get_sample_rate_  = nullptr;
    asio_output_ready_     = nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Device Enumeration
// ═══════════════════════════════════════════════════════════════

std::vector<AudioDriver::DeviceInfo> AsioDriver::enumerate_devices() {
    std::vector<DeviceInfo> devices;

    if (!load_asio_dll()) return devices;

    // Initialize the ASIO driver
    if (asio_init_ && asio_init_(nullptr) == 0) {
        char name_buf[256] = {};
        long version = 0;

        if (asio_get_driver_name_ &&
            asio_get_driver_name_(name_buf, &version) == 0)
        {
            DeviceInfo info;
            info.id   = name_buf;
            info.name = name_buf;

            long in_ch  = 0;
            long out_ch = 0;
            if (asio_get_channels_ &&
                asio_get_channels_(&in_ch, &out_ch) == 0)
            {
                info.input_channels  = static_cast<uint32_t>(in_ch);
                info.output_channels = static_cast<uint32_t>(out_ch);
            }

            // Common sample rates supported by ASIO
            info.sample_rates = { 44100, 48000, 88200, 96000, 176400, 192000 };
            info.buffer_sizes = { 32, 64, 128, 256, 512, 1024 };

            devices.push_back(std::move(info));
        }

        // Exit the driver after enumeration
        if (asio_exit_) asio_exit_();
    }

    return devices;
}

// ═══════════════════════════════════════════════════════════════
// Open / Close
// ═══════════════════════════════════════════════════════════════

bool AsioDriver::open(const DeviceInfo& dev, uint32_t sample_rate,
                       uint32_t buffer_size)
{
    if (open_.load()) close();

    if (!load_asio_dll()) return false;

    // Initialize ASIO
    if (asio_init_ && asio_init_(nullptr) != 0) {
        return false;
    }

    device_name_    = dev.name;
    input_channels_ = dev.input_channels;
    output_channels_= dev.output_channels;

    // ── Prepare buffer info structures ────────────────────────
    long total_inputs  = static_cast<long>(input_channels_);
    long total_outputs = static_cast<long>(output_channels_);
    num_buffers_ = total_inputs + total_outputs;

    buffer_infos_ = new AsioBufferInfo[num_buffers_];

    for (long i = 0; i < total_inputs; ++i) {
        buffer_infos_[i].is_input    = true;
        buffer_infos_[i].channel_num = i;
        buffer_infos_[i].buffers[0]  = nullptr;
        buffer_infos_[i].buffers[1]  = nullptr;
    }
    for (long i = 0; i < total_outputs; ++i) {
        buffer_infos_[total_inputs + i].is_input    = false;
        buffer_infos_[total_inputs + i].channel_num = i;
        buffer_infos_[total_inputs + i].buffers[0]  = nullptr;
        buffer_infos_[total_inputs + i].buffers[1]  = nullptr;
    }

    // ── Prepare callbacks ─────────────────────────────────────
    AsioCallbacks cb;
    cb.buffer_switch =
        reinterpret_cast<void*>(&AsioDriver::buffer_switch_handler);
    cb.sample_rate_did_change = nullptr;
    cb.async_sample_rate      = nullptr;
    cb.buffer_switch_time_info =
        reinterpret_cast<void*>(&AsioDriver::buffer_switch_time_info_handler);
    cb.message                = nullptr;

    // ── Create buffers ────────────────────────────────────────
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        long preferred_size = static_cast<long>(buffer_size);
        if (asio_create_buffers_) {
            long result = asio_create_buffers_(
                buffer_infos_, num_buffers_, preferred_size, &cb);
            if (result != 0) {
                delete[] buffer_infos_;
                buffer_infos_ = nullptr;
                num_buffers_  = 0;
                return false;
            }
        }
    }

    // Query actual buffer size from ASIO
    buffer_size_ = static_cast<uint32_t>(buffer_size);
    sample_rate_ = sample_rate;

    // ── Allocate scratch conversion buffers ───────────────────
    // ASIO delivers interleaved/native format; we convert to float planar.
    if (input_channels_ > 0) {
        input_scratch_ = new float*[input_channels_];
        for (uint32_t i = 0; i < input_channels_; ++i) {
            input_scratch_[i] = new float[buffer_size]();
        }
    }
    if (output_channels_ > 0) {
        output_scratch_ = new float*[output_channels_];
        for (uint32_t i = 0; i < output_channels_; ++i) {
            output_scratch_[i] = new float[buffer_size]();
        }
    }

    open_.store(true);
    return true;
}

void AsioDriver::close() {
    if (!open_.load()) return;

    stop();

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (asio_dispose_buffers_) {
            asio_dispose_buffers_();
        }
        if (buffer_infos_) {
            delete[] buffer_infos_;
            buffer_infos_ = nullptr;
            num_buffers_  = 0;
        }
    }

    // Free scratch buffers
    if (input_scratch_) {
        for (uint32_t i = 0; i < input_channels_; ++i) {
            delete[] input_scratch_[i];
        }
        delete[] input_scratch_;
        input_scratch_ = nullptr;
    }
    if (output_scratch_) {
        for (uint32_t i = 0; i < output_channels_; ++i) {
            delete[] output_scratch_[i];
        }
        delete[] output_scratch_;
        output_scratch_ = nullptr;
    }

    if (asio_exit_) asio_exit_();

    input_channels_  = 0;
    output_channels_ = 0;
    sample_rate_.store(0);
    buffer_size_.store(0);
    open_.store(false);
    running_.store(false);

    unload_asio_dll();
}

// ═══════════════════════════════════════════════════════════════
// Start / Stop
// ═══════════════════════════════════════════════════════════════

bool AsioDriver::start() {
    if (!open_.load() || running_.load()) return false;

    if (asio_start_ && asio_start_() == 0) {
        running_.store(true);
        return true;
    }
    return false;
}

void AsioDriver::stop() {
    if (!running_.load()) return;

    if (asio_stop_) asio_stop_();
    running_.store(false);
}

// ═══════════════════════════════════════════════════════════════
// State Accessors
// ═══════════════════════════════════════════════════════════════

uint32_t AsioDriver::current_sample_rate() const {
    // Try to query actual sample rate from ASIO driver
    if (asio_get_sample_rate_) {
        double rate = 0.0;
        if (asio_get_sample_rate_(&rate) == 0 && rate > 0.0) {
            return static_cast<uint32_t>(rate + 0.5);
        }
    }
    return sample_rate_.load();
}

uint32_t AsioDriver::current_buffer_size() const {
    return buffer_size_.load();
}

bool AsioDriver::is_open() const {
    return open_.load();
}

bool AsioDriver::is_running() const {
    return running_.load();
}

// ═══════════════════════════════════════════════════════════════
// ASIO Callbacks
// ═══════════════════════════════════════════════════════════════

void AsioDriver::buffer_switch_handler(long double_buffer_index,
                                        AsioDriver* self)
{
    self->process_buffers(double_buffer_index);

    // Notify ASIO that output buffers are ready.
    if (self->asio_output_ready_) {
        self->asio_output_ready_();
    }
}

AsioDriver::AsioTime*
AsioDriver::buffer_switch_time_info_handler(AsioTime* /*params*/,
                                             long double_buffer_index,
                                             AsioDriver* self)
{
    self->process_buffers(double_buffer_index);
    if (self->asio_output_ready_) {
        self->asio_output_ready_();
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Process Buffers (Audio Thread)
// ═══════════════════════════════════════════════════════════════

void AsioDriver::process_buffers(long double_buffer_index) {
    if (!callback_) return;

    const uint32_t frames = buffer_size_.load();
    if (frames == 0) return;

    const uint32_t n_inputs  = input_channels_;
    const uint32_t n_outputs = output_channels_;

    // ── Convert ASIO input buffers → float planar ─────────────
    for (uint32_t ch = 0; ch < n_inputs; ++ch) {
        auto& info = buffer_infos_[ch];
        void* src  = info.buffers[double_buffer_index];
        if (src && ch < n_inputs && input_scratch_[ch]) {
            // ASIO commonly delivers 32-bit float or 32-bit int samples.
            // We assume Float32LSB — the most common for modern ASIO drivers.
            std::memcpy(input_scratch_[ch], src,
                        static_cast<size_t>(frames) * sizeof(float));
        }
    }

    // ── Zero output scratch (safety: silence if callback doesn't fill) ─
    for (uint32_t ch = 0; ch < n_outputs; ++ch) {
        if (output_scratch_[ch]) {
            std::memset(output_scratch_[ch], 0,
                        static_cast<size_t>(frames) * sizeof(float));
        }
    }

    // ── Invoke the process callback ────────────────────────────
    float** in_ptr  = (n_inputs  > 0)  ? input_scratch_  : nullptr;
    float** out_ptr = (n_outputs > 0)  ? output_scratch_ : nullptr;

    callback_(in_ptr, out_ptr, frames);

    // ── Convert float planar output → ASIO output buffers ─────
    for (uint32_t ch = 0; ch < n_outputs; ++ch) {
        auto& info = buffer_infos_[n_inputs + ch];
        void* dst  = info.buffers[double_buffer_index];
        if (dst && ch < n_outputs && output_scratch_[ch]) {
            std::memcpy(dst, output_scratch_[ch],
                        static_cast<size_t>(frames) * sizeof(float));
        }
    }
}

} // namespace aria

#endif // defined(_WIN32)
