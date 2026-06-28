#include "audio_plugin_types.h"

#include <cstdlib>
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
///   ~/.clap/
///   ~/.vst3/
///   ~/.aria/plugins/
///   /usr/lib/clap/
///   /usr/lib/vst3/
std::vector<std::string> default_scan_paths() {
    std::vector<std::string> paths;

#if defined(_WIN32)
    // Windows
    auto get_env = [](const char* var) -> std::string {
        char* val = nullptr;
        size_t len = 0;
        if (_dupenv_s(&val, &len, var) == 0 && val) {
            std::string result(val);
            free(val);
            return result;
        }
        return {};
    };

    std::string program_files = get_env("PROGRAMFILES");
    if (!program_files.empty()) {
        paths.push_back(program_files + "\\Common Files\\CLAP\\");
        paths.push_back(program_files + "\\Common Files\\VST3\\");
    }

    std::string appdata = get_env("APPDATA");
    if (!appdata.empty()) {
        paths.push_back(appdata + "\\ARIA\\Plugins\\");
    }

#elif defined(__APPLE__)
    // macOS
    const char* home = std::getenv("HOME");
    if (home) {
        std::string h(home);
        paths.push_back(h + "/Library/Audio/Plug-Ins/CLAP/");
        paths.push_back(h + "/Library/Audio/Plug-Ins/VST3/");
        paths.push_back(h + "/Library/Application Support/ARIA/Plugins/");
    }

    // System-wide paths
    paths.push_back("/Library/Audio/Plug-Ins/CLAP/");
    paths.push_back("/Library/Audio/Plug-Ins/VST3/");

#else
    // Linux / other Unix
    const char* home = std::getenv("HOME");
    if (home) {
        std::string h(home);
        paths.push_back(h + "/.clap/");
        paths.push_back(h + "/.vst3/");
        paths.push_back(h + "/.aria/plugins/");
    }

    paths.push_back("/usr/lib/clap/");
    paths.push_back("/usr/lib/vst3/");
    paths.push_back("/usr/local/lib/clap/");
    paths.push_back("/usr/local/lib/vst3/");

#endif

    return paths;
}

} // namespace aria::plugin
