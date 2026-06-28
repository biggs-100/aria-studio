#ifndef ARIA_NOTE_TRACKER_H
#define ARIA_NOTE_TRACKER_H

#include "midi_types.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace aria {

/// Tracks active MIDI notes including sustain pedal state.
///
/// The NoteTracker maintains a map of currently-sounding notes, handling
/// sustain pedal behavior: when the pedal is held, note-off events are
/// deferred until the pedal is released.
class NoteTracker {
public:
    /// Register a note-on event.
    void note_on(uint8_t note, uint8_t channel, uint8_t velocity,
                 uint64_t sample_pos);

    /// Register a note-off event.
    /// If sustain is active, the note is marked as sustained until
    /// sustain() is called with pedal_down = false.
    void note_off(uint8_t note, uint8_t channel, uint64_t sample_pos);

    /// Immediately release all active notes (panic).
    void all_notes_off();

    /// Set sustain pedal state.
    /// When released (pedal_down = false), any sustained notes that
    /// received note-off while the pedal was down are terminated.
    void sustain(bool pedal_down);

    /// Check if a specific note+channel is currently active.
    bool is_note_active(uint8_t note, uint8_t channel) const;

    /// Number of currently active (sounding) notes.
    uint32_t active_note_count() const;

    /// Access all active notes for inspection.
    const std::vector<NoteState>& active_notes() const { return active_note_list_; }

    /// Get a specific active note state (nullptr if not active or not found).
    const NoteState* active_note(uint8_t note, uint8_t channel) const;

private:
    /// Composite key: (note << 8) | channel
    static uint16_t make_key(uint8_t note, uint8_t channel) {
        return (static_cast<uint16_t>(note) << 8) | channel;
    }

    /// A note that has been released but is held by the sustain pedal.
    struct SustainedNote {
        uint8_t  note;
        uint8_t  channel;
        uint8_t  velocity;
        uint8_t  release_velocity;
        uint64_t note_off_sample;
    };

    std::unordered_map<uint16_t, NoteState> active_notes_;
    std::vector<NoteState> active_note_list_;  // cached for const access
    std::vector<SustainedNote> sustained_notes_;
    bool sustain_pedal_ = false;
};

} // namespace aria

#endif // ARIA_NOTE_TRACKER_H
