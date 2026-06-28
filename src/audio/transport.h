#ifndef ARIA_TRANSPORT_H
#define ARIA_TRANSPORT_H

#include "audio_clock.h"

#include <atomic>
#include <cstdint>
#include <functional>

namespace aria {

/// Transport state machine.
enum class TransportState : uint8_t {
    Stopped   = 0,
    Playing   = 1,
    Recording = 2,
    Paused    = 3,
};

/// Transport — playhead management and loop control.
///
/// Manages the transport state machine:
///
///   Stopped ──► Playing ──► Paused ──► Playing
///       │                        │
///       └──► Recording ──► Stopped
///
/// process() is called from the audio callback to update the clock and
/// enforce loop range. All state transitions are lock-free via atomics.
///
/// Thread safety:
///   - play(), stop(), record(), pause() are called from control thread.
///   - process() is called from the real-time audio thread.
///   - State is stored in std::atomic<TransportState>.
class Transport {
public:
    Transport() = default;

    // ─── State control (control thread) ────────────────────────

    /// Start playback from the current position.
    /// If paused, resumes from the paused position.
    void play();

    /// Stop playback and reset the clock to position 0.
    void stop();

    /// Start recording (enters Recording state).
    void record();

    /// Pause playback — keeps the current clock position.
    void pause();

    // ─── State queries (any thread) ────────────────────────────
    TransportState state() const {
        return state_.load(std::memory_order_relaxed);
    }

    bool is_playing() const {
        TransportState s = state_.load(std::memory_order_relaxed);
        return s == TransportState::Playing;
    }

    bool is_recording() const {
        return state_.load(std::memory_order_relaxed) == TransportState::Recording;
    }

    bool is_stopped() const {
        return state_.load(std::memory_order_relaxed) == TransportState::Stopped;
    }

    bool is_paused() const {
        return state_.load(std::memory_order_relaxed) == TransportState::Paused;
    }

    // ─── Loop control (control thread) ─────────────────────────

    /// Enable/disable loop playback.
    void set_loop(bool enabled) {
        loop_enabled_.store(enabled, std::memory_order_release);
    }

    /// Whether looping is enabled.
    bool loop_enabled() const {
        return loop_enabled_.load(std::memory_order_relaxed);
    }

    /// Set the loop range in sample positions.
    /// @param start  Loop start sample position.
    /// @param end    Loop end sample position (exclusive).
    void set_loop_range(uint64_t start, uint64_t end) {
        loop_start_ = start;
        loop_end_   = end;
    }

    uint64_t loop_start() const { return loop_start_; }
    uint64_t loop_end() const { return loop_end_; }

    // ─── Audio callback integration ────────────────────────────

    /// Process one audio callback block.
    /// Updates the clock and handles loop wrapping.
    /// MUST be called from the real-time audio thread.
    /// @param frames  Number of frames in this block.
    /// @param clock   Audio clock to update.
    void process(uint32_t frames, AudioClock& clock);

    // ─── Position control (control thread) ─────────────────────

    /// Set playback position (sample position).
    void set_position(uint64_t position) {
        seek_position_.store(position, std::memory_order_release);
        pending_seek_.store(true, std::memory_order_release);
    }

    // ─── Punch in/out (control thread) ─────────────────────────

    /// Enable/disable punch recording range.
    void set_punch(bool enabled, uint64_t start, uint64_t end);

    /// Whether punch is enabled.
    bool punch_enabled() const {
        return punch_enabled_.load(std::memory_order_relaxed);
    }

    uint64_t punch_start() const { return punch_start_; }
    uint64_t punch_end() const { return punch_end_; }

    // ─── Metronome (control thread) ─────────────────────────---

    /// Configure the metronome.
    /// @param enabled    Enable/disable metronome clicks.
    /// @param volume_db  Click volume in dB.
    /// @param note_pitch MIDI note number for the click pitch (default 76 = E6).
    void set_metronome(bool enabled, float volume_db = -6.0f, int note_pitch = 76);

    /// Whether the metronome is active.
    bool metronome_enabled() const {
        return metronome_enabled_.load(std::memory_order_relaxed);
    }

    /// Render metronome clicks into an audio buffer.
    /// Must be called after process() on the same audio block.
    /// @param buffer      Output buffer (interleaved float samples).
    /// @param frames      Number of frames in the buffer.
    /// @param channels    Number of interleaved channels.
    /// @param sample_rate Current sample rate for sine generation.
    void render_metronome(float* buffer, uint32_t frames,
                          uint32_t channels, uint32_t sample_rate) const;

private:
    std::atomic<TransportState> state_{TransportState::Stopped};
    std::atomic<bool>           loop_enabled_{false};

    uint64_t loop_start_ = 0;
    uint64_t loop_end_   = 0;

    // ─── Seek state ────────────────────────────────────────────
    std::atomic<uint64_t> seek_position_{0};
    std::atomic<bool>     pending_seek_{false};

    // ─── Punch state ───────────────────────────────────────────
    std::atomic<bool> punch_enabled_{false};
    uint64_t punch_start_ = 0;
    uint64_t punch_end_   = 0;

    // ─── Metronome state ───────────────────────────────────────
    std::atomic<bool> metronome_enabled_{false};
    float  metronome_volume_ = 0.0f;   // linear gain (converted from dB)
    int    metronome_pitch_  = 76;     // MIDI note → frequency
    mutable uint64_t next_beat_ppqn_ = 0;  // next beat to click (tracked by process)

    /// Convert MIDI note to frequency (A4 = 440 Hz, MIDI 69).
    static double note_to_frequency(int note);
};

} // namespace aria

#endif // ARIA_TRANSPORT_H
