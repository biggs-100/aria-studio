#ifndef ARIA_PLUGIN_PLUGIN_SCANNER_DEFAULTS_H
#define ARIA_PLUGIN_PLUGIN_SCANNER_DEFAULTS_H

#include <string>
#include <vector>

namespace aria::plugin {

/// Return the platform-specific default plugin scan paths.
///
/// Windows:
///   %PROGRAMFILES%\Common Files\CLAP\
///   %PROGRAMFILES%\Common Files\VST3\
///   %APPDATA%\ARIA\Plugins\
///
/// macOS:
///   ~/Library/Audio/Plug-Ins/CLAP/
///   ~/Library/Audio/Plug-Ins/VST3/
///   ~/Library/Application Support/ARIA/Plugins/
///
/// Linux:
///   ~/.clap/  ~/.vst3/  ~/.aria/plugins/
///   /usr/lib/clap/  /usr/lib/vst3/
std::vector<std::string> default_scan_paths();

} // namespace aria::plugin

#endif // ARIA_PLUGIN_PLUGIN_SCANNER_DEFAULTS_H
