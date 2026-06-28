#ifndef ARIA_MPE_H
#define ARIA_MPE_H

#include "midi_types.h"
#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>

namespace aria {

// ─── MPE Constants ──────────────────────────────────────────────

/// MPE convention constants per the MIDI MPE specification.
struct MPEConstants {
    /// Master channel is always channel 1 (index 0).
    static constexpr uint8_t kMasterChannel = 0;

    /// Lower zone starts on channel 2 (index 1).
    static constexpr uint8_t kLowerZoneStart = 1;

    /// Upper zone ends on channel 15 (index 14) — 14 channels.
    static constexpr uint8_t kUpperZoneEnd = 14;

    /// CC 74 = Timbre (filter cutoff, brightness).
    static constexpr uint8_t kCC_Timbre = 74;

    /// CC 7 = Volume (per-note in MPE).
    static constexpr uint8_t kCC_Volume = 7;

    /// CC 11 = Expression (per-note in MPE).
    static constexpr uint8_t kCC_Expression = 11;
};

// ─── MPEMessageType ─────────────────────────────────────────────

/// Classifies a MIDI event in the context of MPE.
enum class MPEMessageType : uint8_t {
    MasterPitchBend,         ///< Master channel pitch bend (all zones)
    MasterControlChange,     ///< Master channel CC (global)
    ZonePitchBend,           ///< Per-note pitch bend on member channel
    ZoneTimbre,              ///< CC 74 — per-note timbre
    ZonePressure,            ///< Channel aftertouch — per-note pressure
    ZoneNoteOn,              ///< Note-on on member channel
    ZoneNoteOff,             ///< Note-off on member channel
    NonMPE                   ///< Standard MIDI, no MPE semantics
};

// ─── MPEDecoder ─────────────────────────────────────────────────

/// MPE decoder — converts MIDI input stream into per-note expressions.
///
/// Accepts raw MIDI events and decodes MPE semantics based on channel
/// assignments. Maintains a mapping from MPE member channels to notes
/// and accumulates per-note expression data (pitch bend, timbre, pressure).
class MPEDecoder {
public:
    MPEDecoder();

    /// Process a single MIDI event through the MPE decoder.
    void process(const MidiEvent& event);

    /// Get the accumulated MPE expression for a given note number.
    /// Returns default expression if the note has no MPE data.
    MPEExpression get_note_expression(uint8_t note) const;

    /// Get the MPE expression on a specific member channel.
    MPEExpression get_channel_expression(uint8_t channel) const;

    /// Reset all MPE state (channel mappings, expressions).
    void reset();

    /// Check if the decoder has seen MPE zone configuration.
    bool has_mpe_configuration() const { return mpe_configured_; }

    /// Get the current member channel count (lower + upper).
    uint8_t member_channel_count() const { return member_channel_count_; }

    /// Classify an event's MPE message type.
    MPEMessageType classify(const MidiEvent& event) const;

private:
    // Channel → note mapping for member channels.
    std::array<uint8_t, 16> channel_to_note_;

    // Per-note expression data.
    std::array<MPEExpression, 128> note_expr_;

    // Per-channel expression data (for member channels without note mapping).
    std::array<MPEExpression, 16> channel_expr_;

    // Track which channels are member channels.
    std::array<bool, 16> is_member_channel_{};

    // Zone configuration (detected from incoming data).
    bool    mpe_configured_ = false;
    uint8_t lower_zone_channels_ = 0;
    uint8_t upper_zone_channels_ = 0;
    uint8_t member_channel_count_ = 0;
    uint8_t lower_zone_end_ = 0;
    uint8_t upper_zone_start_ = 0;

    // Set note mapping for a member channel.
    void assign_channel_to_note(uint8_t channel, uint8_t note);

    // Clear note mapping for a member channel.
    void release_channel(uint8_t channel);

    // Detect MPE configuration from incoming events.
    void detect_mpe_configuration(const MidiEvent& event);

    // Internal: get effective channel for a note (0-15, 0xFF if unmapped).
    uint8_t note_to_channel(uint8_t note) const;
};

// ─── MPEEncoder ─────────────────────────────────────────────────

/// MPE encoder — converts per-note expressions into MIDI events.
///
/// Takes per-note MPE data and generates the appropriate MIDI events
/// according to the MPE specification, assigning member channels as
/// needed and generating pitch bend / CC / aftertouch messages.
class MPEEncoder {
public:
    MPEEncoder() = default;

    /// Set per-note expression for a specific note.
    /// Expression changes are batched until generate_events() is called.
    void set_note_expression(uint8_t note, const MPEExpression& expr);

    /// Get the current expression for a note.
    MPEExpression get_note_expression(uint8_t note) const;

    /// Generate all pending MPE events as MIDI events.
    /// Clears the pending change list.
    std::vector<MidiEvent> generate_events();

    /// Reset encoder state (channel assignments, expressions).
    void reset();

    /// Get the member channel assigned to a note (0xFF if unmapped).
    uint8_t note_channel(uint8_t note) const;

private:
    // Per-note expression (accumulated state).
    std::array<MPEExpression, 128> note_expr_;

    // Per-note pending expression (changes since last generate).
    std::array<MPEExpression, 128> pending_expr_;
    std::array<bool, 128> has_pending_{};

    // Note → channel mapping
    std::array<uint8_t, 128> note_to_channel_;
    static constexpr uint8_t kUnmapped = 0xFF;

    // Channel → note reverse mapping
    std::array<uint8_t, 16> channel_to_note_;

    // Next available member channel
    uint8_t next_channel_ = MPEConstants::kLowerZoneStart;
    uint8_t member_count_ = 0;

    // Assign a member channel to a note.
    uint8_t assign_channel(uint8_t note);

    uint16_t pack_pitch_bend(int16_t bend) const;
};

} // namespace aria

#endif // ARIA_MPE_H
