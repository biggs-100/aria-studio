#include "wasapi_driver.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <comdef.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <cstring>
#include <vector>

// ─── COM Smart Pointer Helper ───────────────────────────────────
// Simple RAII wrapper for COM interface pointers.
template <typename T>
class ComPtr {
public:
    ComPtr() : ptr_(nullptr) {}
    explicit ComPtr(T* p) : ptr_(p) { if (ptr_) ptr_->AddRef(); }
    ~ComPtr() { if (ptr_) ptr_->Release(); }

    ComPtr(const ComPtr& other) : ptr_(other.ptr_) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr& operator=(const ComPtr& other) {
        if (this != &other) {
            if (ptr_) ptr_->Release();
            ptr_ = other.ptr_;
            if (ptr_) ptr_->AddRef();
        }
        return *this;
    }
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) ptr_->Release();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* operator->() const { return ptr_; }
    T** operator&() { return &ptr_; }
    operator T*() const { return ptr_; }
    T* get() const { return ptr_; }
    void reset() { if (ptr_) { ptr_->Release(); ptr_ = nullptr; } }

    template <typename Q>
    HRESULT QueryInterface(REFIID riid, Q** ppv) const {
        return ptr_->QueryInterface(riid, reinterpret_cast<void**>(ppv));
    }

private:
    T* ptr_;
};

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════



/// Get a device-friendly name from the property store.
static std::string get_device_name(IMMDevice* device) {
    ComPtr<IPropertyStore> props;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) {
        return "Unknown";
    }

    PROPVARIANT var;
    PropVariantInit(&var);

    std::string name = "Unknown";
    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var)) &&
        var.vt == VT_LPWSTR)
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1,
                                       nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            name.resize(static_cast<size_t>(len) - 1);
            WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1,
                                name.data(), len, nullptr, nullptr);
        }
    }

    PropVariantClear(&var);
    return name;
}

// ═══════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

WasapiDriver::WasapiDriver() = default;

WasapiDriver::~WasapiDriver() {
    close();
}

// ═══════════════════════════════════════════════════════════════
// COM Initialization
// ═══════════════════════════════════════════════════════════════

bool WasapiDriver::initialize_com() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE || hr == S_FALSE;
}

void WasapiDriver::uninitialize_com() {
    CoUninitialize();
}

// ═══════════════════════════════════════════════════════════════
// Device Enumeration
// ═══════════════════════════════════════════════════════════════

std::vector<AudioDriver::DeviceInfo> WasapiDriver::enumerate_devices() {
    std::vector<DeviceInfo> devices;

    if (!initialize_com()) return devices;

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator));

    if (FAILED(hr) || !enumerator) {
        uninitialize_com();
        return devices;
    }

    // Enumerate render (output) devices
    ComPtr<IMMDeviceCollection> render_collection;
    hr = enumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE, &render_collection);

    if (SUCCEEDED(hr) && render_collection) {
        UINT count = 0;
        render_collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> device;
            if (FAILED(render_collection->Item(i, &device))) continue;

            DeviceInfo info;
            std::string dev_name = get_device_name(device);
            info.name = dev_name;

            // Generate a persistent ID from the device ID
            WCHAR* dev_id = nullptr;
            if (SUCCEEDED(device->GetId(&dev_id)) && dev_id) {
                int len = WideCharToMultiByte(CP_UTF8, 0, dev_id, -1,
                                               nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    info.id.resize(static_cast<size_t>(len) - 1);
                    WideCharToMultiByte(CP_UTF8, 0, dev_id, -1,
                                        info.id.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(dev_id);
            }

            // Check if default device
            ComPtr<IMMDevice> default_dev;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(
                    eRender, eConsole, &default_dev)))
            {
                WCHAR* default_id = nullptr;
                if (SUCCEEDED(default_dev->GetId(&default_id))) {
                    std::wstring def_id(default_id);
                    if (dev_id && def_id == dev_id) {
                        info.is_default = true;
                    }
                    CoTaskMemFree(default_id);
                }
            }

            info.output_channels = 2;  // Stereo minimum
            info.sample_rates = { 44100, 48000, 96000, 192000 };
            info.buffer_sizes = { 64, 128, 256, 512, 1024 };

            devices.push_back(std::move(info));
        }
    }

    // Enumerate capture (input) devices
    ComPtr<IMMDeviceCollection> capture_collection;
    hr = enumerator->EnumAudioEndpoints(
        eCapture, DEVICE_STATE_ACTIVE, &capture_collection);

    if (SUCCEEDED(hr) && capture_collection) {
        UINT count = 0;
        capture_collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> device;
            if (FAILED(capture_collection->Item(i, &device))) continue;

            DeviceInfo info;
            info.name = get_device_name(device);

            WCHAR* dev_id = nullptr;
            if (SUCCEEDED(device->GetId(&dev_id)) && dev_id) {
                int len = WideCharToMultiByte(CP_UTF8, 0, dev_id, -1,
                                               nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    info.id.resize(static_cast<size_t>(len) - 1);
                    WideCharToMultiByte(CP_UTF8, 0, dev_id, -1,
                                        info.id.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(dev_id);
            }

            info.input_channels = 2;  // Stereo minimum
            info.sample_rates = { 44100, 48000, 96000, 192000 };
            info.buffer_sizes = { 64, 128, 256, 512, 1024 };

            devices.push_back(std::move(info));
        }
    }

    uninitialize_com();
    return devices;
}

