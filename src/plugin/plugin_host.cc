#include "plugin_host.h"
#include "clap_scanner.h"
#include "native_clap_factory.h"
#include "plugin_scanner_defaults.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace aria {

// ══════════════════════════════════════════════════════════════════════
//  Construction / Destruction
// ══════════════════════════════════════════════════════════════════════

PluginHost::PluginHost() = default;
PluginHost::~PluginHost() { shutdown(); }

// ══════════════════════════════════════════════════════════════════════
//  Lifecycle
// ══════════════════════════════════════════════════════════════════════

bool PluginHost::init() {
    if (initialized_) return true;

    // Allocate sub-components (use singleton factory so all code shares the registry)
    factory_ = &plugin::PluginFactory::instance();
    scanner_ = std::make_unique<plugin::PluginScanner>();
    blacklist_ = std::make_unique<plugin::PluginBlacklist>();

    // Register format scanners
    format_scanners_.push_back(std::make_unique<plugin::CLAPScanner>());

    // Register native plugins in the factory
    register_native_plugins(*factory_);

    // Set up scanner cache path
    scanner_->set_cache_path("~/.aria/plugin_cache.json");

    initialized_ = true;
    return true;
}

void PluginHost::shutdown() {
    // Destroy all active plugin instances
    instances_.clear();
    format_scanners_.clear();
    blacklist_.reset();
    scanner_.reset();
    factory_ = nullptr;  // singleton, not owned
    initialized_ = false;
}

// ══════════════════════════════════════════════════════════════════════
//  Scan
// ══════════════════════════════════════════════════════════════════════

std::vector<PluginHost::PluginInfo> PluginHost::scan_plugins() {
    auto paths = plugin::default_scan_paths();
    return scan_paths(paths);
}

std::vector<PluginHost::PluginInfo> PluginHost::scan_paths(
    const std::vector<std::string>& paths)
{
    if (!initialized_) return {};

    // Build scanner list for the scan call (temporary owning wrappers).
    // The scan() takes const-ref to unique_ptr vector; it only reads the
    // pointees. We release() after the call so our originals survive.
    std::vector<std::unique_ptr<plugin::FormatScanner>> scanners;
    for (auto& s : format_scanners_) {
        scanners.push_back(std::unique_ptr<plugin::FormatScanner>(s.get()));
    }

    // Run the scan
    auto result = scanner_->scan(paths, scanners);

    // Release all scanners so they aren't deleted — we still own them
    for (auto& s : scanners) {
        (void)s.release();
    }

    // Build PluginInfo list from the scan results
    std::vector<PluginInfo> infos;
    infos.reserve(result.all.size());
    for (const auto& desc : result.all) {
        PluginInfo info;
        info.id = desc.id;
        info.name = desc.name;
        info.vendor = desc.vendor;
        info.format = desc.format;
        infos.push_back(std::move(info));
    }

    return infos;
}

uint32_t PluginHost::plugin_count() const {
    if (!factory_) return 0;
    return static_cast<uint32_t>(factory_->count());
}

// ══════════════════════════════════════════════════════════════════════
//  Plugin Instance Management
// ══════════════════════════════════════════════════════════════════════

plugin::PluginSandbox* PluginHost::create_plugin_instance(
    const plugin::PluginID& id)
{
    if (!initialized_ || !factory_) return nullptr;

    // Check blacklist
    if (blacklist_ && blacklist_->is_blacklisted(id)) {
        return nullptr;
    }

    // Try to create the plugin
    auto plugin = factory_->create(id);
    if (!plugin) {
        return nullptr;
    }

    // Wrap in sandbox
    auto sandbox = std::make_unique<plugin::PluginSandbox>(std::move(plugin));
    auto* raw = sandbox.get();

    instances_.push_back(std::move(sandbox));
    return raw;
}

bool PluginHost::destroy_plugin_instance(plugin::PluginSandbox* sandbox) {
    if (!sandbox) return false;

    auto it = std::find_if(instances_.begin(), instances_.end(),
        [sandbox](const auto& ptr) { return ptr.get() == sandbox; });

    if (it == instances_.end()) return false;

    // Stop the sandbox before destroying
    (*it)->stop();
    instances_.erase(it);
    return true;
}

bool PluginHost::process_plugin(plugin::PluginSandbox* sandbox,
                                const plugin::ProcessContext& ctx,
                                const float* const* inputs,
                                float* const* outputs,
                                std::chrono::milliseconds timeout)
{
    if (!sandbox) return false;

    // Ensure sandbox is started
    if (sandbox->state() == plugin::SandboxState::Idle ||
        sandbox->state() == plugin::SandboxState::Terminated) {
        if (!sandbox->start()) return false;
    }

    if (!sandbox->process_async(ctx, inputs, outputs)) {
        return false;
    }

    return sandbox->wait_for_result(timeout);
}

// ══════════════════════════════════════════════════════════════════════
//  Sub-component accessors
// ══════════════════════════════════════════════════════════════════════

plugin::PluginFactory& PluginHost::factory() {
    if (!factory_) {
        throw std::logic_error("PluginFactory not initialized — call init() first");
    }
    return *factory_;
}

plugin::PluginScanner& PluginHost::scanner() {
    if (!scanner_) {
        throw std::logic_error("PluginScanner not initialized — call init() first");
    }
    return *scanner_;
}

plugin::PluginSandbox& PluginHost::sandbox() {
    // No default sandbox — use create_plugin_instance() instead
    throw std::logic_error(
        "PluginHost::sandbox() is not available — use create_plugin_instance()");
}

plugin::PluginBlacklist& PluginHost::blacklist() {
    if (!blacklist_) {
        throw std::logic_error("PluginBlacklist not initialized — call init() first");
    }
    return *blacklist_;
}

} // namespace aria
