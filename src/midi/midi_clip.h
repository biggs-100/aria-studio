#ifndef ARIA_MIDI_CLIP_H
#define ARIA_MIDI_CLIP_H

#include "model/clip.h"
#include "midi_types.h"
#include <cstdint>
#include <vector>

namespace aria {

/// A MIDI clip containing notes and events, used for sequencing and storage.
///
/// Inherits from Clip (position, length, looping, fade, gain, mute).
/// Adds note data, quantization, humanize, transpose, and serialization.
class MidiClip : public Clip {
public:
    MidiClip() = default;

    // ─── Notes ───────────────────────────────────────────────
    void add_note(const MidiNote& note);
    void remove_note(uint32_t index);
    void clear();

    const std::vector<MidiNote>& notes() const { return notes_; }
    std::vector<MidiNote>& notes() { return notes_; }

    // ─── Events (CC, pitch bend, etc.) ───────────────────────
    void add_event(const MidiEvent& event);
    const std::vector<MidiEvent>& events() const { return events_; }
    std::vector<MidiEvent>& events() { return events_; }

    // ─── Quantization ────────────────────────────────────────
    void quantize(uint32_t grid_ppqn, double strength);
    void humanize(double timing_amount, double velocity_amount);

    // ─── Transpose ───────────────────────────────────────────
    void transpose(int8_t semitones);

    // ─── Serialization ───────────────────────────────────────
    std::vector<uint8_t> serialize() const;
    static MidiClip deserialize(const std::vector<uint8_t>& data);

private:
    std::vector<MidiNote> notes_;
    std::vector<MidiEvent> events_;
};

} // namespace aria

#endif // ARIA_MIDI_CLIP_H
