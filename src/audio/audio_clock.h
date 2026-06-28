#ifndef ARIA_AUDIO_CLOCK_H
#define ARIA_AUDIO_CLOCK_H

#include "tempo_map.h"

#include <atomic>
#include <cstdint>

namespace aria {

/// Audio clock — sample-accurate position tracking for the audio engine.
///
/// Tracks the current sample position and provides time-domain conversions.
/// The clock is updated on every audio callback via process().
///
/// Thread safety:
///   - process() is called from the audio thread (real-time safe).
///   - Getters use relaxed atomic loads (single writer, multiple readers).
///   - set_tempo_map() is called from the control thread.
class AudioClock {
public:
    AudioClock() = default;

    /// Advance the clock by @p frames samples.
    /// Called from the audio callback — MUST be real-time safe.
    /// @param frames  Number of frames processed in this callback.
    void process(uint32_t frames);

    // ─── Position queries ──────────────────────────────────────
    /// Current sample position (global).
    uint64_t sample_position() const {
        return sample_position_.load(std::memory_order_relaxed);
    }

    /// Current time in seconds.
    double current_time() const;

    /// Current BPM at the current sample position.
    double current_bpm() const {
        return current_bpm_.load(std::memory_order_relaxed);
    }

    /// Current beat position (in quarter notes).
    /// Calculated from sample position and the current BPM.
    double beat_position() const;

    /// Current measure (bar) number (1-based).
    /// Assumes 4/4 time signature.
    uint32_t current_measure() const;

    /// Current beat within the measure (0-based, in quarter notes).
    uint32_t current_beat() const;

    // ─── Configuration ─────────────────────────────────────────
    /// Set the tempo map for BPM queries.
    /// Called from the control thread.
    void set_tempo_map(const TempoMap& map) { tempo_map_ = map; }

    /// Get the current tempo map (const reference).
    const TempoMap& tempo_map() const { return tempo_map_; }

    /// Reset the clock to zero.
    void reset();

private:
    /// Convert sample position to time in seconds.
    /// This is an approximation that assumes the current sample rate.
    /// For sample-accurate conversion, the engine needs to provide
    /// the actual sample rate per callback.
    static constexpr double kDefaultSampleRate = 48000.0;

    // Current state — only modified by process() on the audio thread.
    std::atomic<uint64_t> sample_position_{0};
    std::atomic<double>   current_bpm_{120.0};

    // Tempo map (read-only after set, modified on control thread).
    TempoMap tempo_map_;

    // Cached sample rate (set externally by audio engine init).
    std::atomic<double> sample_rate_{kDefaultSampleRate};

public:
    /// Set the sample rate for time/beat calculations.
    /// Called during engine initialization.
    void set_sample_rate(double sr) {
        sample_rate_.store(sr, std::memory_order_release);
    }

    /// Current sample rate used for conversions.
    double sample_rate() const {
        return sample_rate_.load(std::memory_order_relaxed);
    }
};

} // namespace aria

#endif // ARIA_AUDIO_CLOCK_H
