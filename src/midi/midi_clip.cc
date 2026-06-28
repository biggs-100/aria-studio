#include "midi_clip.h"
#include <algorithm>
#include <cstring>
#include <random>

namespace aria {

// ─── Notes ─────────────────────────────────────────────────────

void MidiClip::add_note(const MidiNote& note) {
    notes_.push_back(note);
}

void MidiClip::remove_note(uint32_t index) {
    if (index < notes_.size()) {
        notes_.erase(notes_.begin() + static_cast<ptrdiff_t>(index));
    }
}

void MidiClip::clear() {
    notes_.clear();
    events_.clear();
}

void MidiClip::add_event(const MidiEvent& event) {
    events_.push_back(event);
}

// ─── Quantization ──────────────────────────────────────────────

void MidiClip::quantize(uint32_t grid_ppqn, double strength) {
    if (grid_ppqn == 0) return;
    strength = std::clamp(strength, 0.0, 1.0);

    for (auto& note : notes_) {
        uint32_t grid_start = (note.start_ppqn / grid_ppqn) * grid_ppqn;
        uint32_t grid_end   = grid_start + grid_ppqn;
        uint32_t target = (note.start_ppqn - grid_start < grid_end - note.start_ppqn)
                          ? grid_start : grid_end;

        double diff = static_cast<double>(target) - static_cast<double>(note.start_ppqn);
        note.start_ppqn += static_cast<uint32_t>(diff * strength);
    }
}

// ─── Humanize ──────────────────────────────────────────────────

void MidiClip::humanize(double timing_amount, double velocity_amount) {
    timing_amount   = std::clamp(timing_amount, 0.0, 1.0);
    velocity_amount = std::clamp(velocity_amount, 0.0, 1.0);

    std::random_device rd;
    std::mt19937 gen(rd());

    for (auto& note : notes_) {
        // Timing jitter — normal distribution around position
        if (timing_amount > 0.0) {
            std::normal_distribution<double> timing_dist(0.0, timing_amount * 10.0);
            int32_t jitter = static_cast<int32_t>(std::round(timing_dist(gen)));
            if (jitter > 0 && static_cast<uint32_t>(jitter) < note.start_ppqn) {
                note.start_ppqn -= static_cast<uint32_t>(jitter);
            } else if (jitter < 0) {
                note.start_ppqn += static_cast<uint32_t>(-jitter);
            }
        }

        // Velocity variation — uniform around current velocity
        if (velocity_amount > 0.0) {
            std::uniform_int_distribution<int> vel_dist(
                -static_cast<int>(velocity_amount * 20.0),
                 static_cast<int>(velocity_amount * 20.0));
            int vel_delta = vel_dist(gen);
            note.velocity = static_cast<uint8_t>(
                std::clamp(static_cast<int>(note.velocity) + vel_delta, 0, 127));
        }
    }
}

// ─── Transpose ─────────────────────────────────────────────────

void MidiClip::transpose(int8_t semitones) {
    for (auto& note : notes_) {
        int transposed = static_cast<int>(note.note) + semitones;
        note.note = static_cast<uint8_t>(std::clamp(transposed, 0, 127));
    }
}

// ─── Serialization ─────────────────────────────────────────────

std::vector<uint8_t> MidiClip::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(256);

    // Helper: append raw bytes of any trivially-copyable type
    auto append = [&](const auto& val) {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&val);
        data.insert(data.end(), bytes, bytes + sizeof(val));
    };

    // Metadata
    append(length_ppqn);
    append(loop_start_ppqn);
    append(loop_end_ppqn);

    // Notes count
    uint32_t note_count = static_cast<uint32_t>(notes_.size());
    append(note_count);

    for (const auto& note : notes_) {
        append(note.start_ppqn);
        append(note.duration_ppqn);
        append(note.note);
        append(note.velocity);
        append(note.channel);
        append(note.release_velocity);
        append(note.mpe.pitch_bend);
        append(note.mpe.timbre);
        append(note.mpe.pressure);

        uint32_t ev_count = static_cast<uint32_t>(note.note_events.size());
        append(ev_count);
        for (const auto& ev : note.note_events) {
            append(ev.type);
            append(ev.channel);
            append(ev.data1);
            append(ev.data2);
            append(ev.timestamp_us);
            append(ev.ppqn_position);
            uint32_t sysex_size = static_cast<uint32_t>(ev.sysex_data.size());
            append(sysex_size);
            if (sysex_size > 0) {
                data.insert(data.end(), ev.sysex_data.begin(), ev.sysex_data.end());
            }
        }
    }

    // Events count
    uint32_t event_count = static_cast<uint32_t>(events_.size());
    append(event_count);
    for (const auto& ev : events_) {
        append(ev.type);
        append(ev.channel);
        append(ev.data1);
        append(ev.data2);
        append(ev.timestamp_us);
        append(ev.ppqn_position);
        uint32_t sysex_size = static_cast<uint32_t>(ev.sysex_data.size());
        append(sysex_size);
        if (sysex_size > 0) {
            data.insert(data.end(), ev.sysex_data.begin(), ev.sysex_data.end());
        }
    }

    return data;
}