// ═══════════════════════════════════════════════════════════════
// Open / Close
// ═══════════════════════════════════════════════════════════════

bool WasapiDriver::open(const DeviceInfo& dev, uint32_t sample_rate,
                         uint32_t buffer_size)
{
    if (open_.load()) close();
    if (!initialize_com()) return false;

    device_id_     = dev.id;
    input_channels_ = dev.input_channels;
    output_channels_= dev.output_channels;

    HRESULT hr;

    // ── Create device enumerator ──────────────────────────────
    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        uninitialize_com();
        return false;
    }

    // ── Get the device ────────────────────────────────────────
    // Convert device ID to wide string
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, device_id_.c_str(), -1,
                                        nullptr, 0);
    std::wstring wide_id(static_cast<size_t>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, device_id_.c_str(), -1,
                        wide_id.data(), wide_len);

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDevice(wide_id.c_str(), &device);
    if (FAILED(hr) || !device) {
        // Fallback: use default render endpoint
        hr = enumerator->GetDefaultAudioEndpoint(
            eRender, eConsole, &device);
        if (FAILED(hr)) {
            uninitialize_com();
            return false;
        }
    }

    // ── Get IAudioClient ──────────────────────────────────────
    ComPtr<IAudioClient> audio_client;
    hr = device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&audio_client));
    if (FAILED(hr) || !audio_client) {
        uninitialize_com();
        return false;
    }

    // ── Configure audio format ────────────────────────────────
    WAVEFORMATEXTENSIBLE wf = {};
    wf.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels       = static_cast<WORD>(output_channels_);
    wf.Format.nSamplesPerSec  = sample_rate;
    wf.Format.wBitsPerSample  = 32;
    wf.Format.nBlockAlign     = wf.Format.nChannels * (wf.Format.wBitsPerSample / 8);
    wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
    wf.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wf.Samples.wValidBitsPerSample = 32;
    wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    wf.dwChannelMask = output_channels_ >= 2
                         ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
                         : SPEAKER_FRONT_CENTER;

    // ── Create event handle for callback ──────────────────────
    event_handle_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!event_handle_) {
        uninitialize_com();
        return false;
    }

    // ── Initialize audio client ───────────────────────────────
    DWORD stream_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (exclusive_.load()) {
        stream_flags |= AUDCLNT_STREAMFLAGS_RATEADJUST;
    }

    REFERENCE_TIME buffer_duration =
        static_cast<REFERENCE_TIME>(
            10000000ULL * buffer_size / sample_rate);

    hr = audio_client->Initialize(
        exclusive_.load() ? AUDCLNT_SHAREMODE_EXCLUSIVE
                          : AUDCLNT_SHAREMODE_SHARED,
        stream_flags,
        exclusive_.load() ? buffer_duration : 0,
        exclusive_.load() ? buffer_duration : 0,
        reinterpret_cast<WAVEFORMATEX*>(&wf),
        nullptr);

    if (FAILED(hr)) {
        CloseHandle(event_handle_);
        event_handle_ = nullptr;
        uninitialize_com();
        return false;
    }

    // Get the actual buffer size
    UINT32 actual_buf_frames = 0;
    audio_client->GetBufferSize(&actual_buf_frames);

    hr = audio_client->SetEventHandle(event_handle_);
    if (FAILED(hr)) {
        CloseHandle(event_handle_);
        event_handle_ = nullptr;
        uninitialize_com();
        return false;
    }

    // ── Get render client (output) ────────────────────────────
    ComPtr<IAudioRenderClient> render_client;
    if (output_channels_ > 0) {
        hr = audio_client->GetService(
            __uuidof(IAudioRenderClient),
            reinterpret_cast<void**>(&render_client));
        if (FAILED(hr)) {
            CloseHandle(event_handle_);
            event_handle_ = nullptr;
            uninitialize_com();
            return false;
        }
    }

    // ── Get capture client (input) ────────────────────────────
    ComPtr<IAudioCaptureClient> capture_client;
    if (input_channels_ > 0) {
        hr = audio_client->GetService(
            __uuidof(IAudioCaptureClient),
            reinterpret_cast<void**>(&capture_client));
        if (FAILED(hr)) {
            CloseHandle(event_handle_);
            event_handle_ = nullptr;
            uninitialize_com();
            return false;
        }
    }

    // ── Store COM pointers (transfer ownership to void* members) ──
    // We AddRef here because the ComPtr destructors will Release.
    // The void* members are managed manually — we Release on close().
    enumerator->AddRef();
    mm_device_enumerator_ = enumerator.get();
    device->AddRef();
    mm_device_ = device.get();
    audio_client->AddRef();
    audio_client_ = audio_client.get();
    if (render_client) {
        render_client->AddRef();
        render_client_ = render_client.get();
    }
    if (capture_client) {
        capture_client->AddRef();
        capture_client_ = capture_client.get();
    }

    // ── Allocate scratch buffers ──────────────────────────────
    if (input_channels_ > 0) {
        input_scratch_ = new float*[input_channels_];
        for (uint32_t i = 0; i < input_channels_; ++i) {
            input_scratch_[i] = new float[actual_buf_frames]();
        }
    }
    if (output_channels_ > 0) {
        output_scratch_ = new float*[output_channels_];
        for (uint32_t i = 0; i < output_channels_; ++i) {
            output_scratch_[i] = new float[actual_buf_frames]();
        }
    }

    sample_rate_ = sample_rate;
    buffer_size_ = static_cast<uint32_t>(actual_buf_frames);
    open_.store(true);
    return true;
}

