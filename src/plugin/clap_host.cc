#include "clap_host.h"

#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// ── Platform-specific dynamic library loading ────────────────────────

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#else
#include <dlfcn.h>
#endif

namespace aria::plugin {

// ══════════════════════════════════════════════════════════════════════
//  Platform helpers
// ══════════════════════════════════════════════════════════════════════

namespace {

void* dl_open(const char* path) {
#if defined(_WIN32)
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

void* dl_sym(void* handle, const char* symbol) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(
        static_cast<HMODULE>(handle), symbol));
#else
    return dlsym(handle, symbol);
#endif
}

void dl_close(void* handle) {
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

const char* dl_error() {
#if defined(_WIN32)
    static std::string msg;
    DWORD err = GetLastError();
    msg = "LoadLibrary error code: " + std::to_string(err);
    return msg.c_str();
#else
    return dlerror();
#endif
}

// ── AudioPlugin → CLAP category mapping ────────────────────────────

PluginCategory category_from_clap_features(const char* features) {
    if (!features) return PluginCategory::Effect;
    std::string_view f(features);

    if (f.find("instrument") != std::string_view::npos ||
        f.find("synth") != std::string_view::npos)
        return PluginCategory::Synth;

    if (f.find("analyzer") != std::string_view::npos)
        return PluginCategory::Analyzer;

    if (f.find("utility") != std::string_view::npos)
        return PluginCategory::Utility;

    return PluginCategory::Effect;
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════
//  CLAPHost — Host callbacks (static functions)
// ══════════════════════════════════════════════════════════════════════

namespace {

// Retrieve the CLAPHost* from the clap_host->host_data pointer.
inline CLAPHost* from_host(const clap_host* host) {
    return static_cast<CLAPHost*>(host->host_data);
}

// ── clap_host_audio_ports callbacks ─────────────────────────────────

bool host_audio_ports_is_reserved(const clap_host* /*host*/, clap_id /*port_id*/) {
    return false;  // We don't reserve any port IDs
}

// ── clap_host_params callbacks ──────────────────────────────────────

void host_params_rescan(const clap_host* /*host*/, uint32_t /*flags*/) {
    // The plugin wants us to rescan parameter info.
    // For v1, we accept the notification and will handle it in subsequent rescans.
}

void host_params_request_flush(const clap_host* /*host*/) {
    // The plugin has pending parameter changes that need to be flushed.
    // In a real-time context, this would trigger a flush on the audio thread.
}

// ── clap_host_mpe callbacks ─────────────────────────────────────────

void host_mpe_changed(const clap_host* /*host*/) {
    // Plugin's MPE mode changed. Log / forward as needed.
}

// ── Extension getter ────────────────────────────────────────────────

bool host_get_extension(const clap_host* host,
                        const char* extension_id,
                        const void** extension) {
    if (!host || !extension_id || !extension)
        return false;

    auto* self = from_host(host);
    if (!self) return false;

    // Host extensions
    if (std::strcmp(extension_id, CLAP_EXT_AUDIO_PORTS) == 0) {
        *extension = &self->host_audio_ports_;
        return true;
    }
    if (std::strcmp(extension_id, CLAP_EXT_PARAMS) == 0) {
        *extension = &self->host_params_;
        return true;
    }
    if (std::strcmp(extension_id, CLAP_EXT_MPE) == 0) {
        *extension = &self->host_mpe_;
        return true;
    }

    *extension = nullptr;
    return false;
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════
//  CLAPHost — Implementation
// ══════════════════════════════════════════════════════════════════════

CLAPHost::CLAPHost(std::string plugin_path, PluginID id)
    : plugin_path_(std::move(plugin_path))
    , id_(std::move(id)) {
}

CLAPHost::~CLAPHost() {
    deactivate();

    if (plugin_) {
        plugin_->destroy(plugin_);
        plugin_ = nullptr;
    }

    if (entry_) {
        entry_->deinit();
        entry_ = nullptr;
    }

    unload_library();
}

// ── Host initialisation ─────────────────────────────────────────────

void CLAPHost::init_host() {
    if (host_initialized_) return;

    static const clap_host_descriptor s_host_desc = {
        "1.2.0",  // clap_version — CLAP API version string
        "ARIA DAW",
        "ARIA",
        "https://aria-daw.dev",
        "0.1.0"
    };

    host_.desc = &s_host_desc;
    host_.host_data = this;
    host_.get_extension = host_get_extension;
    host_.request_restart = nullptr;   // Not yet implemented
    host_.request_process = nullptr;   // Not yet implemented
    host_.request_callback = nullptr;  // Not yet implemented

    host_initialized_ = true;
}

void CLAPHost::init_extensions() {
    // Host audio ports
    host_audio_ports_.is_reserved = host_audio_ports_is_reserved;

    // Host params
    host_params_.rescan = host_params_rescan;
    host_params_.request_flush = host_params_request_flush;

    // Host MPE
    host_mpe_.changed = host_mpe_changed;
}

// ── Library loading ─────────────────────────────────────────────────

bool CLAPHost::load_library() {
    library_handle_ = dl_open(plugin_path_.c_str());
    if (!library_handle_) {
        return false;
    }

    // Get the CLAP entry point
    using GetEntryFn = const clap_plugin_entry* (*)();
    auto get_entry = reinterpret_cast<GetEntryFn>(
        dl_sym(library_handle_, "clap_entry"));

    if (!get_entry) {
        dl_close(library_handle_);
        library_handle_ = nullptr;
        return false;
    }

    entry_ = get_entry();
    if (!entry_) {
        dl_close(library_handle_);
        library_handle_ = nullptr;
        return false;
    }

    return true;
}

void CLAPHost::unload_library() {
    if (library_handle_) {
        dl_close(library_handle_);
        library_handle_ = nullptr;
    }
}

// ── Lifecycle ──────────────────────────────────────────────────────

bool CLAPHost::init() {
    if (plugin_) return true;

    init_host();
    init_extensions();

    if (!load_library()) {
        return false;
    }

    // Initialise the plugin entry
    if (!entry_->init(plugin_path_.c_str())) {
        unload_library();
        entry_ = nullptr;
        return false;
    }

    // Find our plugin in the factory
    const char* target_id = id_.c_str();
    const clap_plugin_descriptor* found_desc = nullptr;
    uint32_t count = entry_->get_plugin_count();
    for (uint32_t i = 0; i < count; ++i) {
        auto* desc = entry_->get_plugin_descriptor(i);
        if (desc && std::strcmp(desc->id, target_id) == 0) {
            found_desc = desc;
            break;
        }
    }

    if (!found_desc) {
        entry_->deinit();
        unload_library();
        entry_ = nullptr;
        return false;
    }

    // Create the plugin instance
    plugin_ = entry_->create_plugin(&host_, target_id);
    if (!plugin_) {
        entry_->deinit();
        unload_library();
        entry_ = nullptr;
        return false;
    }

    // Initialise the plugin
    if (!plugin_->init(plugin_)) {
        plugin_->destroy(plugin_);
        plugin_ = nullptr;
        entry_->deinit();
        unload_library();
        entry_ = nullptr;
        return false;
    }

    // Read metadata from the descriptor
    name_ = found_desc->name ? found_desc->name : "";
    vendor_ = found_desc->vendor ? found_desc->vendor : "";
    category_ = category_from_clap_features(found_desc->features);

    // Cache plugin extensions
    // In CLAP v1.2, plugin extensions are queried through the plugin's
    // factory or by checking descriptor features. The clap_plugin struct
    // does not have a standard get_extension mechanism — extensions like
    // clap_plugin_params are accessed via their dedicated vtable structs
    // returned from the plugin's factory entry. Extension caching will
    // be completed when the specific extension API surfaces are wired.

    return true;
}

bool CLAPHost::activate(double sample_rate,
                        uint32_t min_frames,
                        uint32_t max_frames) {
    if (!plugin_) return false;

    std::lock_guard<std::mutex> lock(process_mutex_);

    if (!plugin_->activate(plugin_, sample_rate, min_frames, max_frames)) {
        return false;
    }

    sample_rate_ = sample_rate;
    min_frames_ = min_frames;
    max_frames_ = max_frames;
    activated_ = true;

    // Query latency extension
    {
        const void* ext = nullptr;
        // In a full CLAP implementation, we would use the plugin's
        // extension query mechanism. For now we cache null and
        // return 0 latency if the extension is unavailable.
    }

    return true;
}

void CLAPHost::deactivate() {
    if (!activated_) return;

    std::lock_guard<std::mutex> lock(process_mutex_);

    if (plugin_) {
        plugin_->deactivate(plugin_);
    }

    activated_ = false;
}

// ── Identification ──────────────────────────────────────────────────

PluginID CLAPHost::id() const { return id_; }
std::string CLAPHost::name() const { return name_; }
std::string CLAPHost::vendor() const { return vendor_; }
std::string CLAPHost::format() const { return "CLAP"; }
PluginCategory CLAPHost::category() const { return category_; }

// ── Audio processing ───────────────────────────────────────────────

void CLAPHost::process(const ProcessContext& ctx,
                       const float* const* inputs,
                       float* const* outputs) {
    if (!plugin_ || !activated_) {
        // Bypass: copy inputs to outputs (handled by caller/PluginAudioNode)
        return;
    }

    std::lock_guard<std::mutex> lock(process_mutex_);

    // Build temporary clap_audio_buffers
    clap_audio_buffer clap_in;
    clap_in.data32 = const_cast<float**>(inputs);
    clap_in.data64 = nullptr;
    clap_in.channel_count = ctx.num_inputs;
    clap_in.latency = 0;

    clap_audio_buffer clap_out;
    clap_out.data32 = const_cast<float**>(outputs);
    clap_out.data64 = nullptr;
    clap_out.channel_count = ctx.num_outputs;
    clap_out.latency = 0;

    // Build the clap_process struct
    clap_process process;
    process.audio_input = ctx.num_inputs > 0 ? &clap_in : nullptr;
    process.audio_output = &clap_out;
    process.frames_count = ctx.num_frames;
    process.steady_time = ctx.frame_offset;
    process.transport = 0;        // Transport info not yet wired
    process.in_events = nullptr;  // MIDI events not yet wired
    process.out_events = nullptr;

    plugin_->process(plugin_, &process);
}

// ── Parameters ─────────────────────────────────────────────────────

uint32_t CLAPHost::parameter_count() const {
    if (!plugin_params_) return 0;
    uint32_t count = 0;
    plugin_params_->count(plugin_, &count);
    return count;
}

ParameterInfo CLAPHost::parameter_info(ParamID id) const {
    ParameterInfo info;
    if (!plugin_params_) return info;

    clap_param_info clap_info;
    if (plugin_params_->get_info(plugin_, id, &clap_info)) {
        info.id = static_cast<ParamID>(clap_info.id);
        info.name = clap_info.name ? clap_info.name : "";
        info.min_value = clap_info.min_value;
        info.max_value = clap_info.max_value;
        info.default_value = clap_info.default_value;
    }
    return info;
}

double CLAPHost::get_parameter_value(ParamID id) const {
    if (!plugin_params_) return 0.0;
    double value = 0.0;
    plugin_params_->get_value(plugin_, id, &value);
    return value;
}

void CLAPHost::set_parameter_value(ParamID id, double value) {
    // In CLAP, parameter changes during process are handled via events.
    // For non-realtime changes, we create a flush event.
    if (!plugin_params_) return;
    plugin_params_->flush(plugin_, nullptr, nullptr);
}

void CLAPHost::begin_edit(ParamID /*id*/) {
    // CLAP plugins manage their own edit state.
    // We may notify via host extension in the future.
}

void CLAPHost::end_edit(ParamID /*id*/) {
    // CLAP plugins manage their own edit state.
}

// ── State ───────────────────────────────────────────────────────────

bool CLAPHost::save_state(std::vector<uint8_t>& data) const {
    if (!plugin_state_) return false;

    // Use clap_ostream backed by the vector
    struct VectorStream : clap_ostream {
        std::vector<uint8_t>* vec;
    };

    VectorStream stream;
    stream.ctx = &data;
    stream.write = [](const clap_ostream* s, const void* buffer, uint64_t size) -> bool {
        auto* vec = static_cast<VectorStream*>(const_cast<clap_ostream*>(s))->vec;
        const auto* bytes = static_cast<const uint8_t*>(buffer);
        vec->insert(vec->end(), bytes, bytes + size);
        return true;
    };

    return plugin_state_->save(plugin_, &stream);
}

bool CLAPHost::load_state(const std::vector<uint8_t>& data) {
    if (!plugin_state_) return false;

    // Use clap_istream backed by the vector
    struct VectorStream : clap_istream {
        const std::vector<uint8_t>* vec;
        uint64_t pos = 0;
    };

    VectorStream stream;
    stream.ctx = const_cast<std::vector<uint8_t>*>(&data);
    stream.read = [](const clap_istream* s, void* buffer, uint64_t size) -> bool {
        auto* st = const_cast<VectorStream*>(static_cast<const VectorStream*>(s));
        if (st->pos + size > st->vec->size()) return false;
        std::memcpy(buffer, st->vec->data() + st->pos, static_cast<size_t>(size));
        st->pos += size;
        return true;
    };

    return plugin_state_->load(plugin_, &stream);
}

// ── Latency ─────────────────────────────────────────────────────────

uint32_t CLAPHost::latency_samples() const {
    if (!plugin_latency_) return 0;
    return plugin_latency_->get(plugin_);
}

} // namespace aria::plugin