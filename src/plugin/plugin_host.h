#ifndef ARIA_PLUGIN_HOST_H
#define ARIA_PLUGIN_HOST_H

#include <cstdint>
#include <string>
#include <vector>

namespace aria {

/// Plugin Host — CLAP-native plugin loader with VST3/AU/LV2 support.
class PluginHost {
public:
    PluginHost();
    ~PluginHost();

    bool init();
    void shutdown();

    struct PluginInfo {
        std::string id;
        std::string name;
        std::string vendor;
        std::string format;
    };

    std::vector<PluginInfo> scan_plugins();
    uint32_t plugin_count() const;

private:
    bool initialized_ = false;
};

} // namespace aria

#endif // ARIA_PLUGIN_HOST_H
