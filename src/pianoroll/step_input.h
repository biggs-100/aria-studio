#ifndef ARIA_PIANOROLL_STEP_INPUT_H
#define ARIA_PIANOROLL_STEP_INPUT_H

#include "note.h"
#include "note_collection.h"
#include "midi/midi_types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace aria {

// ─── MidiStepInput ──────────────────────────────────────────────

/// Handles real-time and step-based MIDI note input into the piano roll.
///
/// Supports three input modes:
///   - RealTime: notes are recorded as they are played
///   - Step: each note advances the position by step_duration
///   - Replace: held notes replace existing notes at the cursor position
class MidiStepInput {
public:
    // ─── Input mode ───────────────────────────────────────────

    enum InputMode : uint8_t {
        RealTime,   ///< Record as played (natural timing)
        Step,       ///< Step sequencer (advance after each note-on)
        Replace     ///< Replace note at cursor position
    };

    // ─── Configuration ────────────────────────────────────────

    void set_enabled(bool enabled);
    bool is_enabled() const { return enabled_; }

    void set_mode(InputMode mode);
    InputMode mode() const { return mode_; }

    /// Set the step duration in PPQN (default 240 = 16th note).
    void set_step_duration(uint32_t ppqn) { step_ppqn_ = ppqn; }
    uint32_t step_duration() const { return step_ppqn_; }

    // ─── MIDI input ───────────────────────────────────────────

    /// Handle a MIDI note on/off event.
    void on_midi_note(uint8_t note, uint8_t velocity, bool note_on);

    /// Advance the step position manually (for external step sequencer).
    void advance_step();

    /// Get the next position where a note will be placed.
    uint64_t next_position() const { return position_; }

    /// Set the next position (e.g. from playhead).
    void set_next_position(uint64_t pos) { position_ = pos; }

    // ─── Pending notes ────────────────────────────────────────

    /// Return the currently-pending (held) note, if any.
    std::optional<Note> pending_note() const { return held_note_; }

    /// Clear the held note without placing it.
    void clear_pending() { held_note_.reset(); }

    // ─── Processing ───────────────────────────────────────────

    /// Process a MIDI event and add notes to the collection.
    void process_midi(const MidiEvent& event, NoteCollection& notes);

private:
    bool        enabled_    = false;
    InputMode   mode_       = Step;
    uint32_t    step_ppqn_  = 240;       // 16th note default
    uint64_t    position_   = 0;

    // The note currently being held (waiting for note-off in Step mode).
    std::optional<Note> held_note_;
};

} // namespace aria

#endif // ARIA_PIANOROLL_STEP_INPUT_H
