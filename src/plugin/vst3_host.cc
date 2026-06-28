#include "vst3_host.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// ── VST3 SDK stubs (guarded) ────────────────────────────────────────

#if defined(ARIA_FEATURE_VST3)
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/base/fuid_defs.h>
#include <pluginterfaces/base/ipluginfactory.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstunits.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivststream.h>
#endif

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
//  Platform helpers (shared with clap_host)
// ══════════════════════════════════════════════════════════════════════

namespace {

void* vst3_open(const char* path) {
#if defined(_WIN32)
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

void* vst3_sym(void* handle, const char* symbol) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(
        static_cast<HMODULE>(handle), symbol));
#else
    return dlsym(handle, symbol);
#endif
}

void vst3_close(void* handle) {
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════
//  VST3Host — Implementation
// ══════════════════════════════════════════════════════════════════════

VST3Host::VST3Host(std::string module_path, PluginID id)
    : module_path_(std::move(module_path))
    , id_(std::move(id)) {
}

VST3Host::~VST3Host() {
    deactivate();
#if defined(ARIA_FEATURE_VST3)
    unload_module();
#endif
}

// ── Lifecycle ──────────────────────────────────────────────────────

bool VST3Host::init() {
#if !defined(ARIA_FEATURE_VST3)
    return false;  // VST3 support not compiled in
#else
    if (!load_module()) {
        return false;
    }
    return true;
#endif
}

bool VST3Host::activate(double sample_rate,
                        uint32_t min_frames,
                        uint32_t max_frames) {
#if !defined(ARIA_FEATURE_VST3)
    return false;
#else
    if (!component_) return false;

    std::lock_guard<std::mutex> lock(process_mutex_);

    // Configure bus arrangements
    int32_t input_bus_count = component_->getBusCount(
        Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
    int32_t output_bus_count = component_->getBusCount(
        Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);

    io_config_.num_input_busses = static_cast<uint32_t>(input_bus_count);
    io_config_.num_output_busses = static_cast<uint32_t>(output_bus_count);

    // Activate all audio busses
    for (int32_t i = 0; i < input_bus_count; ++i) {
        component_->activateBus(Steinberg::Vst::kAudio,
                                Steinberg::Vst::kInput, i, 1);
        Steinberg::Vst::BusInfo bus_info;
        if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                   Steinberg::Vst::kInput, i, bus_info) ==
            Steinberg::kResultOk) {
            io_config_.total_input_channels +=
                static_cast<uint32_t>(bus_info.channel_count);
        }
    }

    for (int32_t i = 0; i < output_bus_count; ++i) {
        component_->activateBus(Steinberg::Vst::kAudio,
                                Steinberg::Vst::kOutput, i, 1);
        Steinberg::Vst::BusInfo bus_info;
        if (component_->getBusInfo(Steinberg::Vst::kAudio,
                                   Steinberg::Vst::kOutput, i, &bus_info) ==
            Steinberg::kResultOk) {
            io_config_.total_output_channels +=
                static_cast<uint32_t>(bus_info.channel_count);
        }
    }

    // Set up processing
    if (processor_) {
        Steinberg::Vst::ProcessSetup setup;
        setup.process_mode = Steinberg::Vst::kRealtime;
        setup.symbolic_sample_size = 32;  // 32-bit floats
        setup.max_audio_buffers = std::max(input_bus_count, output_bus_count);
        setup.sample_rate = sample_rate;

        if (processor_->setupProcessing(setup) != Steinberg::kResultOk) {
            return false;
        }

        // Prepare internal buffer arrays
        input_buffers_.resize(io_config_.total_input_channels, nullptr);
        output_buffers_.resize(io_config_.total_output_channels, nullptr);
    }

    component_->setActive(true);

    sample_rate_ = sample_rate;
    min_frames_ = min_frames;
    max_frames_ = max_frames;
    activated_ = true;

    return true;
#endif
}

