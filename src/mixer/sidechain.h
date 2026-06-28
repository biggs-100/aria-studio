#ifndef ARIA_MIXER_SIDECHAIN_H
#define ARIA_MIXER_SIDECHAIN_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "mixer/channel.h"
#include "audio/audio_buffer.h"

namespace aria {

// ── Sidechain Manager ──────────────────────────────────────────
///
/// Routes audio from a source channel to a target channel's sidechain
/// input. The target channel's compressor or plugin uses this signal
/// as the detection input (e.g., kick → bass compressor sidechain).
class SidechainManager {
public:
    SidechainManager() = default;
    ~SidechainManager() = default;

    /// Configure sidechain: route audio from @p source to @p target.
    void set_sidechain(ChannelID source, ChannelID target);

    /// Remove sidechain connection for @p target.
    void remove_sidechain(ChannelID target);

    /// Check if @p target has a sidechain source.
    bool has_sidechain(ChannelID target) const;

    /// Get the sidechain audio buffer for @p target (read-only).
    const AudioBuffer* get_sidechain_buffer(ChannelID target) const;

    /// Process sidechain routing for the current buffer.
    /// Must be called from the audio thread each process cycle.
    void process(uint32_t frames);

    /// Prepare internal buffers for a given channel configuration.
    void prepare(uint32_t max_channels, uint32_t max_frames);

private:
    struct SidechainConnection {
        ChannelID source;
        AudioBuffer buffer;
    };

    std::unordered_map<ChannelID, SidechainConnection> connections_;
};

} // namespace aria

#endif // ARIA_MIXER_SIDECHAIN_H
