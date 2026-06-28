#include "pipewire_driver.h"

#if defined(__linux__)

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/result.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

PipeWireDriver::PipeWireDriver() {
    // Initialize PipeWire once (ref-counted internally)
    pw_init(nullptr, nullptr);
}

PipeWireDriver::~PipeWireDriver() {
    close();
}

// ═══════════════════════════════════════════════════════════════
// Device Enumeration (simplified — uses pw_registry)
// ═══════════════════════════════════════════════════════════════

// Context for registry events
struct PwEnumContext {
    std::vector<AudioDriver::DeviceInfo>* devices = nullptr;
};

static void registry_event_global(void* data, uint32_t id,
                                   uint32_t permissions,
                                   const char* type, uint32_t version,
                                   const struct spa_dict* props)
{
    auto* ctx = static_cast<PwEnumContext*>(data);
    if (!ctx || !props) return;

    // Look for audio sink/source nodes
    const char* media_class = spa_dict_lookup(props, "media.class");
    if (!media_class) return;

    bool is_sink   = (std::strcmp(media_class, "Audio/Sink") == 0);
    bool is_source = (std::strcmp(media_class, "Audio/Source") == 0);

    if (!is_sink && !is_source) return;

    AudioDriver::DeviceInfo info;

    const char* node_name = spa_dict_lookup(props, "node.name");
    const char* node_desc = spa_dict_lookup(props, "node.description");
    const char* alsa_id   = spa_dict_lookup(props, "alsa.id");

    info.id   = node_name ? node_name : (alsa_id ? alsa_id : "unknown");
    info.name = node_desc ? node_desc : info.id;

    // Default stereo I/O
    if (is_sink)   info.output_channels = 2;
    if (is_source) info.input_channels  = 2;

    const char* rates_str = spa_dict_lookup(props, "audio.samplerates");
    if (rates_str) {
        // Parse space-separated sample rates
        std::string rates(rates_str);
        size_t pos = 0;
        while ((pos = rates.find(' ')) != std::string::npos) {
            auto sr = static_cast<uint32_t>(
                std::stoul(rates.substr(0, pos)));
            if (sr > 0) info.sample_rates.push_back(sr);
            rates.erase(0, pos + 1);
        }
        if (!rates.empty()) {
            auto sr = static_cast<uint32_t>(std::stoul(rates));
            if (sr > 0) info.sample_rates.push_back(sr);
        }
    }

    if (info.sample_rates.empty()) {
        info.sample_rates = { 44100, 48000, 96000, 192000 };
    }

    info.buffer_sizes = { 64, 128, 256, 512, 1024 };
    info.is_default = is_sink;  // heuristic: first sink is default-ish

    ctx->devices->push_back(std::move(info));
}

static void registry_event_global_remove(void* data, uint32_t id) {
    // Not needed for enumeration
}

static const struct pw_registry_events registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
    .global_remove = registry_event_global_remove,
};

std::vector<AudioDriver::DeviceInfo> PipeWireDriver::enumerate_devices() {
    if (!cached_devices_.empty()) return cached_devices_;

    std::vector<DeviceInfo> devices;

    pw_init(nullptr, nullptr);

    pw_main_loop* loop = pw_main_loop_new(nullptr);
    if (!loop) return devices;

    pw_context* context = pw_context_new(pw_main_loop_get_loop(loop),
                                          nullptr, 0);
    if (!context) {
        pw_main_loop_destroy(loop);
        return devices;
    }

    pw_core* core = pw_context_connect(context, nullptr, 0);
    if (!core) {
        pw_context_destroy(context);
        pw_main_loop_destroy(loop);
        return devices;
    }

    pw_registry* registry = pw_core_get_registry(core,
                                                  PW_VERSION_REGISTRY, 0);
    if (!registry) {
        pw_core_disconnect(core);
        pw_context_destroy(context);
        pw_main_loop_destroy(loop);
        return devices;
    }

    PwEnumContext enum_ctx;
    enum_ctx.devices = &devices;

    struct spa_hub_listener registry_listener;
    spa_zero(registry_listener);
    pw_registry_add_listener(registry, &registry_listener, &registry_events,
                              &enum_ctx);

    // Run the loop briefly to collect device info
    // A more robust implementation would use a timeout loop.
    for (int i = 0; i < 10; ++i) {
        pw_main_loop_run(loop);
    }

    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
    pw_core_disconnect(core);
    pw_context_destroy(context);
    pw_main_loop_destroy(loop);

    cached_devices_ = devices;
    return devices;
}

