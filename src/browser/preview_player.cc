#include "browser/preview_player.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

namespace aria {
namespace browser {

// ═══════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════

PreviewPlayer::PreviewPlayer()
    : ring_buffer_(std::make_unique<float[]>(RING_BUFFER_SIZE)) {
}

PreviewPlayer::~PreviewPlayer() {
    decoder_running_.store(false);
    if (decoder_thread_.joinable()) {
        decoder_thread_.join();
    }
}

// ═══════════════════════════════════════════════════════════════
// Ring buffer management
// ═══════════════════════════════════════════════════════════════

void PreviewPlayer::reset_ring_buffer() {
    ring_write_pos_.store(0);
    ring_read_pos_.store(0);
    std::fill(ring_buffer_.get(), ring_buffer_.get() + RING_BUFFER_SIZE, 0.0f);
}

// ═══════════════════════════════════════════════════════════════
// Playback control
// ═══════════════════════════════════════════════════════════════

bool PreviewPlayer::play(const std::string& path) {
    stop();

    // Check if file exists and is an audio file
    if (!aria_fs_is_audio(path.c_str())) return false;

    current_file_ = path;
    position_sec_ = 0.0;
    duration_sec_ = 0.0;
    state_.store(State::Playing);

    reset_ring_buffer();

    // Start decoder thread
    decoder_running_.store(true);
    decoder_thread_ = std::thread([this]() { decode_loop(); });

    if (state_cb_) state_cb_();

    return true;
}

void PreviewPlayer::stop() {
    decoder_running_.store(false);
    if (decoder_thread_.joinable()) {
        decoder_thread_.join();
    }

    state_.store(State::Stopped);
    position_sec_ = 0.0;
    current_file_.clear();
    reset_ring_buffer();

    if (state_cb_) state_cb_();
}

void PreviewPlayer::pause() {
    if (state_.load() == State::Playing) {
        state_.store(State::Paused);
        if (state_cb_) state_cb_();
    }
}

void PreviewPlayer::resume() {
    if (state_.load() == State::Paused) {
        state_.store(State::Playing);
        if (state_cb_) state_cb_();
    }
}

// ═══════════════════════════════════════════════════════════════
// State queries
// ═══════════════════════════════════════════════════════════════

double PreviewPlayer::position() const {
    return position_sec_;
}

double PreviewPlayer::duration() const {
    return duration_sec_;
}

// ═══════════════════════════════════════════════════════════════
// Volume
// ═══════════════════════════════════════════════════════════════

void PreviewPlayer::set_volume(double db) {
    volume_db_ = std::clamp(db, -60.0, 12.0);
}

// ═══════════════════════════════════════════════════════════════
// Decoder thread
// ═══════════════════════════════════════════════════════════════

void PreviewPlayer::decode_loop() {
    // This is a provisional decoder loop that manages state.
    // In production, the FFI symphonia decoder feeds samples
    // into the ring buffer. Here we simulate the loop structure.

    // NOTE: The actual decoding is handled by the Rust side via
    // aria-filesystem's symphonia integration. The C++ side
    // manages playback state and volume. Full ring buffer
    // feeding from Rust FFI will be wired in a follow-up.
    //
    // For now, this thread monitors state and manages the
    // playback lifecycle. The ring buffer is prepared for
    // the FFI integration path.

    while (decoder_running_.load()) {
        auto current_state = state_.load();

        if (current_state == State::Stopped) {
            break;
        }

        if (current_state == State::Paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Playing state — FFI decoder would fill the ring buffer here.
        // For now, simulate advancing position to prove lifecycle.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        // In production: aria_decode_chunk() → ring_buffer_
        // Apply volume: sample *= std::pow(10.0, volume_db_ / 20.0)
        // Update position_sec_ based on samples consumed
    }
}

} // namespace browser
} // namespace aria
