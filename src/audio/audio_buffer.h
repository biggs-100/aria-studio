#ifndef ARIA_AUDIO_BUFFER_H
#define ARIA_AUDIO_BUFFER_H

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace aria {

/// Maximum number of channels supported throughout the audio engine.
static constexpr uint32_t kMaxChannels = 32;

/// AudioBuffer — multi-channel float sample buffer for real-time processing.
///
/// Channel data is stored in separate planar arrays (not interleaved).
/// Each `data[c]` pointer points to a pre-allocated float array of `capacity`
/// elements. `frames` indicates how many of those are valid.
struct AudioBuffer {
    uint32_t frames    = 0;
    uint32_t channels  = 0;
    float*   data[kMaxChannels] = {};
    uint32_t capacity  = 0;

    /// Returns writable pointer to channel @p ch (null if out of range).
    float* channel(uint32_t ch) {
        return (ch < channels) ? data[ch] : nullptr;
    }

    /// Returns read-only pointer to channel @p ch (null if out of range).
    const float* channel(uint32_t ch) const {
        return (ch < channels) ? data[ch] : nullptr;
    }

    /// Zero-fill all channels up to `frames`.
    void clear() {
        for (uint32_t c = 0; c < channels; ++c) {
            if (data[c]) {
                std::memset(data[c], 0, frames * sizeof(float));
            }
        }
    }

    /// Copy samples from @p src into this buffer.
    /// Both buffers MUST have matching channels and frames <= capacity.
    void copy_from(const AudioBuffer& src) {
        uint32_t n = std::min(frames, src.frames);
        uint32_t nc = std::min(channels, src.channels);
        for (uint32_t c = 0; c < nc; ++c) {
            if (data[c] && src.data[c]) {
                std::memcpy(data[c], src.data[c], n * sizeof(float));
            }
        }
    }

    /// Add samples from @p src multiplied by @p gain.
    /// dst[ch][i] += src[ch][i] * gain
    void add_from(const AudioBuffer& src, float gain) {
        uint32_t n = std::min(frames, src.frames);
        uint32_t nc = std::min(channels, src.channels);
        for (uint32_t c = 0; c < nc; ++c) {
            if (data[c] && src.data[c]) {
                for (uint32_t i = 0; i < n; ++i) {
                    data[c][i] += src.data[c][i] * gain;
                }
            }
        }
    }

    /// Multiply all samples by @p gain.
    void apply_gain(float gain) {
        uint32_t nc = channels;
        uint32_t n = frames;
        for (uint32_t c = 0; c < nc; ++c) {
            if (data[c]) {
                for (uint32_t i = 0; i < n; ++i) {
                    data[c][i] *= gain;
                }
            }
        }
    }
};

/// MixBuffer — 64-bit double-precision accumulation buffer.
///
/// Used for summing/sub-mixing where float precision is insufficient.
struct MixBuffer {
    double* data[kMaxChannels] = {};
    uint32_t frames  = 0;
    uint32_t channels = 0;

    double* channel(uint32_t ch) {
        return (ch < channels) ? data[ch] : nullptr;
    }

    const double* channel(uint32_t ch) const {
        return (ch < channels) ? data[ch] : nullptr;
    }

    void clear() {
        for (uint32_t c = 0; c < channels; ++c) {
            if (data[c]) {
                std::memset(data[c], 0, frames * sizeof(double));
            }
        }
    }
};

} // namespace aria

#endif // ARIA_AUDIO_BUFFER_H
