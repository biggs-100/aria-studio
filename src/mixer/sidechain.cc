#include "mixer/sidechain.h"

#include <algorithm>
#include <cstring>

namespace aria {

// ── SidechainManager ────────────────────────────────────────────

void SidechainManager::set_sidechain(ChannelID source, ChannelID target) {
    SidechainConnection conn;
    conn.source = source;
    // Buffer is prepared in prepare() or on first process
    connections_[target] = conn;
}

void SidechainManager::remove_sidechain(ChannelID target) {
    connections_.erase(target);
}

bool SidechainManager::has_sidechain(ChannelID target) const {
    return connections_.find(target) != connections_.end();
}

const AudioBuffer* SidechainManager::get_sidechain_buffer(ChannelID target) const {
    auto it = connections_.find(target);
    if (it != connections_.end()) {
        return &it->second.buffer;
    }
    return nullptr;
}

void SidechainManager::process(uint32_t frames) {
    // Sidechain processing copies the source channel's audio buffer
    // into the target's sidechain buffer. The actual audio data
    // is provided externally by the Mixer during process().
    //
    // For Slice 1, this is a placeholder — full sidechain routing
    // requires integration with the Mixer's process loop, which
    // will be completed in a follow-up slice.
    //
    // The prepare() method ensures buffers exist.
    (void)frames;
}

void SidechainManager::prepare(uint32_t max_channels, uint32_t max_frames) {
    for (auto& [target, conn] : connections_) {
        if (conn.buffer.capacity < max_frames ||
            conn.buffer.channels < max_channels)
        {
            // Reallocate — in production this would use a BufferPool
            conn.buffer.channels = max_channels;
            conn.buffer.frames   = max_frames;
            conn.buffer.capacity = max_frames;
            // Data pointers are expected to be set externally
            // from a buffer pool or pre-allocated storage
        }
    }
}

} // namespace aria
