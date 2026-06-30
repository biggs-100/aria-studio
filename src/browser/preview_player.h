#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "aria_rust.h"

namespace aria {
namespace browser {

/// Browser-owned audio preview player.
/// Decodes audio via FFI and manages playback state independently
/// of the main transport engine.
class PreviewPlayer {
public:
    PreviewPlayer();
    ~PreviewPlayer();

    // Non-copyable, non-movable
    PreviewPlayer(const PreviewPlayer&) = delete;
    PreviewPlayer& operator=(const PreviewPlayer&) = delete;

    // ─── Playback control ─────────────────────────────────────

    /// Start preview playback of the given file.
    bool play(const std::string& path);

    /// Stop playback and reset position.
    void stop();

    /// Pause playback (retain position).
    void pause();

    /// Resume from paused position.
    void resume();

    // ─── State queries ────────────────────────────────────────

    bool is_playing() const { return state_ == State::Playing; }
    bool is_paused()  const { return state_ == State::Paused; }
    bool is_stopped() const { return state_ == State::Stopped; }

    /// Current playback position in seconds.
    double position() const;

    /// Duration of the currently loaded file in seconds.
    double duration() const;

    /// Currently loaded file path.
    const std::string& current_file() const { return current_file_; }

    // ─── Volume ───────────────────────────────────────────────

    /// Set volume in decibels (range: -60.0 to +12.0).
    void set_volume(double db);

    /// Get current volume in dB.
    double volume() const { return volume_db_; }

    // ─── Events ───────────────────────────────────────────────

    using StateChangeCallback = std::function<void()>;
    void on_state_change(StateChangeCallback cb) { state_cb_ = std::move(cb); }

private:
    enum class State : uint8_t { Stopped, Playing, Paused };

    std::atomic<State> state_{State::Stopped};

    std::string current_file_;
    double      volume_db_   = 0.0;   // dB
    double      position_sec_ = 0.0;
    double      duration_sec_ = 0.0;

    // Decoder thread
    std::thread    decoder_thread_;
    std::atomic<bool> decoder_running_{false};
    std::mutex     state_mutex_;

    // Ring buffer for decoded audio samples
    static constexpr size_t RING_BUFFER_SIZE = 44100 * 2 * 4;  // 2 seconds @ 44.1kHz stereo float
    std::unique_ptr<float[]> ring_buffer_;
    std::atomic<size_t>      ring_write_pos_{0};
    std::atomic<size_t>      ring_read_pos_{0};

    StateChangeCallback state_cb_;

    void decode_loop();
    void reset_ring_buffer();
};

} // namespace browser
} // namespace aria