MidiClip MidiClip::deserialize(const std::vector<uint8_t>& data) {
    MidiClip clip;
    size_t offset = 0;

    // Helper: read bytes into any trivially-copyable type
    auto read_bytes = [&](void* val, size_t size) -> bool {
        if (offset + size > data.size()) return false;
        std::memcpy(val, data.data() + offset, size);
        offset += size;
        return true;
    };

    // Metadata
    if (!read_bytes(&clip.length_ppqn, sizeof(clip.length_ppqn))) return clip;
    if (!read_bytes(&clip.loop_start_ppqn, sizeof(clip.loop_start_ppqn))) return clip;
    if (!read_bytes(&clip.loop_end_ppqn, sizeof(clip.loop_end_ppqn))) return clip;

    // Notes
    uint32_t note_count = 0;
    if (!read_bytes(&note_count, sizeof(note_count))) return clip;
    clip.notes_.reserve(note_count);

    for (uint32_t i = 0; i < note_count; ++i) {
        MidiNote note;
        read_bytes(&note.start_ppqn, sizeof(note.start_ppqn));
        read_bytes(&note.duration_ppqn, sizeof(note.duration_ppqn));
        read_bytes(&note.note, sizeof(note.note));
        read_bytes(&note.velocity, sizeof(note.velocity));
        read_bytes(&note.channel, sizeof(note.channel));
        read_bytes(&note.release_velocity, sizeof(note.release_velocity));
        read_bytes(&note.mpe.pitch_bend, sizeof(note.mpe.pitch_bend));
        read_bytes(&note.mpe.timbre, sizeof(note.mpe.timbre));
        read_bytes(&note.mpe.pressure, sizeof(note.mpe.pressure));

        uint32_t ev_count = 0;
        read_bytes(&ev_count, sizeof(ev_count));
        note.note_events.reserve(ev_count);
        for (uint32_t j = 0; j < ev_count; ++j) {
            MidiEvent ev{};
            read_bytes(&ev.type, sizeof(ev.type));
            read_bytes(&ev.channel, sizeof(ev.channel));
            read_bytes(&ev.data1, sizeof(ev.data1));
            read_bytes(&ev.data2, sizeof(ev.data2));
            read_bytes(&ev.timestamp_us, sizeof(ev.timestamp_us));
            read_bytes(&ev.ppqn_position, sizeof(ev.ppqn_position));
            uint32_t sysex_size = 0;
            read_bytes(&sysex_size, sizeof(sysex_size));
            if (sysex_size > 0 && offset + sysex_size <= data.size()) {
                ev.sysex_data.assign(data.begin() + static_cast<ptrdiff_t>(offset),
                                     data.begin() + static_cast<ptrdiff_t>(offset + sysex_size));
                offset += sysex_size;
            }
            note.note_events.push_back(std::move(ev));
        }
        clip.notes_.push_back(note);
    }

    // Events
    uint32_t event_count = 0;
    read_bytes(&event_count, sizeof(event_count));
    clip.events_.reserve(event_count);
    for (uint32_t i = 0; i < event_count; ++i) {
        MidiEvent ev{};
        read_bytes(&ev.type, sizeof(ev.type));
        read_bytes(&ev.channel, sizeof(ev.channel));
        read_bytes(&ev.data1, sizeof(ev.data1));
        read_bytes(&ev.data2, sizeof(ev.data2));
        read_bytes(&ev.timestamp_us, sizeof(ev.timestamp_us));
        read_bytes(&ev.ppqn_position, sizeof(ev.ppqn_position));
        uint32_t sysex_size = 0;
        read_bytes(&sysex_size, sizeof(sysex_size));
        if (sysex_size > 0 && offset + sysex_size <= data.size()) {
            ev.sysex_data.assign(data.begin() + static_cast<ptrdiff_t>(offset),
                                 data.begin() + static_cast<ptrdiff_t>(offset + sysex_size));
            offset += sysex_size;
        }
        clip.events_.push_back(std::move(ev));
    }

    return clip;
}

} // namespace aria
