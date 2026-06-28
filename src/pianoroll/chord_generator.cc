#include "chord_generator.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <map>
#include <numeric>
#include <random>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Interval tables
// ═══════════════════════════════════════════════════════════════

std::vector<int> ChordGenerator::chord_intervals(Type type) {
    switch (type) {
    case Major:      return {0, 4, 7};
    case Minor:      return {0, 3, 7};
    case Dim:        return {0, 3, 6};
    case Aug:        return {0, 4, 8};
    case Maj7:       return {0, 4, 7, 11};
    case Min7:       return {0, 3, 7, 10};
    case Dom7:       return {0, 4, 7, 10};
    case Dim7:       return {0, 3, 6, 9};
    case Sus2:       return {0, 2, 7};
    case Sus4:       return {0, 5, 7};
    case PowerChord: return {0, 7};
    case Maj9:       return {0, 4, 7, 11, 14};
    case Min9:       return {0, 3, 7, 10, 14};
    case Dom9:       return {0, 4, 7, 10, 14};
    case Maj13:      return {0, 4, 7, 11, 14, 21};
    case Min13:      return {0, 3, 7, 10, 14, 21};
    default:         return {0, 4, 7};  // fallback to Major
    }
}

// ═══════════════════════════════════════════════════════════════
// Voicing transformations
// ═══════════════════════════════════════════════════════════════

