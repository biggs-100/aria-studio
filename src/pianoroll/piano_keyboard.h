#ifndef ARIA_PIANOROLL_PIANO_KEYBOARD_H
#define ARIA_PIANOROLL_PIANO_KEYBOARD_H

#include "note.h"

#include <cstdint>

namespace aria {

// Forward declarations from the graphics layer.
class SkiaCanvas;
struct Theme;

/// Vertical piano keyboard rendered to the left of the note grid.
///
/// Draws white and black keys for the visible key range and provides
/// Y-coordinate to MIDI-key mapping for hit-testing.
class PianoKeyboard {
public:
    /// Render the keyboard within the given bounds for the visible key range.
    void render(SkiaCanvas* canvas, const Rect& bounds,
                uint8_t key_low, uint8_t key_high, const Rgba& theme);

    /// Return the MIDI key at the given Y offset within the keyboard bounds.
    uint8_t key_at_y(float y) const;

    // ─── Sizing constants ───────────────────────────────────

    static constexpr float WHITE_KEY_WIDTH  = 24.0f;
    static constexpr float BLACK_KEY_WIDTH  = 14.0f;

    /// Return true if a MIDI key is a "black key" (accidental).
    static bool is_black_key(uint8_t key) {
        constexpr bool black[] = {
            false, true,  false, true,  false, false,
            true,  false, true,  false, true,  false
        };
        return black[key % 12];
    }

private:
    /// Pre-computed pixel positions of black keys within a white-key band.
    static float black_key_offset(uint8_t key);
};

} // namespace aria

#endif // ARIA_PIANOROLL_PIANO_KEYBOARD_H
