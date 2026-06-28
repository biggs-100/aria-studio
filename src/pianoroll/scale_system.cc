#include "scale_system.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Built-in scale definitions
// ═══════════════════════════════════════════════════════════════

const Scale ScaleSystem::MAJOR          { "Major",          {0, 2, 4, 5, 7, 9, 11} };
const Scale ScaleSystem::MINOR          { "Minor",          {0, 2, 3, 5, 7, 8, 10} };
const Scale ScaleSystem::HARMONIC_MINOR  { "Harmonic Minor", {0, 2, 3, 5, 7, 8, 11} };
const Scale ScaleSystem::MELODIC_MINOR   { "Melodic Minor",  {0, 2, 3, 5, 7, 9, 11} };
const Scale ScaleSystem::PENTATONIC_MAJOR{ "Pentatonic Major",{0, 2, 4, 7, 9} };
const Scale ScaleSystem::PENTATONIC_MINOR{ "Pentatonic Minor",{0, 3, 5, 7, 10} };
const Scale ScaleSystem::BLUES          { "Blues",          {0, 3, 5, 6, 7, 10} };
const Scale ScaleSystem::DORIAN         { "Dorian",         {0, 2, 3, 5, 7, 9, 10} };
const Scale ScaleSystem::PHRYGIAN       { "Phrygian",       {0, 1, 3, 5, 7, 8, 10} };
const Scale ScaleSystem::LYDIAN         { "Lydian",         {0, 2, 4, 6, 7, 9, 11} };
const Scale ScaleSystem::MIXOLYDIAN     { "Mixolydian",     {0, 2, 4, 5, 7, 9, 10} };
const Scale ScaleSystem::LOCRIAN        { "Locrian",        {0, 1, 3, 5, 6, 8, 10} };
const Scale ScaleSystem::WHOLE_TONE     { "Whole Tone",     {0, 2, 4, 6, 8, 10} };
const Scale ScaleSystem::DIMINISHED     { "Diminished",     {0, 2, 3, 5, 6, 8, 9, 11} };
const Scale ScaleSystem::CHROMATIC      { "Chromatic",      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11} };

// ═══════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════

void ScaleSystem::set_scale(uint8_t root, const Scale& scale) {
    root_ = root;
    scale_ = &scale;
}

void ScaleSystem::set_enabled(bool enabled) {
    enabled_ = enabled;
}

// ═══════════════════════════════════════════════════════════════
// Queries
// ═══════════════════════════════════════════════════════════════

bool ScaleSystem::is_in_scale(uint8_t note) const {
    if (!enabled_) return true;
    uint8_t pc = pitch_class(note);
    uint8_t root_pc = pitch_class(root_);
    int semitone = (static_cast<int>(pc) - static_cast<int>(root_pc) + 12) % 12;

    for (int interval : scale_->intervals) {
        if (semitone == interval) return true;
    }
    return false;
}

uint8_t ScaleSystem::snap_to_scale(uint8_t note) const {
    if (!enabled_ || scale_->intervals.empty()) return note;

    uint8_t pc = pitch_class(note);
    uint8_t root_pc = pitch_class(root_);
    int semitone = (static_cast<int>(pc) - static_cast<int>(root_pc) + 12) % 12;

    // Find the closest interval in the scale.
    int best_interval = scale_->intervals[0];
    int best_dist = std::abs(semitone - best_interval);

    for (int interval : scale_->intervals) {
        int dist = std::abs(semitone - interval);
        if (dist < best_dist) {
            best_dist = dist;
            best_interval = interval;
        }

        // Also check wrap-around: the same interval an octave above/below.
        int dist_up = std::abs(semitone - (interval + 12));
        if (dist_up < best_dist) {
            best_dist = dist_up;
            best_interval = interval + 12;
        }
        int dist_dn = std::abs(semitone - (interval - 12));
        if (dist_dn < best_dist) {
            best_dist = dist_dn;
            best_interval = interval - 12;
        }
    }

    // Compute the snapped pitch-class, then rebuild the MIDI note.
    int snapped_pc = (static_cast<int>(root_pc) + best_interval) % 12;
    if (snapped_pc < 0) snapped_pc += 12;

    uint8_t oct = octave(note);
    int result = static_cast<int>(oct) * 12 + snapped_pc;

    // Handle wrap-around at note boundaries.
    if (result > 127) {
        result = oct * 12 + snapped_pc - 12;
        if (result < 0) result = 0;
    }
    if (result < 0) {
        result = oct * 12 + snapped_pc + 12;
        if (result > 127) result = 127;
    }

    return static_cast<uint8_t>(result);
}

int ScaleSystem::scale_degree(uint8_t note) const {
    if (!enabled_) return 0;

    uint8_t pc = pitch_class(note);
    uint8_t root_pc = pitch_class(root_);
    int semitone = (static_cast<int>(pc) - static_cast<int>(root_pc) + 12) % 12;

    for (size_t i = 0; i < scale_->intervals.size(); ++i) {
        if (semitone == scale_->intervals[i]) return static_cast<int>(i);
    }
    return -1;  // Not in scale
}

std::vector<uint8_t> ScaleSystem::scale_notes(uint8_t octave_low,
                                              uint8_t octave_high) const {
    std::vector<uint8_t> result;
    if (scale_->intervals.empty()) return result;

    uint8_t root_pc = pitch_class(root_);

    // Estimate capacity to reduce reallocation.
    result.reserve(scale_->intervals.size() * (octave_high - octave_low + 1));

    for (uint8_t oct = octave_low; oct <= octave_high; ++oct) {
        for (int interval : scale_->intervals) {
            int note = static_cast<int>(oct) * 12 + static_cast<int>(root_pc) + interval;
            if (note >= 0 && note <= 127) {
                result.push_back(static_cast<uint8_t>(note));
            }
        }
    }
    return result;
}

} // namespace aria