std::vector<uint8_t> ChordGenerator::apply_voicing(std::vector<uint8_t> notes,
                                                    Voicing voicing,
                                                    uint8_t root) {
    if (notes.empty()) return notes;

    // Sort the notes (they're already generated close-voiced within one octave).
    std::sort(notes.begin(), notes.end());

    switch (voicing) {
    case Close:
        // Already close-voiced — no transformation needed.
        return notes;

    case Open: {
        // Spread notes across 2 octaves: lowest stays, next goes up 1 octave,
        // then back down, etc.
        std::vector<uint8_t> result;
        result.reserve(notes.size());
        for (size_t i = 0; i < notes.size(); ++i) {
            int n = notes[i];
            if (i % 2 == 1) {
                n += 12;  // Every other note up one octave
            }
            if (n >= 0 && n <= 127) {
                result.push_back(static_cast<uint8_t>(n));
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    case Drop2: {
        // Drop-2: take the 2nd-highest note and lower it one octave.
        if (notes.size() < 4) return notes;
        std::vector<uint8_t> result = notes;
        int drop = static_cast<int>(result[result.size() - 2]) - 12;
        result[result.size() - 2] = static_cast<uint8_t>(std::clamp(drop, 0, 127));
        std::sort(result.begin(), result.end());
        return result;
    }

    case Drop3: {
        // Drop-3: take the 3rd-highest note and lower it one octave.
        if (notes.size() < 4) return notes;
        std::vector<uint8_t> result = notes;
        int drop = static_cast<int>(result[result.size() - 3]) - 12;
        result[result.size() - 3] = static_cast<uint8_t>(std::clamp(drop, 0, 127));
        std::sort(result.begin(), result.end());
        return result;
    }

    case Spread: {
        // Spread: stack notes in a wide arrangement (root, 5th, 3rd an octave up, etc.)
        if (notes.size() < 3) return notes;
        std::vector<uint8_t> result;
        result.reserve(notes.size());

        // Root stays, 3rd and 5th go up an octave, 7th stays, etc.
        for (size_t i = 0; i < notes.size(); ++i) {
            int n = notes[i];
            if (i >= 2 && i <= 4) {
                n += 12;
            }
            if (n >= 0 && n <= 127) {
                result.push_back(static_cast<uint8_t>(n));
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    case Cluster: {
        // Cluster: compress notes into a dense semitone cluster centered on root.
        std::vector<uint8_t> result;
        result.reserve(notes.size());
        result.push_back(root);
        int cluster_pos = static_cast<int>(root);
        for (size_t i = 1; i < notes.size(); ++i) {
            cluster_pos += (i % 2 == 1) ? 1 : 2;  // Alternate between 1 and 2 semitones
            if (cluster_pos >= 0 && cluster_pos <= 127) {
                result.push_back(static_cast<uint8_t>(cluster_pos));
            }
        }
        return result;
    }

    default:
        return notes;
    }
}

// ═══════════════════════════════════════════════════════════════
// Generation
// ═══════════════════════════════════════════════════════════════

std::vector<uint8_t> ChordGenerator::generate(uint8_t root, Type type,
                                               Voicing voicing,
                                               uint8_t octave) const {
    auto intervals = chord_intervals(type);
    std::vector<uint8_t> notes;
    notes.reserve(intervals.size());

    for (int interval : intervals) {
        int n = static_cast<int>(octave) * 12 + static_cast<int>(root % 12) + interval;
        if (n >= 0 && n <= 127) {
            notes.push_back(static_cast<uint8_t>(n));
        }
    }

    return apply_voicing(notes, voicing, root % 12 + octave * 12);
}

std::vector<uint8_t> ChordGenerator::generate_inversion(uint8_t root, Type type,
                                                         int inversion) const {
    auto intervals = chord_intervals(type);
    if (intervals.empty()) return {};

    // Generate close-voiced notes in octave 4.
    uint8_t octave = 4;
    std::vector<uint8_t> notes;
    notes.reserve(intervals.size());

    for (int interval : intervals) {
        int n = static_cast<int>(octave) * 12 + static_cast<int>(root % 12) + interval;
        if (n >= 0 && n <= 127) {
            notes.push_back(static_cast<uint8_t>(n));
        }
    }

    // Apply inversion: rotate notes and move the inverted notes down.
    if (inversion > 0 && !notes.empty()) {
        int actual_inv = inversion % static_cast<int>(notes.size());
        for (int i = 0; i < actual_inv; ++i) {
            int lowered = static_cast<int>(notes[i]) + 12;
            if (lowered <= 127) {
                notes[i] = static_cast<uint8_t>(lowered);
            }
        }
        std::rotate(notes.begin(), notes.begin() + actual_inv, notes.end());
    }

    return notes;
}

// ═══════════════════════════════════════════════════════════════
// Progression generation
// ═══════════════════════════════════════════════════════════════

ChordGenerator::Progression ChordGenerator::generate_progression(
    uint8_t key, const Scale& scale, const std::vector<int>& degrees) const
{
    Progression prog;

    // Map scale degrees to chord types (diatonic triads in major).
    //  I = Major, ii = Minor, iii = Minor, IV = Major, V = Major,
    // vi = Minor, vii° = Dim
    static const Type diatonic_triads[7] = {
        Major, Minor, Minor, Major, Major, Minor, Dim
    };
    static const Type diatonic_sevenths[7] = {
        Maj7, Min7, Min7, Maj7, Dom7, Min7, Dim7
    };

    for (int degree : degrees) {
        // Normalize degree to 0–6 (1-indexed → 0-indexed).
        int idx = ((degree - 1) % 7 + 7) % 7;

        // Find the root note for this degree within the scale.
        auto intervals = scale.intervals;
        if (intervals.empty()) continue;

        int degree_pc = intervals[idx % intervals.size()];
        int root_note = (static_cast<int>(key % 12) + degree_pc) % 12 + 60;  // C4-based

        prog.roots.push_back(static_cast<uint8_t>(root_note));
        prog.chords.push_back(diatonic_triads[idx]);
    }

    return prog;
}

// ═══════════════════════════════════════════════════════════════
// Arpeggiation
// ═══════════════════════════════════════════════════════════════

std::vector<uint64_t> ChordGenerator::arpeggiate(
    const std::vector<uint8_t>& notes,
    uint64_t start, uint64_t dur,
    ArpPattern pattern) const
{
    if (notes.empty()) return {};
    if (dur == 0) dur = 240;

    std::vector<uint64_t> positions;

    switch (pattern) {
    case Up: {
        // Ascending sequential — one pass.
        positions.reserve(notes.size());
        for (size_t i = 0; i < notes.size(); ++i) {
            positions.push_back(start + i * dur);
        }
        break;
    }

    case Down: {
        // Descending sequential.
        positions.reserve(notes.size());
        for (size_t i = 0; i < notes.size(); ++i) {
            positions.push_back(start + i * dur);
        }
        // Notes are in descending order, but positions ascend.
        // We store note order as descending by sorting positions + notes.
        // But positions are just timestamps, so we produce them ascending.
        break;
    }

    case UpDown: {
        // Up then down (exclude top repeat).
        size_t n = notes.size();
        size_t total = (n == 1) ? 1 : (2 * n - 2);
        positions.reserve(total);
        for (size_t i = 0; i < n; ++i) {
            positions.push_back(start + i * dur);
        }
        for (size_t i = n - 2; i > 0; --i) {
            positions.push_back(start + positions.size() * dur);
        }
        if (n > 1) {
            positions.push_back(start + positions.size() * dur);  // Root again
        }
        break;
    }

    case DownUp: {
        // Down then up (exclude bottom repeat).
        size_t n = notes.size();
        size_t total = (n == 1) ? 1 : (2 * n - 2);
        positions.reserve(total);
        for (size_t i = 0; i < n; ++i) {
            positions.push_back(start + i * dur);
        }
        for (size_t i = n - 2; i > 0; --i) {
            positions.push_back(start + positions.size() * dur);
        }
        if (n > 1) {
            positions.push_back(start + positions.size() * dur);
        }
        break;
    }

    case Random: {
        // Random order (simple LCG-based shuffle).
        std::vector<size_t> indices(notes.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::mt19937 rng(random_seed_);
        std::shuffle(indices.begin(), indices.end(), rng);
        random_seed_ = rng();
        positions.reserve(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            positions.push_back(start + i * dur);
        }
        break;
    }

    case Chord: {
        // All notes simultaneously.
        positions.push_back(start);
        break;
    }

    case AsPlayed: {
        // Preserve the order the notes are given.
        positions.reserve(notes.size());
        for (size_t i = 0; i < notes.size(); ++i) {
            positions.push_back(start + i * dur);
        }
        break;
    }

    case ForwardBack: {
        // Ping-pong: full forward then full back.
        size_t n = notes.size();
        size_t total = 2 * n;
        positions.reserve(total);
        for (size_t i = 0; i < n; ++i) {
            positions.push_back(start + i * dur);
        }
        for (size_t i = 0; i < n; ++i) {
            positions.push_back(start + (n + i) * dur);
        }
        break;
    }
    }

    return positions;
}

} // namespace aria
