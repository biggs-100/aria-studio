#include "clap_scanner.h"

#include <clap/clap.h>

#include <cstring>
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
//  CLAPScanner — Implementation
// ══════════════════════════════════════════════════════════════════════

std::string CLAPScanner::name() const {
    return "CLAP";
}

std::string CLAPScanner::extension() const {
    return ".clap";
}

bool CLAPScanner::verify(const std::string& path) {
    auto* handle = dl_open(path.c_str());
    if (!handle) return false;

    auto get_entry = reinterpret_cast<const clap_plugin_entry* (*)()>(
        dl_sym(handle, "clap_entry"));

    bool ok = (get_entry != nullptr);
    dl_close(handle);
    return ok;
}

std::vector<PluginDescriptor> CLAPScanner::scan(const std::string& path) {
    auto* handle = dl_open(path.c_str());
    if (!handle) return {};

    auto get_entry = reinterpret_cast<const clap_plugin_entry* (*)()>(
        dl_sym(handle, "clap_entry"));

    if (!get_entry) {
        dl_close(handle);
        return {};
    }

    const clap_plugin_entry* entry = get_entry();
    if (!entry || !entry->init(path.c_str())) {
        dl_close(handle);
        return {};
    }

    uint32_t count = entry->get_plugin_count();
    std::vector<PluginDescriptor> results;
    results.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        auto* desc = entry->get_plugin_descriptor(i);
        if (!desc) continue;

        PluginDescriptor pd;
        pd.id = desc->id ? desc->id : "";
        pd.name = desc->name ? desc->name : "";
        pd.vendor = desc->vendor ? desc->vendor : "";
        pd.version = desc->version ? desc->version : "";
        pd.format = "CLAP";
        pd.path = path;
        pd.category = category_from_clap_features(desc->features);

        // Parse features for more info
        if (desc->features) {
            std::string_view features(desc->features);
            pd.is_synth = (features.find("instrument") != std::string_view::npos);

            // Split comma-separated features into tags
            const char* start = desc->features;
            const char* end = start;
            while (*end) {
                if (*end == ',') {
                    if (end > start) {
                        pd.features.emplace_back(start, end - start);
                    }
                    start = end + 1;
                }
                ++end;
            }
            if (end > start) {
                pd.features.emplace_back(start, end - start);
            }
        }

        pd.has_editor = false;
        pd.last_modified = 0;

        results.push_back(std::move(pd));
    }

    entry->deinit();
    dl_close(handle);

    return results;
}

} // namespace aria::plugin