void VST3Host::deactivate() {
#if defined(ARIA_FEATURE_VST3)
    if (!activated_) return;

    std::lock_guard<std::mutex> lock(process_mutex_);

    if (component_) {
        component_->setActive(false);
    }

    activated_ = false;
    input_buffers_.clear();
    output_buffers_.clear();
#endif
}

// ── Identification ──────────────────────────────────────────────────

PluginID VST3Host::id() const { return id_; }
std::string VST3Host::name() const { return name_; }
std::string VST3Host::vendor() const { return vendor_; }
std::string VST3Host::format() const { return "VST3"; }
PluginCategory VST3Host::category() const { return category_; }

// ── Parameters ─────────────────────────────────────────────────────

uint32_t VST3Host::parameter_count() const {
#if !defined(ARIA_FEATURE_VST3)
    return 0;
#else
    if (!controller_) return 0;
    return static_cast<uint32_t>(controller_->getParameterCount());
#endif
}

ParameterInfo VST3Host::parameter_info(ParamID id) const {
#if !defined(ARIA_FEATURE_VST3)
    return ParameterInfo{};
#else
    ParameterInfo info;
    if (!controller_) return info;

    Steinberg::Vst::ParameterInfo vst3_info;
    if (controller_->getParameterInfo(static_cast<int32_t>(id), vst3_info) ==
        Steinberg::kResultOk) {
        info.id = id;
        // Convert UTF-16 to narrow (simplified: just use first bytes)
        info.name = "";
        for (int i = 0; i < 64 && vst3_info.title[i] != 0; ++i) {
            info.name += static_cast<char>(vst3_info.title[i] & 0xFF);
        }
        info.min_value = 0.0;
        info.max_value = 1.0;
        info.default_value = vst3_info.default_normalized_value;
    }
    return info;
#endif
}

double VST3Host::get_parameter_value(ParamID id) const {
#if !defined(ARIA_FEATURE_VST3)
    return 0.0;
#else
    if (!controller_) return 0.0;
    double value = 0.0;
    controller_->getParamNormalized(static_cast<int32_t>(id), value);
    return value;
#endif
}

void VST3Host::set_parameter_value(ParamID id, double value) {
#if defined(ARIA_FEATURE_VST3)
    if (!controller_) return;
    controller_->setParamNormalized(static_cast<int32_t>(id), value);
#endif
}

void VST3Host::begin_edit(ParamID /*id*/) {
    // VST3 manages edit state internally
}

void VST3Host::end_edit(ParamID /*id*/) {
    // VST3 manages edit state internally
}

// ── Audio processing ───────────────────────────────────────────────

void VST3Host::process(const ProcessContext& ctx,
                       const float* const* inputs,
                       float* const* outputs) {
#if !defined(ARIA_FEATURE_VST3)
    return;
#else
    if (!processor_ || !activated_) return;

    std::lock_guard<std::mutex> lock(process_mutex_);

    // Build VST3 ProcessData
    Steinberg::Vst::ProcessData data;
    data.process_mode = Steinberg::Vst::kRealtime;
    data.symbolic_sample_size = 32;
    data.num_inputs = static_cast<int32_t>(io_config_.num_input_busses);
    data.num_outputs = static_cast<int32_t>(io_config_.num_output_busses);
    data.sample_rate = ctx.sample_rate;
    data.continous_sample_pos = ctx.frame_offset;
    data.num_samples = static_cast<int32_t>(ctx.num_frames);

    std::memset(&data, 0, sizeof(data));
    data.sample_rate = ctx.sample_rate;
    data.num_samples = static_cast<int32_t>(ctx.num_frames);

    processor_->process(data);
#endif
}

// ── State ───────────────────────────────────────────────────────────

bool VST3Host::save_state(std::vector<uint8_t>& data) const {
#if !defined(ARIA_FEATURE_VST3)
    return false;
#else
    if (!component_) return false;
    // VST3 state is saved via IBStream. The component state includes
    // both component and controller state when they are linked.
    // Full IBStream implementation requires the VST3 SDK.
    return false;  // TODO: implement IBStream wrapper
#endif
}

