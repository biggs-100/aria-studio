#include "alsa_driver.h"

#if defined(__linux__)

#include <alsa/asoundlib.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

AlsaDriver::AlsaDriver() = default;

AlsaDriver::~AlsaDriver() {
    close();
}

// ═══════════════════════════════════════════════════════════════
// Device Enumeration
// ═══════════════════════════════════════════════════════════════

std::vector<AudioDriver::DeviceInfo> AlsaDriver::enumerate_devices() {
    std::vector<DeviceInfo> devices;

    void** hints = nullptr;
    if (snd_device_name_hint(-1, "pcm", &hints) < 0 || !hints) {
        return devices;
    }

    for (int i = 0; hints[i]; ++i) {
        char* name = snd_device_name_get_hint(hints[i], "NAME");
        char* desc = snd_device_name_get_hint(hints[i], "DESC");
        char* ioid = snd_device_name_get_hint(hints[i], "IOID");

        if (name) {
            DeviceInfo info;
            info.id   = name;
            info.name = desc ? desc : name;

            // Determine I/O direction
            bool is_input  = (ioid == nullptr || std::strcmp(ioid, "Input") == 0);
            bool is_output = (ioid == nullptr || std::strcmp(ioid, "Output") == 0);

            if (is_output) info.output_channels = 2;
            if (is_input)  info.input_channels  = 2;

            info.sample_rates = { 44100, 48000, 96000, 192000 };
            info.buffer_sizes = { 64, 128, 256, 512, 1024 };

            // Check if "default" or similar
            if (std::strcmp(name, "default") == 0) {
                info.is_default = true;
            }

            devices.push_back(std::move(info));
        }

        if (name) free(name);
        if (desc) free(desc);
        if (ioid) free(ioid);
    }

    snd_device_name_free_hint(hints);
    return devices;
}

// ═══════════════════════════════════════════════════════════════
// Open / Close
// ═══════════════════════════════════════════════════════════════

bool AlsaDriver::open(const DeviceInfo& dev, uint32_t sample_rate,
                       uint32_t buffer_size)
{
    if (open_.load()) close();

    device_name_     = dev.id;
    input_channels_  = dev.input_channels;
    output_channels_ = dev.output_channels;

    // ── Open PCM device ───────────────────────────────────────
    snd_pcm_t* pcm = nullptr;
    int err = snd_pcm_open(&pcm, device_name_.c_str(),
                           SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0 || !pcm) {
        return false;
    }

    // ── Set hardware parameters ───────────────────────────────
    snd_pcm_hw_params_t* hw_params = nullptr;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(pcm, hw_params);

    snd_pcm_hw_params_set_access(pcm, hw_params,
        use_mmap_.load() ? SND_PCM_ACCESS_MMAP_INTERLEAVED
                         : SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_FLOAT_LE);

    unsigned int rate = sample_rate;
    snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, nullptr);

    snd_pcm_hw_params_set_channels(pcm, hw_params,
                                   output_channels_);

    snd_pcm_uframes_t period = buffer_size;
    snd_pcm_hw_params_set_period_size_near(pcm, hw_params,
                                            &period, nullptr);

    err = snd_pcm_hw_params(pcm, hw_params);
    if (err < 0) {
        snd_pcm_close(pcm);
        return false;
    }

    // ── Set software parameters ───────────────────────────────
    snd_pcm_sw_params_t* sw_params = nullptr;
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(pcm, sw_params);

    snd_pcm_sw_params_set_avail_min(pcm, sw_params, period);

    snd_pcm_sw_params_set_start_threshold(pcm, sw_params, 0);

    err = snd_pcm_sw_params(pcm, sw_params);
    if (err < 0) {
        snd_pcm_close(pcm);
        return false;
    }

    // ── Allocate scratch buffers ──────────────────────────────
    const uint32_t frames = static_cast<uint32_t>(period);

    if (input_channels_ > 0) {
        input_scratch_ = new float*[input_channels_];
        for (uint32_t i = 0; i < input_channels_; ++i) {
            input_scratch_[i] = new float[frames]();
        }
    }
    if (output_channels_ > 0) {
        output_scratch_ = new float*[output_channels_];
        for (uint32_t i = 0; i < output_channels_; ++i) {
            output_scratch_[i] = new float[frames]();
        }
    }

    pcm_handle_ = pcm;
    sample_rate_ = rate;
    buffer_size_ = frames;
    open_.store(true);
    return true;
}

