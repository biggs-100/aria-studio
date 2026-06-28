#include "audio_engine.h"

namespace aria {

AudioEngine::AudioEngine() = default;
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init(uint32_t sample_rate, uint32_t buffer_size) {
    sample_rate_ = sample_rate;
    buffer_size_ = buffer_size;
    initialized_ = true;
    return true;
}

void AudioEngine::shutdown() {
    initialized_ = false;
}

bool AudioEngine::is_initialized() const { return initialized_; }
uint32_t AudioEngine::sample_rate() const { return sample_rate_; }
uint32_t AudioEngine::buffer_size() const { return buffer_size_; }

} // namespace aria