bool VST3Host::load_state(const std::vector<uint8_t>& data) {
#if !defined(ARIA_FEATURE_VST3)
    return false;
#else
    if (!component_) return false;
    return false;  // TODO: implement IBStream wrapper
#endif
}

// ── Latency ─────────────────────────────────────────────────────────

uint32_t VST3Host::latency_samples() const {
#if !defined(ARIA_FEATURE_VST3)
    return 0;
#else
    if (!processor_) return 0;
    return processor_->getLatencySamples();
#endif
}

#if defined(ARIA_FEATURE_VST3)
// ── Module loading ─────────────────────────────────────────────────

bool VST3Host::load_module() {
    // Open the .vst3 module
    module_handle_ = vst3_open(module_path_.c_str());
    if (!module_handle_) {
        return false;
    }

    // Get the exported factory function
    using GetFactoryProc = Steinberg::IPluginFactory* (*)();
    auto get_factory = reinterpret_cast<GetFactoryProc>(
        vst3_sym(module_handle_, "GetPluginFactory"));

    if (!get_factory) {
        vst3_close(module_handle_);
        module_handle_ = nullptr;
        return false;
    }

    factory_ = get_factory();
    if (!factory_) {
        vst3_close(module_handle_);
        module_handle_ = nullptr;
        return false;
    }

    // Enumerate classes to find our plugin
    int32_t count = factory_->countClasses();
    for (int32_t i = 0; i < count; ++i) {
        Steinberg::PClassInfo class_info;
        if (factory_->getClassInfo(i, &class_info) != Steinberg::kResultOk) {
            continue;
        }

        // Check if this is the plugin we're looking for
        // Match by class ID or by category
        // Build a class_id string from TUID for comparison
        std::string class_id_str;
        for (int j = 0; j < 16; ++j) {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02x",
                         reinterpret_cast<const uint8_t*>(class_info.cid)[j]);
            class_id_str += hex;
        }

        if (class_id_str != id_) {
            continue;  // Not our plugin class
        }

        // Create the component
        auto* component = factory_->createInstance(
            class_info.cid,
            Steinberg::Vst::IComponent::iid);
        if (!component) {
            continue;
        }

        component_ = static_cast<Steinberg::Vst::IComponent*>(component);
        if (component_->initialize(nullptr) != Steinberg::kResultOk) {
            component_->release();
            component_ = nullptr;
            continue;
        }

        // Get the controller class ID from the component
        Steinberg::TUID controller_cid;
        if (component_->getControllerClassId(&controller_cid) ==
            Steinberg::kResultOk) {
            // Create the edit controller
            auto* controller =
                factory_->createInstance(controller_cid,
                                         Steinberg::Vst::IEditController::iid);
            if (controller) {
                controller_ =
                    static_cast<Steinberg::Vst::IEditController*>(controller);
                controller_->initialize(nullptr);
                // Link component state to controller
                controller_->setComponentState(nullptr);
            }
        }

        // Get the audio processor interface
        processor_ = component_->queryInterface<Steinberg::Vst::IAudioProcessor>();

        // Read metadata
        // For now, use the PClassInfo name
        name_ = "";
        for (int c = 0; c < 64 && class_info.name_[c] != 0; ++c) {
            name_ += static_cast<char>(class_info.name_[c] & 0xFF);
        }

        break;
    }

    if (!component_) {
        vst3_close(module_handle_);
        module_handle_ = nullptr;
        return false;
    }

    return true;
}

void VST3Host::unload_module() {
    if (controller_) {
        controller_->terminate();
        controller_->release();
        controller_ = nullptr;
    }

    if (component_) {
        component_->terminate();
        component_->release();
        component_ = nullptr;
    }

    factory_ = nullptr;

    if (module_handle_) {
        vst3_close(module_handle_);
        module_handle_ = nullptr;
    }
}

VST3Host::AudioIOConfig VST3Host::audio_io_config() const {
    return io_config_;
}

#endif // ARIA_FEATURE_VST3

} // namespace aria::plugin