void AlsaDriver::close() {
    if (!open_.load()) return;

    stop();

    if (pcm_handle_) {
        snd_pcm_drain(static_cast<snd_pcm_t*>(pcm_handle_));
        snd_pcm_close(static_cast<snd_pcm_t*>(pcm_handle_));
        pcm_handle_ = nullptr;
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

    input_channels_  = 0;
    output_channels_ = 0;
    sample_rate_.store(0);
    buffer_size_.store(0);
    open_.store(false);
    running_.store(false);
}

// ═══════════════════════════════════════════════════════════════
// Start / Stop
// ═══════════════════════════════════════════════════════════════

bool AlsaDriver::start() {
    if (!open_.load() || running_.load()) return false;
    if (!pcm_handle_) return false;

    snd_pcm_t* pcm = static_cast<snd_pcm_t*>(pcm_handle_);

    // Prepare PCM for playback
    int err = snd_pcm_prepare(pcm);
    if (err < 0) return false;

    running_.store(true);

    // Spawn audio thread
    std::lock_guard<std::mutex> lock(thread_mutex_);
    audio_thread_ = std::make_unique<std::thread>(
        &AlsaDriver::audio_thread_func, this);

    return true;
}

void AlsaDriver::stop() {
    if (!running_.load()) return;

    running_.store(false);

    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        if (audio_thread_ && audio_thread_->joinable()) {
            audio_thread_->join();
        }
        audio_thread_.reset();
    }

    if (pcm_handle_) {
        snd_pcm_drop(static_cast<snd_pcm_t*>(pcm_handle_));
    }
}

// ═══════════════════════════════════════════════════════════════
// State Accessors
// ═══════════════════════════════════════════════════════════════

uint32_t AlsaDriver::current_sample_rate() const {
    return sample_rate_.load();
}

uint32_t AlsaDriver::current_buffer_size() const {
    return buffer_size_.load();
}

bool AlsaDriver::is_open() const {
    return open_.load();
}

bool AlsaDriver::is_running() const {
    return running_.load();
}

// ═══════════════════════════════════════════════════════════════
// Audio Thread
// ═══════════════════════════════════════════════════════════════

void AlsaDriver::audio_thread_func() {
    snd_pcm_t* pcm = static_cast<snd_pcm_t*>(pcm_handle_);
    if (!pcm) return;

    const uint32_t buf_frames = buffer_size_.load();
    const uint32_t n_inputs   = input_channels_;
    const uint32_t n_outputs  = output_channels_;

    // Interleaved buffer for ALSA I/O
    std::vector<float> interleaved(
        static_cast<size_t>(buf_frames) * std::max(n_inputs, n_outputs));

    while (running_.load()) {
        if (!callback_) {
            // Write silence if no callback
            if (pcm) {
                interleaved.assign(interleaved.size(), 0.0f);
                snd_pcm_writei(pcm, interleaved.data(), buf_frames);
            }
            continue;
        }

        // ── Read input (capture) ──
        // ALSA duplex is complex; for simplicity, if input is required
        // we would open a separate pcm capture stream. For this
        // implementation we focus on output playback.

        // ── Zero output scratch ────────────────────────────────
        for (uint32_t ch = 0; ch < n_outputs; ++ch) {
            if (output_scratch_[ch]) {
                std::memset(output_scratch_[ch], 0,
                            static_cast<size_t>(buf_frames) * sizeof(float));
            }
        }

        // ── Invoke process callback ────────────────────────────
        float** in_ptr  = (n_inputs > 0)  ? input_scratch_  : nullptr;
        float** out_ptr = (n_outputs > 0) ? output_scratch_ : nullptr;
        callback_(in_ptr, out_ptr, buf_frames);

        // ── Interleave output and write to ALSA ────────────────
        for (uint32_t f = 0; f < buf_frames; ++f) {
            for (uint32_t ch = 0; ch < n_outputs; ++ch) {
                interleaved[f * n_outputs + ch] = output_scratch_[ch][f];
            }
        }

        snd_pcm_sframes_t written = snd_pcm_writei(
            pcm, interleaved.data(), buf_frames);

        if (written < 0) {
            // Handle underrun / XRUN
            snd_pcm_recover(pcm, static_cast<int>(written), 0);
        }
    }
}

} // namespace aria

#endif // defined(__linux__)
