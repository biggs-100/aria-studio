#ifndef ARIA_MIDI_TYPES_H
#define ARIA_MIDI_TYPES_H

#include <cstdint>
#include <vector>

namespace aria {

// ─── MidiMessageType ──────────────────────────────────────────

/// Standard MIDI 1.0 message types.
enum class MidiMessageType : uint8_t {
    NoteOff           = 0x80,
    NoteOn            = 0x90,
    PolyphonicKey     = 0xA0,  // Polyphonic Aftertouch
    ControlChange     = 0xB0,
    ProgramChange     = 0xC0,
    ChannelAftertouch = 0xD0,
    PitchBend         = 0xE0,
    SysEx             = 0xF0,
    MidiClock         = 0xF8,
    MidiStart         = 0xFA,
    MidiContinue      = 0xFB,
    MidiStop          = 0xFC,
    ActiveSensing     = 0xFE,
    Meta              = 0xFF   // Internal meta-events
};

// ─── MPEExpression ────────────────────────────────────────────

/// Per-note MPE expression data.
struct MPEExpression {
    int16_t pitch_bend = 0;   // -8192 to 8191
    uint8_t timbre     = 64;  // CC 74 (0-127), default 64 (center)
    uint8_t pressure   = 0;   // Channel pressure (0-127)

    bool has_data() const {
        return pitch_bend != 0 || timbre != 64 || pressure != 0;
    }
};

// ─── MidiEvent ────────────────────────────────────────────────

/// A single MIDI event with timestamp and position information.
struct MidiEvent {
    MidiMessageType type          = MidiMessageType::NoteOff;
    uint8_t         channel       = 0;   // 0-15
    uint8_t         data1         = 0;   // Note number, CC number, etc.
    uint8_t         data2         = 0;   // Velocity, CC value, etc.
    std::vector<uint8_t> sysex_data;     // For SysEx messages
    uint64_t        timestamp_us  = 0;   // Microsecond timestamp (recording)
    uint32_t        ppqn_position = 0;   // PPQN position (sequencing)

    // ─── Utility methods ─────────────────────────────────────
    bool is_note() const {
        return type == MidiMessageType::NoteOn ||
               type == MidiMessageType::NoteOff;
    }

    bool is_cc() const {
        return type == MidiMessageType::ControlChange;
    }

    bool is_channel_message() const {
        uint8_t raw = static_cast<uint8_t>(type);
        return raw >= 0x80 && raw < 0xF0;
    }
};

// ─── MidiNote (for clip storage) ──────────────────────────────

/// A note inside a MidiClip, stored with PPQN timing and MPE data.
struct MidiNote {
    uint32_t start_ppqn    = 0;
    uint32_t duration_ppqn = 0;
    uint8_t  note          = 0;     // MIDI note number (0-127)
    uint8_t  velocity      = 0;     // 0-127
    uint8_t  channel       = 0;     // 0-15
    uint8_t  release_velocity = 0;
    MPEExpression mpe;
    std::vector<MidiEvent> note_events;  // CC events within this note
};

// ─── NoteState (for NoteTracker) ──────────────────────────────

/// Runtime state for an active (currently-sounding) note.
struct NoteState {
    uint8_t  note            = 0;
    uint8_t  channel         = 0;
    uint8_t  velocity        = 0;
    uint8_t  release_velocity = 0;
    uint64_t start_sample    = 0;
    uint64_t duration        = 0;
    bool     active          = false;
    MPEExpression mpe;
};

} // namespace aria

#endif // ARIA_MIDI_TYPES_H
