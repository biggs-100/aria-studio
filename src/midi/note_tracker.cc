#include "note_tracker.h"
#include <algorithm>

namespace aria {

void NoteTracker::note_on(uint8_t note, uint8_t channel, uint8_t velocity,
                           uint64_t sample_pos) {
    uint16_t key = make_key(note, channel);

    NoteState state;
    state.note            = note;
    state.channel         = channel;
    state.velocity        = velocity;
    state.release_velocity= 0;
    state.start_sample    = sample_pos;
    state.duration        = 0;
    state.active          = true;

    active_notes_[key] = state;

    // Rebuild active list
    active_note_list_.clear();
    active_note_list_.reserve(active_notes_.size());
    for (const auto& [_, ns] : active_notes_) {
        if (ns.active) {
            active_note_list_.push_back(ns);
        }
    }
}

void NoteTracker::note_off(uint8_t note, uint8_t channel, uint64_t sample_pos) {
    uint16_t key = make_key(note, channel);
    auto it = active_notes_.find(key);
    if (it == active_notes_.end()) return;

    if (sustain_pedal_) {
        // Pedal is held — sustain the note until pedal release
        SustainedNote sn;
        sn.note             = note;
        sn.channel          = channel;
        sn.velocity         = it->second.velocity;
        sn.release_velocity = 0;
        sn.note_off_sample  = sample_pos;
        sustained_notes_.push_back(sn);

        it->second.duration = sample_pos - it->second.start_sample;
        it->second.active   = false;
    } else {
        it->second.duration = sample_pos - it->second.start_sample;
        it->second.active   = false;
        active_notes_.erase(it);
    }

    // Rebuild active list
    active_note_list_.clear();
    active_note_list_.reserve(active_notes_.size());
    for (const auto& [_, ns] : active_notes_) {
        if (ns.active) {
            active_note_list_.push_back(ns);
        }
    }
}

void NoteTracker::all_notes_off() {
    active_notes_.clear();
    sustained_notes_.clear();
    active_note_list_.clear();
}

void NoteTracker::sustain(bool pedal_down) {
    sustain_pedal_ = pedal_down;

    if (!pedal_down && !sustained_notes_.empty()) {
        // Release all sustained notes
        for (const auto& sn : sustained_notes_) {
            uint16_t key = make_key(sn.note, sn.channel);
            active_notes_.erase(key);
        }
        sustained_notes_.clear();

        // Rebuild active list
        active_note_list_.clear();
        active_note_list_.reserve(active_notes_.size());
        for (const auto& [_, ns] : active_notes_) {
            if (ns.active) {
                active_note_list_.push_back(ns);
            }
        }
    }
}

bool NoteTracker::is_note_active(uint8_t note, uint8_t channel) const {
    uint16_t key = make_key(note, channel);
    auto it = active_notes_.find(key);
    return it != active_notes_.end() && it->second.active;
}

uint32_t NoteTracker::active_note_count() const {
    return static_cast<uint32_t>(active_note_list_.size());
}

const NoteState* NoteTracker::active_note(uint8_t note, uint8_t channel) const {
    uint16_t key = make_key(note, channel);
    auto it = active_notes_.find(key);
    if (it != active_notes_.end() && it->second.active) {
        return &it->second;
    }
    return nullptr;
}

} // namespace aria
