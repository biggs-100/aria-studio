#ifndef ARIA_PLUGIN_CLAP_SCANNER_H
#define ARIA_PLUGIN_CLAP_SCANNER_H

#include "format_scanner.h"

#include <string>
#include <vector>

namespace aria::plugin {

/// FormatScanner for CLAP (.clap) plugin binaries.
///
/// Opens each .clap file via dlopen/LoadLibrary, queries the
/// clap_plugin_entry factory, and returns discovered PluginDescriptors.
class CLAPScanner : public FormatScanner {
public:
    std::string name() const override;
    std::vector<PluginDescriptor> scan(const std::string& path) override;
    std::string extension() const override;
    bool verify(const std::string& path) override;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_CLAP_SCANNER_H