void WasapiDriver::close() {
    if (!open_.load()) return;

    stop();

    // ── Release COM objects ───────────────────────────────────
    if (audio_client_) {
        static_cast<IAudioClient*>(audio_client_)->Stop();
        static_cast<IAudioClient*>(audio_client_)->Release();
        audio_client_ = nullptr;
    }
    if (render_client_) {
        static_cast<IAudioRenderClient*>(render_client_)->Release();
        render_client_ = nullptr;
    }
    if (capture_client_) {
        static_cast<IAudioCaptureClient*>(capture_client_)->Release();
        capture_client_ = nullptr;
    }
    if (mm_device_) {
        static_cast<IMMDevice*>(mm_device_)->Release();
        mm_device_ = nullptr;
    }
    if (mm_device_enumerator_) {
        static_cast<IMMDeviceEnumerator*>(mm_device_enumerator_)->Release();
        mm_device_enumerator_ = nullptr;
    }

    if (event_handle_) {
        CloseHandle(event_handle_);
        event_handle_ = nullptr;
    }

    // ── Free scratch buffers ──────────────────────────────────
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

    input_channels_  = 0;
    output_channels_ = 0;
    sample_rate_.store(0);
    buffer_size_.store(0);
    open_.store(false);
    running_.store(false);

    uninitialize_com();
}

// ═══════════════════════════════════════════════════════════════
// Start / Stop
// ═══════════════════════════════════════════════════════════════

bool WasapiDriver::start() {
    if (!open_.load() || running_.load()) return false;

    IAudioClient* client = static_cast<IAudioClient*>(audio_client_);
    if (!client) return false;

    HRESULT hr = client->Start();
    if (FAILED(hr)) return false;

    running_.store(true);

    // ── Spawn audio thread ────────────────────────────────────
    std::lock_guard<std::mutex> lock(thread_mutex_);
    audio_thread_ = std::make_unique<std::thread>(
        &WasapiDriver::audio_thread_func, this);

    return true;
}

