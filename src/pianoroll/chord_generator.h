#ifndef ARIA_PIANOROLL_CHORD_GENERATOR_H
#define ARIA_PIANOROLL_CHORD_GENERATOR_H

#include "scale_system.h"

#include <cstdint>
#include <vector>

namespace aria {

// ─── ChordGenerator ─────────────────────────────────────────────

/// Generates chord voicings, inversions, progressions, and arpeggios.
///
/// Supports 16 chord types, 6 voicing styles, arbitrary inversions,
/// diatonic progression generation, and 8 arpeggio patterns.
class ChordGenerator {
public:
    // ─── Chord types ──────────────────────────────────────────

    enum Type : uint8_t {
        Major,
        Minor,
        Dim,
        Aug,
        Maj7,
        Min7,
        Dom7,
        Dim7,
        Sus2,
        Sus4,
        PowerChord,
        Maj9,
        Min9,
        Dom9,
        Maj13,
        Min13
    };

    // ─── Voicings ─────────────────────────────────────────────

    enum Voicing : uint8_t {
        Close,    // Notes as close as possible
        Open,     // Spread across octaves
        Drop2,    // Drop-2 voicing
        Drop3,    // Drop-3 voicing
        Spread,   // Wide intervals (> 1 octave)
        Cluster   // Dense semitone/tone clusters
    };

    // ─── Arpeggio patterns ────────────────────────────────────

    enum ArpPattern : uint8_t {
        Up,
        Down,
        UpDown,
        DownUp,
        Random,
        Chord,
        AsPlayed,
        ForwardBack
    };

    // ─── Progression ──────────────────────────────────────────

    /// A complete chord progression with chord types and roots.
    struct Progression {
        std::vector<Type>     chords;
        std::vector<uint8_t>  roots;  // MIDI note numbers
    };

    // ─── Generation ───────────────────────────────────────────

    /// Generate chord notes for a given root, type, voicing, and octave.
    std::vector<uint8_t> generate(uint8_t root, Type type,
                                  Voicing voicing = Close,
                                  uint8_t octave = 4) const;

    /// Generate a specific inversion of a chord.
    /// inversion = 0 (root), 1 (first), 2 (second), etc.
    std::vector<uint8_t> generate_inversion(uint8_t root, Type type,
                                            int inversion) const;

    /// Generate a diatonic chord progression in a given key and scale.
    Progression generate_progression(uint8_t key, const Scale& scale,
                                     const std::vector<int>& degrees) const;

    /// Arpeggiate a set of notes into a sequence of PPQN positions.
    /// @param notes    The chord tones (as MIDI note numbers).
    /// @param start    The PPQN position of the first arpeggio note.
    /// @param dur      The PPQN duration per arpeggio note.
    /// @param pattern  The arpeggio direction/pattern.
    /// @return A vector of PPQN positions, one per note in the pattern.
    std::vector<uint64_t> arpeggiate(const std::vector<uint8_t>& notes,
                                     uint64_t start, uint64_t dur,
                                     ArpPattern pattern) const;

private:
    /// Return the semitone intervals (from root) for a chord type.
    static std::vector<int> chord_intervals(Type type);

    /// Apply a voicing transformation to a set of raw chord notes.
    static std::vector<uint8_t> apply_voicing(std::vector<uint8_t> notes,
                                               Voicing voicing,
                                               uint8_t root);

    /// Seed for Random arpeggio mode.
    mutable uint32_t random_seed_ = 42;
};

} // namespace aria

#endif // ARIA_PIANOROLL_CHORD_GENERATOR_H
