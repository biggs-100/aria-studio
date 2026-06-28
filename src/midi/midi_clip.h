#ifndef ARIA_MIDI_CLIP_H
#define ARIA_MIDI_CLIP_H

#include "midi_types.h"
#include <cstdint>
#include <vector>

namespace aria {

/// A MIDI clip containing notes and events, used for sequencing and storage.
///
/// Clips store notes with PPQN timing resolution and support quantize,
/// humanize, transpose, and serialization operations.
class MidiClip {
public:
    // ─── Metadata ────────────────────────────────────────────
    uint32_t length_ppqn     = 0;
    uint32_t loop_start_ppqn = 0;
    uint32_t loop_end_ppqn   = 0;

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
