#ifndef ARIA_AUDIO_ENGINE_H
#define ARIA_AUDIO_ENGINE_H

#include <cstdint>
#include <string>
#include <vector>

namespace aria {

/// Audio Engine — real-time multi-track audio I/O and processing.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init(uint32_t sample_rate, uint32_t buffer_size);
    void shutdown();
    bool is_initialized() const;

    uint32_t sample_rate() const;
    uint32_t buffer_size() const;

private:
    bool initialized_ = false;
    uint32_t sample_rate_ = 48000;
    uint32_t buffer_size_ = 128;
};

} // namespace aria

#endif // ARIA_AUDIO_ENGINE_H