// ═══════════════════════════════════════════════════════════════
// Stream processing callback
// ═══════════════════════════════════════════════════════════════

struct PwStreamContext {
    PipeWireDriver* driver = nullptr;
};

static void stream_process_cb(void* data) {
    auto* ctx = static_cast<PwStreamContext*>(data);
    if (!ctx || !ctx->driver) return;

    PipeWireDriver* driver = ctx->driver;
    auto* stream = static_cast<pw_stream*>(driver->pw_stream_);
    if (!stream) return;

    pw_buffer* buf = nullptr;
    buf = pw_stream_dequeue_buffer(stream);
    if (!buf) return;

    struct spa_buffer* spa_buf = buf->buffer;

    if (!driver->callback_) {
        // Silence output
        for (uint32_t i = 0; i < spa_buf->n_datas; ++i) {
            if (spa_buf->datas[i].data) {
                std::memset(spa_buf->datas[i].data, 0,
                            spa_buf->datas[i].maxsize);
            }
        }
        pw_stream_queue_buffer(stream, buf);
        return;
    }

    const uint32_t frames = driver->buffer_size_.load();
    const uint32_t n_outputs = driver->output_channels_;

    // ── Zero output scratch ────────────────────────────────────
    for (uint32_t ch = 0; ch < n_outputs; ++ch) {
        if (driver->output_scratch_ && driver->output_scratch_[ch]) {
            std::memset(driver->output_scratch_[ch], 0,
                        static_cast<size_t>(frames) * sizeof(float));
        }
    }

    // ── Invoke process callback ────────────────────────────────
    float** in_ptr  = nullptr;  // Capture not yet implemented
    float** out_ptr = (n_outputs > 0) ? driver->output_scratch_ : nullptr;
    driver->callback_(in_ptr, out_ptr, frames);

    // ── Write interleaved output to PipeWire buffer ───────────
    if (spa_buf->datas[0].data && n_outputs > 0 && driver->output_scratch_) {
        float* dst = static_cast<float*>(spa_buf->datas[0].data);
        for (uint32_t f = 0; f < frames && f < n_outputs; ++f) {
            for (uint32_t ch = 0; ch < n_outputs; ++ch) {
                dst[f * n_outputs + ch] = driver->output_scratch_[ch][f];
            }
        }
    }

    pw_stream_queue_buffer(stream, buf);
}

static const struct pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .process = stream_process_cb,
};

// ═══════════════════════════════════════════════════════════════
// Open / Close
// ═══════════════════════════════════════════════════════════════

bool PipeWireDriver::open(const DeviceInfo& dev, uint32_t sample_rate,
                           uint32_t buffer_size)
{
    if (open_.load()) close();

    device_name_     = dev.id;
    input_channels_  = dev.input_channels;
    output_channels_ = dev.output_channels;

    pw_init(nullptr, nullptr);

    pw_main_loop* loop = pw_main_loop_new(nullptr);
    if (!loop) return false;

    pw_context* context = pw_context_new(pw_main_loop_get_loop(loop),
                                          nullptr, 0);
    if (!context) {
        pw_main_loop_destroy(loop);
        return false;
    }

    pw_core* core = pw_context_connect(context, nullptr, 0);
    if (!core) {
        pw_context_destroy(context);
        pw_main_loop_destroy(loop);
        return false;
    }

    // ── Create stream params ───────────────────────────────────
    uint8_t params_buffer[1024];
    auto* audio_info = reinterpret_cast<spa_audio_info_raw*>(params_buffer);

    spa_zero(*audio_info);
    audio_info->format   = SPA_AUDIO_FORMAT_F32;
    audio_info->rate     = sample_rate;
    audio_info->channels = output_channels_;

    // Build parameter
    spa_pod_builder pod_builder;
    spa_pod_builder_init(&pod_builder, params_buffer, sizeof(params_buffer));
    const spa_pod* pod = spa_format_audio_raw_build(&pod_builder,
                                                     SPA_PARAM_EnumFormat,
                                                     audio_info);

    // ── Create stream ──────────────────────────────────────────
    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_NODE_NAME, "aria-daw",
        PW_KEY_NODE_DESCRIPTION, "ARIA DAW Audio Engine",
        nullptr);

    pw_stream* stream = pw_stream_new(core, "aria-daw playback", props);

    auto* stream_ctx = new PwStreamContext();
    stream_ctx->driver = this;

    pw_stream_add_listener(stream, &stream_events, stream_ctx);

    // ── Connect stream ─────────────────────────────────────────
    pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                       PW_STREAM_FLAG_AUTOCONNECT
                       | PW_STREAM_FLAG_MAP_BUFFERS
                       | PW_STREAM_FLAG_RT_PRIORITY,
                       &pod, 1);

    // ── Allocate scratch buffers ──────────────────────────────
    if (output_channels_ > 0) {
        output_scratch_ = new float*[output_channels_];
        for (uint32_t i = 0; i < output_channels_; ++i) {
            output_scratch_[i] = new float[buffer_size]();
        }
    }

    pw_main_loop_   = loop;
    pw_context_     = context;
    pw_core_        = core;
    pw_stream_      = stream;

    sample_rate_ = sample_rate;
    buffer_size_ = buffer_size;
    open_.store(true);
    return true;
}

