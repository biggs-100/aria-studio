#ifndef ARIA_PLUGIN_HOST_H
#define ARIA_PLUGIN_HOST_H

#include "audio_plugin_types.h"
#include "plugin_blacklist.h"
#include "plugin_factory.h"
#include "plugin_sandbox.h"
#include "plugin_scanner.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria {

/// Plugin Host — CLAP-native plugin loader with VST3/AU/LV2 support.
///
/// Facade that wires together the plugin factory, scanner, sandbox
/// instances, and blacklist. Sub-components are allocated during
/// init() and destroyed during shutdown().
///
/// The host owns the scanner, factory, sandbox pool (for standalone
/// testing), and blacklist. Created plugin instances are wrapped in
/// PluginSandbox objects for watchdog isolation.
class PluginHost {
public:
    PluginHost();
    ~PluginHost();

    // ── Lifecycle ──────────────────────────────────────────────

    /// Initialize all sub-components. Returns true on success.
    bool init();

    /// Shut down all sub-components and release resources.
    void shutdown();

    // ── Scan ───────────────────────────────────────────────────

    struct PluginInfo {
        std::string id;
        std::string name;
        std::string vendor;
        std::string format;
    };

    /// Run a plugin scan across default platform paths.
    /// Returns a list of discovered plugin infos.
    std::vector<PluginInfo> scan_plugins();

    /// Run a plugin scan across custom directories.
    /// @param paths  Directories to scan for plugins.
    std::vector<PluginInfo> scan_paths(const std::vector<std::string>& paths);

    /// Number of uniquely registered plugin types.
    uint32_t plugin_count() const;

    // ── Plugin Instance Management ─────────────────────────────

    /// Create a plugin instance by ID and wrap it in a sandbox.
    /// Returns nullptr if the ID is unknown, blacklisted, or creation fails.
    /// The sandbox is NOT started — caller must call sandbox->start().
    plugin::PluginSandbox* create_plugin_instance(const plugin::PluginID& id);

    /// Destroy a plugin instance that was previously created with
    /// create_plugin_instance(). Returns true if the instance was found.
    bool destroy_plugin_instance(plugin::PluginSandbox* sandbox);

    /// Synchronously process a plugin instance through its sandbox.
    /// Convenience wrapper around sandbox->process_async() + wait_for_result().
    /// @return true if processing completed within the timeout.
    bool process_plugin(plugin::PluginSandbox* sandbox,
                        const plugin::ProcessContext& ctx,
                        const float* const* inputs,
                        float* const* outputs,
                        std::chrono::milliseconds timeout);

    // ── Sub-component accessors ────────────────────────────────

    plugin::PluginFactory& factory();
    plugin::PluginScanner& scanner();
    plugin::PluginSandbox& sandbox();   // default sandbox (for testing)
    plugin::PluginBlacklist& blacklist();

private:
    bool initialized_ = false;

    // Sub-components; allocated in init(), destroyed in shutdown()
    plugin::PluginFactory* factory_ = nullptr;
    std::unique_ptr<plugin::PluginScanner> scanner_;
    std::unique_ptr<plugin::PluginSandbox> sandbox_;
    std::unique_ptr<plugin::PluginBlacklist> blacklist_;

    // Format scanners registered at init()
    std::vector<std::unique_ptr<plugin::FormatScanner>> format_scanners_;

    // Active plugin instances (sandboxes created via create_plugin_instance)
    std::vector<std::unique_ptr<plugin::PluginSandbox>> instances_;
};

} // namespace aria

#endif // ARIA_PLUGIN_HOST_H
