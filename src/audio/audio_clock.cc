#include "audio_clock.h"

#include <cmath>

namespace aria {

void AudioClock::process(uint32_t frames) {
    // Advance sample position
    uint64_t pos = sample_position_.load(std::memory_order_relaxed);
    pos += frames;
    sample_position_.store(pos, std::memory_order_release);

    // Update current BPM from tempo map
    double bpm = tempo_map_.bpm_at(pos);
    current_bpm_.store(bpm, std::memory_order_release);
}

double AudioClock::current_time() const {
    uint64_t pos = sample_position_.load(std::memory_order_relaxed);
    double   sr  = sample_rate_.load(std::memory_order_relaxed);
    if (sr <= 0.0) sr = kDefaultSampleRate;
    return static_cast<double>(pos) / sr;
}

double AudioClock::beat_position() const {
    // Quarter-note position: time_in_seconds * (bpm / 60)
    double bpm = current_bpm_.load(std::memory_order_relaxed);
    double sec = current_time();
    return sec * (bpm / 60.0);
}

uint32_t AudioClock::current_measure() const {
    // Assumes 4/4 time: 4 quarter-notes per measure.
    // beat_position is 0-based → measure is floor(bp / 4) + 1.
    double bp = beat_position();
    if (bp < 0.0) return 1;
    return static_cast<uint32_t>(bp / 4.0) + 1;
}

uint32_t AudioClock::current_beat() const {
    // 0-based beat within the current measure (4/4).
    double bp = beat_position();
    if (bp < 0.0) return 0;
    return static_cast<uint32_t>(std::floor(bp)) % 4;
}

void AudioClock::reset() {
    sample_position_.store(0, std::memory_order_release);
    current_bpm_.store(tempo_map_.default_bpm(), std::memory_order_release);
}

} // namespace aria
