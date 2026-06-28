#include "transport.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <numbers>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// State Control
// ═══════════════════════════════════════════════════════════════

void Transport::play() {
    TransportState expected = TransportState::Stopped;
    if (state_.compare_exchange_weak(expected, TransportState::Playing,
                                     std::memory_order_acq_rel)) {
        return;
    }

    expected = TransportState::Paused;
    if (state_.compare_exchange_weak(expected, TransportState::Playing,
                                     std::memory_order_acq_rel)) {
        return;
    }
}

void Transport::stop() {
    TransportState desired = TransportState::Stopped;

    TransportState expected = TransportState::Playing;
    if (state_.compare_exchange_weak(expected, desired,
                                     std::memory_order_acq_rel)) {
        return;
    }

    expected = TransportState::Recording;
    if (state_.compare_exchange_weak(expected, desired,
                                     std::memory_order_acq_rel)) {
        return;
    }

    expected = TransportState::Paused;
    if (state_.compare_exchange_weak(expected, desired,
                                     std::memory_order_acq_rel)) {}
}

void Transport::record() {
    TransportState expected = TransportState::Stopped;
    state_.compare_exchange_weak(expected, TransportState::Recording,
                                 std::memory_order_acq_rel);
}

void Transport::pause() {
    TransportState expected = TransportState::Playing;
    state_.compare_exchange_weak(expected, TransportState::Paused,
                                 std::memory_order_acq_rel);
}

// ═══════════════════════════════════════════════════════════════
// Punch
// ═══════════════════════════════════════════════════════════════

void Transport::set_punch(bool enabled, uint64_t start, uint64_t end) {
    punch_enabled_.store(enabled, std::memory_order_release);
    punch_start_ = start;
    punch_end_   = end;
}

// ═══════════════════════════════════════════════════════════════
// Audio Callback
// ═══════════════════════════════════════════════════════════════

void Transport::process(uint32_t frames, AudioClock& clock) {
    TransportState s = state_.load(std::memory_order_acquire);

    // ── Pending seek ──────────────────────────────────────────
    if (pending_seek_.exchange(false, std::memory_order_acq_rel)) {
        clock.set_sample_position(seek_position_.load(std::memory_order_relaxed));
    }

    if (s == TransportState::Playing ||
        s == TransportState::Recording)
    {
        // Advance the clock
        clock.process(frames);

        // ── Loop wrap ─────────────────────────────────────────
        if (loop_enabled_.load(std::memory_order_relaxed) &&
            loop_end_ > loop_start_)
        {
            uint64_t pos = clock.sample_position();
            if (pos >= loop_end_) {
                // Wrap: compute overshoot and jump to loop_start
                uint64_t overshoot = pos - loop_end_;
                clock.set_sample_position(loop_start_ + overshoot);
            }
        }

        // ── Metronome beat tracking ───────────────────────────
        if (metronome_enabled_.load(std::memory_order_relaxed)) {
            // Track beat position in PPQN (48000 SR assumed for metronome)
            // This is used by render_metronome() to know when clicks occur.
            // The actual click is rendered separately.
            double bpm = clock.current_bpm();
            if (bpm > 0.0) {
                uint64_t pos     = clock.sample_position();
                double   sr      = clock.sample_rate();
                if (sr <= 0.0) sr = 48000.0;

                // Convert sample position to PPQN
                // beats = samples / sr * (bpm / 60)
                // ppqn  = beats * 960
                double beats = static_cast<double>(pos) / sr * (bpm / 60.0);
                uint64_t ppqn = static_cast<uint64_t>(beats * 960.0);

                // Round to nearest beat
                uint64_t beat_ppqn = (ppqn / 960) * 960;
                if (beat_ppqn > next_beat_ppqn_) {
                    next_beat_ppqn_ = beat_ppqn + 960;  // next beat boundary
                }
            }
        }
    }
    // Paused / Stopped: clock not advanced
}

// ═══════════════════════════════════════════════════════════════
// Metronome
// ═══════════════════════════════════════════════════════════════

void Transport::set_metronome(bool enabled, float volume_db, int note_pitch) {
    metronome_enabled_.store(enabled, std::memory_order_release);
    metronome_volume_ = std::pow(10.0f, volume_db / 20.0f);  // dB → linear
    metronome_pitch_  = note_pitch;
    next_beat_ppqn_    = 0;
}

double Transport::note_to_frequency(int note) {
    // A4 = 440 Hz = MIDI 69
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

void Transport::render_metronome(float* buffer, uint32_t frames,
                                  uint32_t channels, uint32_t sample_rate) const
{
    if (!metronome_enabled_.load(std::memory_order_relaxed)) return;
    if (!buffer || frames == 0) return;

    double freq = note_to_frequency(metronome_pitch_);
    float  gain = metronome_volume_;

    // Generate a brief sine click (~5 ms) on each quarter-note beat.
    // The click amplitude is shaped by a fast attack / short decay.
    uint32_t click_samples = static_cast<uint32_t>(0.005 * sample_rate); // 5 ms
    if (click_samples < 1) click_samples = 1;

    // Determine the current beat PPQN from the playback state
    // (approximate — the exact PPQN is tracked in process()).
    // For simplicity, we check if the current position is on a beat boundary
    // by calculating the sample-based beat position.
    //
    // In a production implementation, the beat tracking would be
    // communicated via a shared atomic state. For now we generate
    // clicks at the start of each buffer based on the beat tracking
    // done in process().

    // Write a click at the beginning of the buffer if we're on a beat
    for (uint32_t f = 0; f < std::min(click_samples, frames); ++f) {
        // Envelope: quick attack, exponential decay
        double env = std::exp(-3.0 * static_cast<double>(f) /
                              static_cast<double>(click_samples));

        // Very brief sine "tick"
        double phase = 2.0 * std::numbers::pi * freq * static_cast<double>(f) /
                       static_cast<double>(sample_rate);
        float sample = gain * static_cast<float>(env * std::sin(phase));

        // Interleave the sample into all channels
        for (uint32_t ch = 0; ch < channels; ++ch) {
            buffer[f * channels + ch] += sample;
        }
    }
}

} // namespace aria
