#include "step_input.h"

#include <algorithm>
#include <cstdint>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════

void MidiStepInput::set_enabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled) {
        held_note_.reset();
    }
}

void MidiStepInput::set_mode(InputMode mode) {
    mode_ = mode;
    if (mode != Step) {
        held_note_.reset();
    }
}

// ═══════════════════════════════════════════════════════════════
// MIDI note handling
// ═══════════════════════════════════════════════════════════════

void MidiStepInput::on_midi_note(uint8_t note, uint8_t velocity, bool note_on) {
    if (!enabled_) return;

    if (note_on && mode_ == Step) {
        // If a previous note was held and not released, finalize it.
        if (held_note_.has_value()) {
            held_note_->duration_ppqn = position_ - held_note_->start_ppqn;
            if (held_note_->duration_ppqn < 1) {
                held_note_->duration_ppqn = step_ppqn_;
            }
        }

        // Start a new held note at the current position.
        Note n;
        n.start_ppqn    = position_;
        n.duration_ppqn = step_ppqn_;
        n.key           = note;
        n.velocity      = velocity;
        n.channel       = 0;
        held_note_      = n;

        // Advance the step position for next note.
        advance_step();

    } else if (note_on && mode_ == RealTime) {
        // In RealTime mode, notes are recorded with actual timing.
        // The held note stores the start; duration is determined on note-off.
        Note n;
        n.start_ppqn    = position_;
        n.duration_ppqn = step_ppqn_;  // placeholder until note-off
        n.key           = note;
        n.velocity      = velocity;
        n.channel       = 0;
        held_note_      = n;

    } else if (!note_on) {
        // Note-off: finalize the held note duration.
        if (held_note_.has_value() && held_note_->key == note) {
            if (mode_ == RealTime) {
                // Compute actual duration based on the step position advance.
                held_note_->duration_ppqn = position_ - held_note_->start_ppqn;
                if (held_note_->duration_ppqn < 1) {
                    held_note_->duration_ppqn = step_ppqn_;
                }
            }
            // In Step mode, the note is already finalized on the next note-on.
        }
    }
}

void MidiStepInput::advance_step() {
    if (enabled_ && mode_ == Step) {
        position_ += step_ppqn_;
    }
}

// ═══════════════════════════════════════════════════════════════
// MIDI event processing
// ═══════════════════════════════════════════════════════════════

void MidiStepInput::process_midi(const MidiEvent& event, NoteCollection& notes) {
    if (!enabled_) return;

    if (event.type == MidiMessageType::NoteOn && event.data2 > 0) {
        // Note-on with velocity > 0.
        on_midi_note(event.data1, event.data2, true);

        // In RealTime mode, add the note immediately on note-on.
        // The duration will be updated on note-off.
        if (held_note_.has_value()) {
            // Clone held note — the original stays for duration update.
            Note n = *held_note_;
            // Assign a proper ID and add to collection.
            NoteID id = notes.add(n);
            if (id.value != 0) {
                // Update the held_note_ with the assigned ID.
                held_note_->id = id;
            }
        }

    } else if (event.type == MidiMessageType::NoteOff ||
               (event.type == MidiMessageType::NoteOn && event.data2 == 0)) {
        // Note-off or note-on with velocity 0.
        on_midi_note(event.data1, event.data2, false);

        // Update the note in the collection with the correct duration.
        if (held_note_.has_value() && held_note_->id.value != 0) {
            // If we have a valid ID, update the duration in the collection.
            Note* existing = notes.find(held_note_->id);
            if (existing) {
                existing->duration_ppqn = held_note_->duration_ppqn;
            }
            // Clear the held note after processing.
            held_note_.reset();
        }
    }
}

} // namespace aria
