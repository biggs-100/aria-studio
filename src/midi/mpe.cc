#include "mpe.h"
#include <algorithm>
#include <cmath>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// MPEDecoder
// ═══════════════════════════════════════════════════════════════

MPEDecoder::MPEDecoder() {
    reset();
}

void MPEDecoder::process(const MidiEvent& event) {
    uint8_t chan = event.channel;
    MPEMessageType msg_type = classify(event);

    switch (msg_type) {
        case MPEMessageType::MasterPitchBend:
            // Master channel pitch bend — global pitch bend for all zones.
            // Store on master channel expression.
            channel_expr_[MPEConstants::kMasterChannel].pitch_bend =
                static_cast<int16_t>(
                    static_cast<int>(event.data1) |
                    (static_cast<int>(event.data2) << 7)) - 8192;
            break;

        case MPEMessageType::MasterControlChange:
            // Master channel CC — global. Store on master channel.
            if (event.data1 == MPEConstants::kCC_Timbre) {
                channel_expr_[MPEConstants::kMasterChannel].timbre = event.data2;
            }
            break;

        case MPEMessageType::ZonePitchBend: {
            // Per-note pitch bend on a member channel.
            int16_t bend = static_cast<int16_t>(
                static_cast<int>(event.data1) |
                (static_cast<int>(event.data2) << 7)) - 8192;

            uint8_t note = channel_to_note_[chan];
            if (note < 128) {
                note_expr_[note].pitch_bend = bend;
            }
            channel_expr_[chan].pitch_bend = bend;
            break;
        }

        case MPEMessageType::ZoneTimbre: {
            // CC 74 — per-note timbre.
            uint8_t note = channel_to_note_[chan];
            if (note < 128) {
                note_expr_[note].timbre = event.data2;
            }
            channel_expr_[chan].timbre = event.data2;
            break;
        }

        case MPEMessageType::ZonePressure: {
            // Channel aftertouch — per-note pressure.
            uint8_t note = channel_to_note_[chan];
            if (note < 128) {
                note_expr_[note].pressure = event.data1;
            }
            channel_expr_[chan].pressure = event.data1;
            break;
        }

        case MPEMessageType::ZoneNoteOn:
            // Note-on on a member channel — assign channel → note mapping.
            if (event.data2 > 0) {
                assign_channel_to_note(chan, event.data1);
            } else {
                // velocity 0 = note-off
                release_channel(chan);
            }
            break;

        case MPEMessageType::ZoneNoteOff:
            // Note-off on member channel — release channel mapping.
            release_channel(chan);
            break;

        case MPEMessageType::NonMPE:
        default:
            break;
    }

    // Try to detect MPE configuration from the event stream
    detect_mpe_configuration(event);
}

MPEExpression MPEDecoder::get_note_expression(uint8_t note) const {
    if (note < 128) {
        return note_expr_[note];
    }
    return MPEExpression{};
}

MPEExpression MPEDecoder::get_channel_expression(uint8_t channel) const {
    if (channel < 16) {
        return channel_expr_[channel];
    }
    return MPEExpression{};
}

void MPEDecoder::reset() {
    for (auto& mapping : channel_to_note_) {
        mapping = 0xFF; // 0xFF = unmapped
    }
    for (auto& expr : note_expr_) {
        expr = MPEExpression{};
    }
    for (auto& expr : channel_expr_) {
        expr = MPEExpression{};
    }
    for (auto& member : is_member_channel_) {
        member = false;
    }
    mpe_configured_ = false;
    lower_zone_channels_ = 0;
    upper_zone_channels_ = 0;
    member_channel_count_ = 0;
    lower_zone_end_ = 0;
    upper_zone_start_ = 0;
}

MPEMessageType MPEDecoder::classify(const MidiEvent& event) const {
    uint8_t chan = event.channel;

    // Master channel
    if (chan == MPEConstants::kMasterChannel) {
        if (event.type == MidiMessageType::PitchBend) {
            return MPEMessageType::MasterPitchBend;
        }
        if (event.type == MidiMessageType::ControlChange) {
            return MPEMessageType::MasterControlChange;
        }
        return MPEMessageType::NonMPE;
    }

    // Check if this is a recognized member channel
    if (chan > MPEConstants::kUpperZoneEnd) {
        return MPEMessageType::NonMPE;
    }

    // Member channel events
    switch (event.type) {
        case MidiMessageType::NoteOn:
        case MidiMessageType::NoteOff:
            return (event.type == MidiMessageType::NoteOn)
                       ? MPEMessageType::ZoneNoteOn
                       : MPEMessageType::ZoneNoteOff;
        case MidiMessageType::PitchBend:
            return MPEMessageType::ZonePitchBend;
        case MidiMessageType::ControlChange:
            if (event.data1 == MPEConstants::kCC_Timbre) {
                return MPEMessageType::ZoneTimbre;
            }
            return MPEMessageType::NonMPE;
        case MidiMessageType::ChannelAftertouch:
            return MPEMessageType::ZonePressure;
        default:
            return MPEMessageType::NonMPE;
    }
}

void MPEDecoder::assign_channel_to_note(uint8_t channel, uint8_t note) {
    // Release previous note on this channel if any
    if (channel_to_note_[channel] < 128) {
        uint8_t old_note = channel_to_note_[channel];
        note_expr_[old_note] = MPEExpression{};
    }

    channel_to_note_[channel] = note;
    is_member_channel_[channel] = true;
}

