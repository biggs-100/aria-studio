#include "mixer/sidechain.h"

#include <algorithm>
#include <cstring>

namespace aria {

// ── SidechainManager ────────────────────────────────────────────

void SidechainManager::set_sidechain(ChannelID source, ChannelID target) {
    SidechainConnection conn;
    conn.source = source;
    // Buffer is prepared in prepare()
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

void SidechainManager::process(AudioBuffer** inputs, uint32_t num_inputs,
                                uint32_t frames,
                                const std::function<Channel*(ChannelID)>& get_channel)
{
    for (auto& [target, conn] : connections_) {
        Channel* source_ch = get_channel(conn.source);
        if (!source_ch) {
            // Source channel not found — zero the sidechain buffer
            for (uint32_t c = 0; c < conn.buffer.channels; ++c) {
                if (conn.buffer.data[c]) {
                    std::memset(conn.buffer.data[c], 0,
                                frames * sizeof(float));
                }
            }
            conn.buffer.frames = frames;
            continue;
        }

        uint32_t idx = source_ch->input_index();
        if (idx >= num_inputs || !inputs[idx]) {
            // No input data — zero the sidechain buffer
            for (uint32_t c = 0; c < conn.buffer.channels; ++c) {
                if (conn.buffer.data[c]) {
                    std::memset(conn.buffer.data[c], 0,
                                frames * sizeof(float));
                }
            }
            conn.buffer.frames = frames;
            continue;
        }

        AudioBuffer* src_buf = inputs[idx];
        uint32_t n = std::min(frames, src_buf->frames);
        uint32_t nc = std::min(src_buf->channels, conn.buffer.channels);

        // Copy source audio into sidechain buffer
        for (uint32_t c = 0; c < nc; ++c) {
            if (conn.buffer.data[c] && src_buf->data[c]) {
                std::memcpy(conn.buffer.data[c], src_buf->data[c],
                            static_cast<size_t>(n) * sizeof(float));
            }
        }

        // Zero any remaining channels in the sidechain buffer
        // beyond what the source provides
        for (uint32_t c = nc; c < conn.buffer.channels; ++c) {
            if (conn.buffer.data[c]) {
                std::memset(conn.buffer.data[c], 0,
                            static_cast<size_t>(frames) * sizeof(float));
            }
        }

        conn.buffer.frames = n;
    }
}

void SidechainManager::prepare(uint32_t max_channels, uint32_t max_frames) {
    for (auto& [target, conn] : connections_) {
        // Only reallocate if size changed
        size_t needed = static_cast<size_t>(max_channels) * max_frames;
        if (conn.storage.size() < needed) {
            conn.storage.resize(needed, 0.0f);
        }

        conn.buffer.channels = max_channels;
        conn.buffer.frames   = max_frames;
        conn.buffer.capacity = max_frames;

        // Point data pointers into owned storage
        for (uint32_t c = 0; c < max_channels; ++c) {
            conn.buffer.data[c] = conn.storage.data() + c * max_frames;
        }
        // Clear any remaining channel pointers beyond max_channels
        for (uint32_t c = max_channels; c < kMaxChannels; ++c) {
            conn.buffer.data[c] = nullptr;
        }
    }
}

void SidechainManager::clear() {
    connections_.clear();
}

} // namespace aria
