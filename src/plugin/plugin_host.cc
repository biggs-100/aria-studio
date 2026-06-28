#include "plugin_host.h"

namespace aria {

PluginHost::PluginHost() = default;
PluginHost::~PluginHost() { shutdown(); }

bool PluginHost::init() { initialized_ = true; return true; }
void PluginHost::shutdown() { initialized_ = false; }

std::vector<PluginHost::PluginInfo> PluginHost::scan_plugins() { return {}; }
uint32_t PluginHost::plugin_count() const { return 0; }

} // namespace aria
