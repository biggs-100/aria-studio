#include "plugin_factory.h"

#include <algorithm>
#include <cctype>
#include <locale>

namespace aria::plugin {

namespace {

/// Case-insensitive character comparison.
bool iequal(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

/// Case-insensitive substring search.
bool contains_icase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(), iequal);
    return it != haystack.end();
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════
//  Singleton
// ══════════════════════════════════════════════════════════════════════

PluginFactory& PluginFactory::instance() {
    static PluginFactory factory;
    return factory;
}

// ══════════════════════════════════════════════════════════════════════
//  Registration
// ══════════════════════════════════════════════════════════════════════

void PluginFactory::register_plugin(
    PluginID id,
    std::function<std::unique_ptr<AudioPlugin>()> factory,
    std::string name,
    std::string vendor,
    PluginCategory category)
{
    Entry entry;
    entry.factory = std::move(factory);
    entry.name = std::move(name);
    entry.vendor = std::move(vendor);
    entry.category = category;
    registry_[std::move(id)] = std::move(entry);
}

// ══════════════════════════════════════════════════════════════════════
//  Instantiation
// ══════════════════════════════════════════════════════════════════════

std::unique_ptr<AudioPlugin> PluginFactory::create(const PluginID& id) const {
    auto it = registry_.find(id);
    if (it == registry_.end() || !it->second.factory) {
        return nullptr;
    }
    return it->second.factory();
}

// ══════════════════════════════════════════════════════════════════════
//  Queries
// ══════════════════════════════════════════════════════════════════════

size_t PluginFactory::count() const {
    return registry_.size();
}

PluginCategory PluginFactory::category(const PluginID& id) const {
    auto it = registry_.find(id);
    if (it == registry_.end()) return PluginCategory::Other;
    return it->second.category;
}

bool PluginFactory::has(const PluginID& id) const {
    return registry_.find(id) != registry_.end();
}

std::vector<PluginID> PluginFactory::plugin_ids() const {
    std::vector<PluginID> ids;
    ids.reserve(registry_.size());
    for (const auto& [id, _] : registry_) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<PluginID> PluginFactory::search(const std::string& query,
                                              PluginCategory category) const
{
    std::vector<PluginID> results;

    for (const auto& [id, entry] : registry_) {
        // Apply category filter
        if (category != PluginCategory::Other && entry.category != category) {
            continue;
        }

        // Apply text search (match name, vendor, or ID)
        if (!query.empty()) {
            bool match = contains_icase(entry.name, query) ||
                         contains_icase(entry.vendor, query) ||
                         contains_icase(id, query);
            if (!match) continue;
        }

        results.push_back(id);
    }

    // Sort for deterministic output
    std::sort(results.begin(), results.end());

    return results;
}

} // namespace aria::plugin
