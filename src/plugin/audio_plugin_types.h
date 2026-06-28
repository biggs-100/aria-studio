#ifndef ARIA_PLUGIN_AUDIO_PLUGIN_TYPES_H
#define ARIA_PLUGIN_AUDIO_PLUGIN_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace aria::plugin {

/// Unique parameter identifier.
using ParamID = uint32_t;

/// Unique plugin identifier (e.g. "aria.eq", "com.valhalla.supermassive").
using PluginID = std::string;

/// Broad category for plugin classification and search.
enum class PluginCategory {
    Synth,
    Effect,
    Analyzer,
    Utility,
    Instrument,
    Other
};

/// Metadata describing a single plugin parameter.
struct ParameterInfo {
    ParamID id = 0;
    std::string name;
    double min_value = 0.0;
    double max_value = 1.0;
    double default_value = 0.0;
    std::vector<std::string> value_labels;  // populated for enum-style params, empty otherwise
};

/// Context passed to AudioPlugin::process() for each audio block.
struct ProcessContext {
    double sample_rate = 48000.0;
    uint32_t num_frames = 0;
    uint64_t frame_offset = 0;  // sample-accurate position in project
    uint32_t num_inputs = 0;
    uint32_t num_outputs = 0;
    bool is_bypassed = false;
    bool is_offline = false;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_AUDIO_PLUGIN_TYPES_H
