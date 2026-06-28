#ifndef ARIA_PIANOROLL_SCALE_SYSTEM_H
#define ARIA_PIANOROLL_SCALE_SYSTEM_H

#include <cstdint>
#include <string>
#include <vector>

namespace aria {

// ─── Scale ───────────────────────────────────────────────────────

/// A musical scale defined by semitone intervals from the root.
struct Scale {
    std::string name;
    std::vector<int> intervals;  // Semitone intervals from root (ascending)
};

// ─── ScaleSystem ─────────────────────────────────────────────────

/// Manages scale selection, membership testing, and scale-aware snapping.
///
/// Supports 15 common scales including diatonic, pentatonic, blues,
/// modal, whole-tone, diminished, and chromatic.
class ScaleSystem {
public:
    // ─── Built-in scale definitions ───────────────────────────

    static const Scale MAJOR;          // Ionian
    static const Scale MINOR;          // Natural minor (Aeolian)
    static const Scale HARMONIC_MINOR;
    static const Scale MELODIC_MINOR;
    static const Scale PENTATONIC_MAJOR;
    static const Scale PENTATONIC_MINOR;
    static const Scale BLUES;
    static const Scale DORIAN;
    static const Scale PHRYGIAN;
    static const Scale LYDIAN;
    static const Scale MIXOLYDIAN;
    static const Scale LOCRIAN;
    static const Scale WHOLE_TONE;
    static const Scale DIMINISHED;
    static const Scale CHROMATIC;

    // ─── Configuration ────────────────────────────────────────

    /// Set the root note (MIDI note number 0–127) and scale.
    void set_scale(uint8_t root, const Scale& scale);

    /// Enable or disable scale-aware behaviour.
    void set_enabled(bool enabled);
    bool is_enabled() const { return enabled_; }

    // ─── Queries ──────────────────────────────────────────────

    /// Returns true if `note` is a member of the current scale.
    bool is_in_scale(uint8_t note) const;

    /// Snap `note` to the nearest scale tone.
    uint8_t snap_to_scale(uint8_t note) const;

    /// Return the scale degree of `note` (0 = root, -1 = not in scale).
    int scale_degree(uint8_t note) const;

    /// Return all scale tones as MIDI note numbers across octaves.
    std::vector<uint8_t> scale_notes(uint8_t octave_low,
                                     uint8_t octave_high) const;

    // ─── Accessors ────────────────────────────────────────────

    uint8_t root() const { return root_; }
    const Scale& scale() const { return *scale_; }
    std::string scale_name() const { return scale_->name; }

private:
    uint8_t        root_    = 60;  // C4
    const Scale*   scale_   = &MAJOR;
    bool           enabled_ = false;

    /// Return the pitch-class (0–11) for a MIDI note.
    static uint8_t pitch_class(uint8_t note) { return note % 12; }

    /// Return the octave number (0–10) for a MIDI note.
    static uint8_t octave(uint8_t note) { return note / 12; }
};

} // namespace aria

#endif // ARIA_PIANOROLL_SCALE_SYSTEM_H