void PipeWireDriver::close() {
    if (!open_.load()) return;

    stop();

    if (pw_stream_) {
        pw_stream_disconnect(static_cast<pw_stream*>(pw_stream_));
        pw_stream_destroy(static_cast<pw_stream*>(pw_stream_));
        pw_stream_ = nullptr;
    }

    if (pw_core_) {
        pw_core_disconnect(static_cast<pw_core*>(pw_core_));
        pw_core_ = nullptr;
    }

    if (pw_context_) {
        pw_context_destroy(static_cast<pw_context*>(pw_context_));
        pw_context_ = nullptr;
    }

    if (pw_main_loop_) {
        pw_main_loop_destroy(static_cast<pw_main_loop*>(pw_main_loop_));
        pw_main_loop_ = nullptr;
    }

    // Free scratch buffers
    if (output_scratch_) {
        for (uint32_t i = 0; i < output_channels_; ++i) {
            delete[] output_scratch_[i];
        }
        delete[] output_scratch_;
        output_scratch_ = nullptr;
    }
    if (input_scratch_) {
        for (uint32_t i = 0; i < input_channels_; ++i) {
            delete[] input_scratch_[i];
        }
        delete[] input_scratch_;
        input_scratch_ = nullptr;
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

bool PipeWireDriver::start() {
    if (!open_.load() || running_.load()) return false;
    if (!pw_main_loop_) return false;

    running_.store(true);

    // Spawn thread to run the PipeWire main loop
    std::lock_guard<std::mutex> lock(thread_mutex_);
    audio_thread_ = std::make_unique<std::thread>(
        &PipeWireDriver::audio_thread_func, this);

    return true;
}

void PipeWireDriver::stop() {
    if (!running_.load()) return;

    running_.store(false);

    // Signal the main loop to quit
    if (pw_main_loop_) {
        pw_main_loop_quit(static_cast<pw_main_loop*>(pw_main_loop_));
    }

    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        if (audio_thread_ && audio_thread_->joinable()) {
            audio_thread_->join();
        }
        audio_thread_.reset();
    }
}

// ═══════════════════════════════════════════════════════════════
// State Accessors
// ═══════════════════════════════════════════════════════════════

uint32_t PipeWireDriver::current_sample_rate() const {
    return sample_rate_.load();
}

uint32_t PipeWireDriver::current_buffer_size() const {
    return buffer_size_.load();
}

bool PipeWireDriver::is_open() const {
    return open_.load();
}

bool PipeWireDriver::is_running() const {
    return running_.load();
}

// ═══════════════════════════════════════════════════════════════
// Audio Thread
// ═══════════════════════════════════════════════════════════════

void PipeWireDriver::audio_thread_func() {
    if (!pw_main_loop_) return;

    // Run the PipeWire main loop — it will call stream_process_cb
    pw_main_loop_run(static_cast<pw_main_loop*>(pw_main_loop_));
}

} // namespace aria

#endif // defined(__linux__)
