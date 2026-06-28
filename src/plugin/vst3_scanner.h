#ifndef ARIA_PLUGIN_VST3_SCANNER_H
#define ARIA_PLUGIN_VST3_SCANNER_H

#include "format_scanner.h"

#include <string>
#include <vector>

namespace aria::plugin {

/// FormatScanner for VST3 (.vst3) plugin bundles.
///
/// Loads each .vst3 module, queries IPluginFactory for plugin classes,
/// and returns discovered PluginDescriptors.
///
/// All VST3-specific code is compiled only when ARIA_FEATURE_VST3 is
/// defined. Without it, VST3Scanner always returns empty results.
class VST3Scanner : public FormatScanner {
public:
    std::string name() const override;
    std::vector<PluginDescriptor> scan(const std::string& path) override;
    std::string extension() const override;
    bool verify(const std::string& path) override;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_VST3_SCANNER_H