void MPEDecoder::release_channel(uint8_t channel) {
    if (channel < 16 && channel_to_note_[channel] < 128) {
        [[maybe_unused]] uint8_t note = channel_to_note_[channel];
        channel_to_note_[channel] = 0xFF;
        // Don't clear note_expr_ — let get_note_expression() still report
        // the last known expression for this note.
    }
}

void MPEDecoder::detect_mpe_configuration(const MidiEvent& event) {
    if (mpe_configured_) return;

    uint8_t chan = event.channel;

    // MPE configuration is detected when we see events on channels
    // beyond the master channel that form a zone pattern.
    if (chan > MPEConstants::kMasterChannel &&
        chan <= MPEConstants::kUpperZoneEnd &&
        (event.type == MidiMessageType::NoteOn ||
         event.type == MidiMessageType::NoteOff)) {

        // Detect lower zone: channels 2-8
        if (chan >= MPEConstants::kLowerZoneStart &&
            chan <= MPEConstants::kLowerZoneStart + 6) {
            is_member_channel_[chan] = true;
            lower_zone_channels_++;
        }
        // Upper zone: channels 9-15 (index 8-14)
        if (chan >= MPEConstants::kLowerZoneStart + 7) {
            is_member_channel_[chan] = true;
            upper_zone_channels_++;
        }

        member_channel_count_ = lower_zone_channels_ + upper_zone_channels_;
        lower_zone_end_ = MPEConstants::kLowerZoneStart + lower_zone_channels_ - 1;
        upper_zone_start_ = MPEConstants::kLowerZoneStart + 7;
        mpe_configured_ = true;
    }
}

uint8_t MPEDecoder::note_to_channel(uint8_t note) const {
    for (uint8_t ch = 0; ch < 16; ++ch) {
        if (channel_to_note_[ch] == note) {
            return ch;
        }
    }
    return 0xFF;
}

// ═══════════════════════════════════════════════════════════════
// MPEEncoder
// ═══════════════════════════════════════════════════════════════

void MPEEncoder::set_note_expression(uint8_t note, const MPEExpression& expr) {
    if (note >= 128) return;

    pending_expr_[note] = expr;
    has_pending_[note] = true;
    note_expr_[note] = expr;
}

MPEExpression MPEEncoder::get_note_expression(uint8_t note) const {
    if (note < 128) {
        return note_expr_[note];
    }
    return MPEExpression{};
}

std::vector<MidiEvent> MPEEncoder::generate_events() {
    std::vector<MidiEvent> events;

    for (uint8_t note = 0; note < 128; ++note) {
        if (!has_pending_[note]) continue;
        has_pending_[note] = false;

        const MPEExpression& current = note_expr_[note];
        const MPEExpression& prev    = pending_expr_[note];
        uint8_t channel = note_to_channel_[note];

        // If no channel assigned yet, get one
        if (channel == kUnmapped) {
            channel = assign_channel(note);
        }

        // Generate pitch bend if changed
        if (current.pitch_bend != 0) {
            MidiEvent pb;
            pb.type    = MidiMessageType::PitchBend;
            pb.channel = channel;
            uint16_t packed = pack_pitch_bend(current.pitch_bend);
            pb.data1 = static_cast<uint8_t>(packed & 0x7F);
            pb.data2 = static_cast<uint8_t>((packed >> 7) & 0x7F);
            events.push_back(pb);
        }

        // Generate timbre (CC 74) if changed
        if (current.timbre != prev.timbre) {
            MidiEvent cc74;
            cc74.type    = MidiMessageType::ControlChange;
            cc74.channel = channel;
            cc74.data1   = MPEConstants::kCC_Timbre;
            cc74.data2   = current.timbre;
            events.push_back(cc74);
        }

        // Generate channel pressure if changed
        if (current.pressure != prev.pressure) {
            MidiEvent at;
            at.type    = MidiMessageType::ChannelAftertouch;
            at.channel = channel;
            at.data1   = current.pressure;
            events.push_back(at);
        }
    }

    return events;
}

void MPEEncoder::reset() {
    for (auto& expr : note_expr_) {
        expr = MPEExpression{};
    }
    for (auto& pending : has_pending_) {
        pending = false;
    }
    for (auto& ch : note_to_channel_) {
        ch = kUnmapped;
    }
    for (auto& note : channel_to_note_) {
        note = 0;
    }
    next_channel_ = MPEConstants::kLowerZoneStart;
    member_count_ = 0;
}

uint8_t MPEEncoder::note_channel(uint8_t note) const {
    if (note >= 128) return kUnmapped;
    return note_to_channel_[note];
}

uint8_t MPEEncoder::assign_channel(uint8_t note) {
    uint8_t channel = next_channel_;

    note_to_channel_[note] = channel;
    channel_to_note_[channel] = note;

    // Advance to next available channel
    next_channel_++;
    if (next_channel_ > MPEConstants::kUpperZoneEnd) {
        next_channel_ = MPEConstants::kLowerZoneStart;
    }
    member_count_++;

    return channel;
}

uint16_t MPEEncoder::pack_pitch_bend(int16_t bend) const {
    // Clamp to valid range [-8192, 8191]
    int32_t clamped = std::clamp<int32_t>(bend, -8192, 8191);

    // Convert to 14-bit unsigned: 0 = -8192, 8192 = center, 16383 = +8191
    uint32_t unsigned_bend = static_cast<uint32_t>(clamped + 8192);

    // Pack as MSB/LSB (7 bits each)
    return static_cast<uint16_t>(
        ((unsigned_bend & 0x3F80) << 1) | // MSB → data2 (bits 13-7)
        (unsigned_bend & 0x007F));         // LSB → data1 (bits 6-0)
}

} // namespace aria
