#ifndef ARIA_PLUGIN_FORMAT_SCANNER_H
#define ARIA_PLUGIN_FORMAT_SCANNER_H

#include "audio_plugin_types.h"

#include <string>
#include <vector>

namespace aria::plugin {

/// Descriptor for a discovered plugin during scanning.
struct PluginDescriptor {
    PluginID id;                   ///< Unique identifier (e.g. "com.valhalla.supermassive")
    std::string name;              ///< Human-readable display name
    std::string vendor;            ///< Developer / company name
    std::string version;           ///< Plugin version string
    std::string format;            ///< "CLAP", "VST3", etc.
    std::string path;              ///< Full filesystem path
    PluginCategory category;       ///< Synth, Effect, etc.
    uint32_t num_inputs = 0;       ///< Number of audio input channels
    uint32_t num_outputs = 0;      ///< Number of audio output channels
    bool has_editor = false;       ///< Whether the plugin provides a custom editor
    bool is_synth = false;         ///< Whether this is an instrument/synth
    std::vector<std::string> features;  ///< Tags like "equalizer", "reverb", "synth"
    uint64_t last_modified = 0;    ///< File modification timestamp for incremental scanning
};

/// Abstract interface for format-specific plugin scanners.
///
/// Each plugin format (CLAP, VST3, etc.) provides its own scanner
/// that knows how to load, query, and enumerate plugins of that format.
class FormatScanner {
public:
    virtual ~FormatScanner() = default;

    /// Human-readable name of this format (e.g. "CLAP", "VST3").
    virtual std::string name() const = 0;

    /// Scan a single file or bundle at the given path and return all
    /// plugin descriptors found within.
    ///
    /// For single-file formats (CLAP), this typically returns zero or
    /// one descriptor. For bundle formats (VST3) it may return multiple
    /// if the bundle contains several plugin components.
    virtual std::vector<PluginDescriptor> scan(const std::string& path) = 0;

    /// Return the file extension(s) this scanner handles, including
    /// the leading dot (e.g. ".clap", ".vst3").
    virtual std::string extension() const = 0;

    /// Optional: verify that a plugin binary at the given path is valid
    /// and loadable. Base implementation returns true.
    virtual bool verify(const std::string& path);
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_FORMAT_SCANNER_H