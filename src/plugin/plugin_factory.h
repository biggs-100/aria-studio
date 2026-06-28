#ifndef ARIA_PLUGIN_PLUGIN_FACTORY_H
#define ARIA_PLUGIN_PLUGIN_FACTORY_H

#include "audio_plugin.h"
#include "audio_plugin_types.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria::plugin {

/// Singleton registry for creating AudioPlugin instances by ID.
///
/// Format scanners register creators at startup. Native CLAP plugins
/// are registered via NativeCLAPFactory. The factory supports
/// category-based filtering and case-insensitive text search.
class PluginFactory {
public:
    /// Access the singleton instance.
    static PluginFactory& instance();

    PluginFactory(const PluginFactory&) = delete;
    PluginFactory& operator=(const PluginFactory&) = delete;

    // ── Registration ───────────────────────────────────────────

    /// Register a plugin creator with metadata.
    /// @param id        Unique PluginID (e.g. "aria.eq")
    /// @param factory   Factory function that creates a new AudioPlugin
    /// @param name      Human-readable display name
    /// @param vendor    Developer / company name
    /// @param category  PluginCategory classification
    void register_plugin(PluginID id,
                         std::function<std::unique_ptr<AudioPlugin>()> factory,
                         std::string name,
                         std::string vendor,
                         PluginCategory category);

    // ── Instantiation ──────────────────────────────────────────

    /// Create a plugin instance by ID.
    /// Returns nullptr if the ID is not registered or if creation fails.
    std::unique_ptr<AudioPlugin> create(const PluginID& id) const;

    // ── Queries ────────────────────────────────────────────────

    /// Total number of registered plugins.
    size_t count() const;

    /// Query a registered plugin's category.
    PluginCategory category(const PluginID& id) const;

    /// Check if a plugin ID is registered.
    bool has(const PluginID& id) const;

    /// Get all registered plugin IDs.
    std::vector<PluginID> plugin_ids() const;

    /// Search plugins by text query (matches name, vendor, and ID).
    /// @param query    Case-insensitive search string
    /// @param category Optional category filter (PluginCategory::Other
    ///                 means no filter)
    /// @return Matching PluginIDs
    std::vector<PluginID> search(const std::string& query,
                                 PluginCategory category) const;

    PluginFactory() = default;
    ~PluginFactory() = default;

    struct Entry {
        std::function<std::unique_ptr<AudioPlugin>()> factory;
        std::string name;
        std::string vendor;
        PluginCategory category;
    };

    std::unordered_map<PluginID, Entry> registry_;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_PLUGIN_FACTORY_H
