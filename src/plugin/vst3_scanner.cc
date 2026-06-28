#include "vst3_scanner.h"

#include <cstdio>
#include <cstring>
#include <string>
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

// ── VST3 SDK stubs (guarded) ────────────────────────────────────────
// These are only included when ARIA_FEATURE_VST3 is defined.
// The stubs live in vendor/vst3sdk/ and provide the minimal interfaces.

#if defined(ARIA_FEATURE_VST3)
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/base/fuid_defs.h>
#include <pluginterfaces/base/ipluginfactory.h>
#endif

namespace aria::plugin {

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
//  VST3Scanner — Implementation
// ══════════════════════════════════════════════════════════════════════

std::string VST3Scanner::name() const {
    return "VST3";
}

std::string VST3Scanner::extension() const {
    return ".vst3";
}

bool VST3Scanner::verify(const std::string& path) {
#if !defined(ARIA_FEATURE_VST3)
    return false;
#else
    auto* handle = vst3_open(path.c_str());
    if (!handle) return false;

    using GetFactoryProc = Steinberg::IPluginFactory* (*)();
    auto get_factory = reinterpret_cast<GetFactoryProc>(
        vst3_sym(handle, "GetPluginFactory"));

    bool ok = (get_factory != nullptr);
    vst3_close(handle);
    return ok;
#endif
}

std::vector<PluginDescriptor> VST3Scanner::scan(const std::string& path) {
#if !defined(ARIA_FEATURE_VST3)
    return {};
#else
    auto* handle = vst3_open(path.c_str());
    if (!handle) return {};

    using GetFactoryProc = Steinberg::IPluginFactory* (*)();
    auto get_factory = reinterpret_cast<GetFactoryProc>(
        vst3_sym(handle, "GetPluginFactory"));

    if (!get_factory) {
        vst3_close(handle);
        return {};
    }

    auto* factory = get_factory();
    if (!factory) {
        vst3_close(handle);
        return {};
    }

    int32_t count = factory->countClasses();
    std::vector<PluginDescriptor> results;
    results.reserve(static_cast<size_t>(count));

    for (int32_t i = 0; i < count; ++i) {
        Steinberg::PClassInfo class_info;
        if (factory->getClassInfo(i, &class_info) != Steinberg::kResultOk) {
            continue;
        }

        // Build a hex ID from the TUID
        std::string plugin_id;
        for (int j = 0; j < 16; ++j) {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02x",
                         reinterpret_cast<const uint8_t*>(class_info.cid)[j]);
            plugin_id += hex;
        }

        PluginDescriptor pd;
        pd.id = plugin_id;
        pd.name = "";
        for (int c = 0; c < 64 && class_info.name_[c] != 0; ++c) {
            pd.name += static_cast<char>(class_info.name_[c] & 0xFF);
        }
        pd.vendor = "";
        for (int c = 0; c < 64 && class_info.vendor_[c] != 0; ++c) {
            pd.vendor += static_cast<char>(class_info.vendor_[c] & 0xFF);
        }
        pd.version = "";
        pd.format = "VST3";
        pd.path = path;
        pd.category = PluginCategory::Effect;
        pd.last_modified = 0;

        results.push_back(std::move(pd));
    }

    vst3_close(handle);
    return results;
#endif
}

} // namespace aria::plugin