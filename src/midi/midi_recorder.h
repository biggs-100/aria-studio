#ifndef ARIA_MIDI_RECORDER_H
#define ARIA_MIDI_RECORDER_H

#include "midi_types.h"
#include "midi_clip.h"
#include <cstdint>
#include <vector>
#include <deque>
#include <unordered_map>

namespace aria {

// ─── JitterCompensator ──────────────────────────────────────────
// (forward-declared — full definition in jitter_compensator.h)

/// MIDI recorder — captures incoming events and builds MidiClips.
///
/// Receives MIDI events with timestamps during recording, tracks
/// note on/off pairs to build complete MidiNote objects, and
/// produces a finalized MidiClip with proper note boundaries and
/// per-note expression data.
///
/// The recorder handles:
///   - Circular event buffering during recording
///   - Note-on/note-off pairing into MidiNote objects
///   - Per-note MPE expression tracking
///   - Optional real-time quantize on record
///   - CC event capture within note boundaries
class MidiRecorder {
public:
    MidiRecorder() = default;

    // ─── Lifecycle ──────────────────────────────────────────
    /// Start recording at a given PPQN position.
    void start_recording(uint32_t ppqn_position);

    /// Stop recording and finalize the clip.
    void stop_recording();

    /// Whether recording is currently active.
    bool is_recording() const { return recording_; }

    /// Record an incoming MIDI event with its sample-accurate position.
    /// @param event          The MIDI event to record.
    /// @param sample_position Current sample position in the audio buffer.
    /// @param sample_rate    Current sample rate (for timestamp conversion).
    void record_event(const MidiEvent& event, uint64_t sample_position,
                      double sample_rate);

    /// Finalize the current recording into a MidiClip.
    /// Returns the completed clip and resets internal state.
    MidiClip finalize_clip();

    // ─── Quantize-on-record ─────────────────────────────────
    /// Enable or disable real-time quantization during recording.
    void set_quantize_on_record(bool enabled, uint32_t grid_ppqn);

    /// Whether quantize on record is enabled.
    bool quantize_on_record() const { return quantize_enabled_; }

    /// Get the current grid PPQN for quantize-on-record.
    uint32_t grid_ppqn() const { return grid_ppqn_; }

    // ─── State ──────────────────────────────────────────────
    /// Number of events recorded so far.
    size_t event_count() const { return event_buffer_.size(); }

    /// Number of completed notes formed.
    size_t note_count() const { return completed_notes_.size(); }

    /// Reset all recording state without producing a clip.
    void reset();

private:
    bool recording_      = false;
    uint32_t start_ppqn_ = 0;

    // ─── Event buffer ───────────────────────────────────────
    // Stores raw events during recording.
    std::deque<MidiEvent> event_buffer_;

    // ─── Active notes ───────────────────────────────────────
    // Maps (note << 8 | channel) → note-on event for pairing.
    // Key is a combined uint16: (note << 8) | channel.
    struct PendingNote {
        uint8_t  note;
        uint8_t  channel;
        uint8_t  velocity;
        uint64_t start_us;
        uint32_t start_ppqn;
        MPEExpression mpe;          // Accumulated expression during the note
        std::vector<MidiEvent> cc_events;  // CC events during the note
    };
    std::unordered_map<uint16_t, PendingNote> pending_notes_;

    // ─── Completed notes ────────────────────────────────────
    std::vector<MidiNote> completed_notes_;

    // ─── Clip events (CC, pitch bend, etc.) ─────────────────
    std::vector<MidiEvent> clip_events_;

    // ─── Quantize on record ─────────────────────────────────
    bool     quantize_enabled_ = false;
    uint32_t grid_ppqn_        = 480; // Default: eighth note

    // ─── Helpers ─────────────────────────────────────────────
    void handle_note_on(const MidiEvent& event, uint64_t sample_position,
                        double sample_rate);
    void handle_note_off(const MidiEvent& event, uint64_t sample_position,
                         double sample_rate);
    void handle_cc(const MidiEvent& event);

    /// Build a combined key for pending_notes_ map.
    static uint16_t note_key(uint8_t note, uint8_t channel) {
        return (static_cast<uint16_t>(note) << 8) | channel;
    }
};

} // namespace aria

#endif // ARIA_MIDI_RECORDER_H
