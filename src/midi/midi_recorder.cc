#include "midi_recorder.h"
#include <algorithm>
#include <cmath>

namespace aria {

// ─── Lifecycle ──────────────────────────────────────────────────

void MidiRecorder::start_recording(uint32_t ppqn_position) {
    reset();
    recording_ = true;
    start_ppqn_ = ppqn_position;
}

void MidiRecorder::stop_recording() {
    // Close any pending notes (note-off may not arrive)
    if (recording_) {
        for (auto& [key, pending] : pending_notes_) {
            MidiNote note;
            note.note          = pending.note;
            note.channel       = pending.channel;
            note.velocity      = pending.velocity;
            note.start_ppqn    = pending.start_ppqn;
            note.duration_ppqn = 0; // Zero-length — closed by stop
            note.mpe           = pending.mpe;
            note.note_events   = std::move(pending.cc_events);
            completed_notes_.push_back(note);
        }
        pending_notes_.clear();
    }
    recording_ = false;
}

void MidiRecorder::record_event(const MidiEvent& event, uint64_t sample_position,
                                double sample_rate) {
    if (!recording_) return;

    // Store raw event in buffer
    event_buffer_.push_back(event);

    // Route by message type
    switch (event.type) {
        case MidiMessageType::NoteOn:
            handle_note_on(event, sample_position, sample_rate);
            break;
        case MidiMessageType::NoteOff:
            handle_note_off(event, sample_position, sample_rate);
            break;
        case MidiMessageType::ControlChange:
            handle_cc(event);
            break;
        case MidiMessageType::PolyphonicKey:
            // Poly aftertouch → update MPE pressure for the specific note
            if (pending_notes_.count(note_key(event.data1, event.channel))) {
                auto& pending = pending_notes_[note_key(event.data1, event.channel)];
                pending.mpe.pressure = event.data2;
                pending.cc_events.push_back(event);
            }
            break;
        case MidiMessageType::ChannelAftertouch:
            // Channel aftertouch → update all active notes on this channel
            for (auto& [key, pending] : pending_notes_) {
                if (pending.channel == event.channel) {
                    pending.mpe.pressure = event.data1;
                    pending.cc_events.push_back(event);
                }
            }
            break;
        case MidiMessageType::PitchBend:
            // Pitch bend → update all active notes on this channel
            for (auto& [key, pending] : pending_notes_) {
                if (pending.channel == event.channel) {
                    int16_t bend = static_cast<int16_t>(
                        static_cast<int>(event.data1) |
                        (static_cast<int>(event.data2) << 7)) - 8192;
                    pending.mpe.pitch_bend = bend;
                    pending.cc_events.push_back(event);
                }
            }
            break;
        default:
            break;
    }
}

MidiClip MidiRecorder::finalize_clip() {
    // Ensure recording is stopped
    if (recording_) {
        stop_recording();
    }

    MidiClip clip;
    clip.length_ppqn = 0;

    // Calculate clip length from the last note or event
    if (!completed_notes_.empty()) {
        auto max_end = std::max_element(completed_notes_.begin(),
            completed_notes_.end(), [](const MidiNote& a, const MidiNote& b) {
                return (a.start_ppqn + a.duration_ppqn) <
                       (b.start_ppqn + b.duration_ppqn);
            });
        uint32_t end = max_end->start_ppqn + max_end->duration_ppqn;
        // Round up to nearest bar (4 beats = 4 * 480 = 1920 PPQN at default grid)
        clip.length_ppqn = ((end / 1920) + 1) * 1920;
    } else if (!clip_events_.empty()) {
        auto max_ev = std::max_element(clip_events_.begin(), clip_events_.end(),
            [](const MidiEvent& a, const MidiEvent& b) {
                return a.ppqn_position < b.ppqn_position;
            });
        clip.length_ppqn = ((max_ev->ppqn_position / 1920) + 1) * 1920;
    } else {
        clip.length_ppqn = 1920; // Minimum: 1 bar
    }

    if (clip.length_ppqn < 1920) clip.length_ppqn = 1920;

    clip.loop_start_ppqn = 0;
    clip.loop_end_ppqn   = clip.length_ppqn;

    // Transfer notes
    for (auto& note : completed_notes_) {
        clip.add_note(note);
    }

    // Transfer clip-level events
    for (auto& ev : clip_events_) {
        clip.add_event(ev);
    }

    // Reset state
    reset();

    return clip;
}

// ─── Quantize-on-record ─────────────────────────────────────────

void MidiRecorder::set_quantize_on_record(bool enabled, uint32_t grid_ppqn) {
    quantize_enabled_ = enabled;
    grid_ppqn_ = (grid_ppqn > 0) ? grid_ppqn : 480;
}

// ─── State ──────────────────────────────────────────────────────

void MidiRecorder::reset() {
    recording_ = false;
    start_ppqn_ = 0;
    event_buffer_.clear();
    pending_notes_.clear();
    completed_notes_.clear();
    clip_events_.clear();
}

// ─── Event handlers ─────────────────────────────────────────────

void MidiRecorder::handle_note_on(const MidiEvent& event, uint64_t sample_position,
                                  double sample_rate) {
    // Note-on with velocity 0 is equivalent to note-off
    if (event.data2 == 0) {
        handle_note_off(event, sample_position, sample_rate);
        return;
    }

    uint16_t key = note_key(event.data1, event.channel);

    // If the note is already pending, close it first (retrigger)
    if (pending_notes_.count(key)) {
        // Close the existing note
        auto& existing = pending_notes_[key];
        MidiNote completed;
        completed.note          = existing.note;
        completed.channel       = existing.channel;
        completed.velocity      = existing.velocity;
        completed.start_ppqn    = existing.start_ppqn;
        completed.duration_ppqn = start_ppqn_ - existing.start_ppqn;
        completed.mpe           = existing.mpe;
        completed.note_events   = std::move(existing.cc_events);

        if (quantize_enabled_ && grid_ppqn_ > 0) {
            uint32_t grid = grid_ppqn_;
            uint32_t grid_start = (completed.start_ppqn / grid) * grid;
            uint32_t grid_end   = grid_start + grid;
            completed.start_ppqn = (completed.start_ppqn - grid_start <
                                    grid_end - completed.start_ppqn)
                                       ? grid_start : grid_end;
        }

        completed_notes_.push_back(completed);
    }

    // Calculate PPQN position from sample position
    double seconds = sample_position / sample_rate;
    double beats   = seconds * (120.0 / 60.0); // Assuming 120 BPM default
    uint32_t ppqn  = start_ppqn_ + static_cast<uint32_t>(beats * 480.0);

    if (quantize_enabled_) {
        // Snap to grid
        uint32_t grid_start = (ppqn / grid_ppqn_) * grid_ppqn_;
        uint32_t grid_end   = grid_start + grid_ppqn_;
        ppqn = (ppqn - grid_start < grid_end - ppqn) ? grid_start : grid_end;
    }

    PendingNote pending;
    pending.note       = event.data1;
    pending.channel    = event.channel;
    pending.velocity   = event.data2;
    pending.start_us   = event.timestamp_us;
    pending.start_ppqn = ppqn;
    pending_notes_[key] = pending;
}

void MidiRecorder::handle_note_off(const MidiEvent& event, uint64_t sample_position,
                                   double sample_rate) {
    uint16_t key = note_key(event.data1, event.channel);

    auto it = pending_notes_.find(key);
    if (it == pending_notes_.end()) return;

    // Calculate end PPQN position
    double seconds = sample_position / sample_rate;
    double beats   = seconds * (120.0 / 60.0);
    uint32_t end_ppqn = start_ppqn_ + static_cast<uint32_t>(beats * 480.0);

    if (quantize_enabled_) {
        uint32_t grid_start = (end_ppqn / grid_ppqn_) * grid_ppqn_;
        uint32_t grid_end   = grid_start + grid_ppqn_;
        end_ppqn = (end_ppqn - grid_start < grid_end - end_ppqn) ? grid_start : grid_end;
    }

    MidiNote completed;
    completed.note             = it->second.note;
    completed.channel          = it->second.channel;
    completed.velocity         = it->second.velocity;
    completed.release_velocity = event.data2;
    completed.start_ppqn       = it->second.start_ppqn;

    int64_t duration = static_cast<int64_t>(end_ppqn) -
                       static_cast<int64_t>(it->second.start_ppqn);
    completed.duration_ppqn = static_cast<uint32_t>(std::max<int64_t>(duration, 48)); // min 1 tick

    completed.mpe         = it->second.mpe;
    completed.note_events = std::move(it->second.cc_events);
    completed_notes_.push_back(completed);

    pending_notes_.erase(it);
}

void MidiRecorder::handle_cc(const MidiEvent& event) {
    // CC 64 = sustain pedal — handled by NoteTracker, record as clip event
    clip_events_.push_back(event);

    // If we have pending notes on this channel, associate the CC
    for (auto& [key, pending] : pending_notes_) {
        if (pending.channel == event.channel) {
            pending.cc_events.push_back(event);
        }
    }
}

} // namespace aria
