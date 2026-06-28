#ifndef ARIA_MIXER_MIXER_H
#define ARIA_MIXER_MIXER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "mixer/channel.h"
#include "audio/audio_buffer.h"

namespace aria {

// ── Mixer Configuration ─────────────────────────────────────────
struct MixerConfig {
    uint32_t max_channels            = 1024;
    uint32_t max_buses               = 128;
    uint32_t max_sends_per_channel   = 16;
    double   master_volume_min_db    = -60.0;
    double   master_volume_max_db    = +24.0;
};

// ── Pan Law ─────────────────────────────────────────────────────
enum class PanLaw {
    EqualPower_3dB,     // -3 dB at center, sin/cos (default)
    Linear_0dB,         // 0 dB at center, constant amplitude
    Linear_6dB,         // -6 dB at center, constant power (mono)
    StereoBalance       // Balance control, reduces opposite side
};

// ── Mixer Engine ────────────────────────────────────────────────
///
/// Central mixer engine managing channel strips, buses, and audio
/// summing. All internal summation uses 64-bit float precision via
/// MixBuffer to prevent truncation noise.
class Mixer {
public:
    Mixer();
    ~Mixer();

    /// Initialise the mixer with a sample rate.
    bool init(uint32_t sample_rate);

    /// Shut down and release all resources.
    void shutdown();

    // ── Channel management ─────────────────────────────────────
    ChannelID create_channel(const std::string& name, ChannelType type);
    void      destroy_channel(ChannelID id);
    Channel*  get_channel(ChannelID id);
    const Channel* get_channel(ChannelID id) const;
    std::vector<ChannelID> all_channels() const;

    // ── Buses ──────────────────────────────────────────────────
    BusID              create_bus(const std::string& name);
    void               destroy_bus(BusID id);
    void               assign_channel_to_bus(ChannelID ch, BusID bus);
    void               remove_channel_from_bus(ChannelID ch);
    BusID              channel_bus(ChannelID ch) const;
    std::string        bus_name(BusID id) const;
    std::vector<BusID> all_buses() const;

    // ── Master ─────────────────────────────────────────────────
    Channel*       master_channel();
    const Channel* master_channel() const;

    // ── Pan law ────────────────────────────────────────────────
    void   set_pan_law(PanLaw law) { pan_law_ = law; }
    PanLaw pan_law() const         { return pan_law_; }

    // ── Processing ─────────────────────────────────────────────
    /// Main process call from the audio thread.
    /// @param inputs       Array of input audio buffers (one per audio channel)
    /// @param num_inputs   Number of inputs in the array
    /// @param master_output Destination buffer for the master output
    /// @param frames       Number of sample frames to process
    void process(AudioBuffer** inputs, uint32_t num_inputs,
                 AudioBuffer* master_output, uint32_t frames);

    // ── Configuration ──────────────────────────────────────────
    void set_config(const MixerConfig& config) { config_ = config; }
    const MixerConfig& config() const { return config_; }

private:
    // Rebuild the processing order (topological sort).
    void rebuild_processing_order();

    // Apply equal-power pan law (sin/cos).
    void apply_equal_power_pan(float in_l, float in_r, double pan,
                               float* out_l, float* out_r) const;

    // Channels indexed by ChannelID
    std::unordered_map<ChannelID, std::unique_ptr<Channel>> channels_;
    ChannelID next_channel_id_ = 1;

    // Master channel
    ChannelID master_id_ = kInvalidChannelID;

    // Bus definitions: BusID → vector of ChannelID
    struct BusInfo {
        std::string name;
        std::vector<ChannelID> members;
    };
    std::unordered_map<BusID, BusInfo> buses_;
    BusID next_bus_id_ = 1;

    // Channel → bus assignment (reverse lookup)
    std::unordered_map<ChannelID, BusID> channel_to_bus_;

    // Processing state
    uint32_t sample_rate_ = 48000;
    PanLaw pan_law_ = PanLaw::EqualPower_3dB;
    MixerConfig config_;

    // Cached processing order
    std::vector<ChannelID> processing_order_;
    bool order_dirty_ = true;
};

} // namespace aria

#endif // ARIA_MIXER_MIXER_H
