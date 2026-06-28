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
        // We set the clock position directly — the audio thread
        // will start from here on the next process() call.
        // This is a soft seek: the clock must be reset before the
        // next audio callback.
        seek_position_.store(position, std::memory_order_release);
        pending_seek_.store(true, std::memory_order_release);
    }

private:
    std::atomic<TransportState> state_{TransportState::Stopped};
    std::atomic<bool>           loop_enabled_{false};

    uint64_t loop_start_ = 0;
    uint64_t loop_end_   = 0;

    // Pending seek position (set from control thread, consumed
    // on the next audio callback process()).
    std::atomic<uint64_t> seek_position_{0};
    std::atomic<bool>     pending_seek_{false};
};

} // namespace aria

#endif // ARIA_TRANSPORT_H