void WasapiDriver::stop() {
    if (!running_.load()) return;

    running_.store(false);

    // Signal the event to wake up the audio thread so it can exit
    if (event_handle_) {
        SetEvent(static_cast<HANDLE>(event_handle_));
    }

    // Wait for the audio thread to finish
    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        if (audio_thread_ && audio_thread_->joinable()) {
            audio_thread_->join();
        }
        audio_thread_.reset();
    }

    if (audio_client_) {
        static_cast<IAudioClient*>(audio_client_)->Stop();
    }
}

// ═══════════════════════════════════════════════════════════════
// State Accessors
// ═══════════════════════════════════════════════════════════════

uint32_t WasapiDriver::current_sample_rate() const {
    return sample_rate_.load();
}

uint32_t WasapiDriver::current_buffer_size() const {
    return buffer_size_.load();
}

bool WasapiDriver::is_open() const {
    return open_.load();
}

bool WasapiDriver::is_running() const {
    return running_.load();
}

// ═══════════════════════════════════════════════════════════════
// Audio Thread
// ═══════════════════════════════════════════════════════════════

void WasapiDriver::audio_thread_func() {
    // Set high-priority for the audio thread
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    IAudioRenderClient* render   = static_cast<IAudioRenderClient*>(render_client_);
    IAudioCaptureClient* capture = static_cast<IAudioCaptureClient*>(capture_client_);
    HANDLE              event    = static_cast<HANDLE>(event_handle_);

    const uint32_t buf_frames = buffer_size_.load();
    const uint32_t n_inputs   = input_channels_;
    const uint32_t n_outputs  = output_channels_;

    while (running_.load()) {
        // Wait for the next buffer event
        DWORD wait = WaitForSingleObject(event, 100);
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0) break;

        if (!callback_) continue;

        // ── Read input (capture) ───────────────────────────────
        if (capture && n_inputs > 0 && input_scratch_) {
            UINT32 packet_frames = 0;
            BYTE* data = nullptr;
            DWORD flags = 0;

            HRESULT hr = capture->GetBuffer(&data, &packet_frames, &flags,
                                             nullptr, nullptr);
            if (SUCCEEDED(hr) && data && packet_frames > 0) {
                // De-interleave float data into planar format
                const float* src = reinterpret_cast<const float*>(data);
                for (uint32_t ch = 0; ch < n_inputs; ++ch) {
                    float* dst = input_scratch_[ch];
                    for (uint32_t f = 0; f < packet_frames; ++f) {
                        dst[f] = src[f * n_inputs + ch];
                    }
                }
                capture->ReleaseBuffer(packet_frames);
            }
        }

        // ── Zero output scratch ────────────────────────────────
        for (uint32_t ch = 0; ch < n_outputs; ++ch) {
            if (output_scratch_[ch]) {
                std::memset(output_scratch_[ch], 0,
                            static_cast<size_t>(buf_frames) * sizeof(float));
            }
        }

        // ── Invoke process callback ────────────────────────────
        float** in_ptr  = (n_inputs  > 0)  ? input_scratch_  : nullptr;
        float** out_ptr = (n_outputs > 0)  ? output_scratch_ : nullptr;
        callback_(in_ptr, out_ptr, buf_frames);

        // ── Write output (render) ──────────────────────────────
        if (render && n_outputs > 0 && output_scratch_) {
            BYTE* data = nullptr;
            HRESULT hr = render->GetBuffer(buf_frames, &data);
            if (SUCCEEDED(hr) && data) {
                // Interleave planar float into WASAPI buffer
                float* dst = reinterpret_cast<float*>(data);
                for (uint32_t f = 0; f < buf_frames; ++f) {
                    for (uint32_t ch = 0; ch < n_outputs; ++ch) {
                        dst[f * n_outputs + ch] = output_scratch_[ch][f];
                    }
                }
                render->ReleaseBuffer(buf_frames, 0);
            }
        }
    }
}

} // namespace aria

#endif // defined(_WIN32)
