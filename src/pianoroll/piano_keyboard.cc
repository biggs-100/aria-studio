#include "piano_keyboard.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Black key offsets (horizontal position within a white-key span)
// ═══════════════════════════════════════════════════════════════

float PianoKeyboard::black_key_offset(uint8_t key) {
    // Offsets are expressed as a fraction of WHITE_KEY_WIDTH from the
    // left edge of the corresponding white key.
    // Pattern: C#, D#, F#, G#, A#
    switch (key % 12) {
        case 1:  return WHITE_KEY_WIDTH * 0.60f;  // C#
        case 3:  return WHITE_KEY_WIDTH * 1.55f;  // D#
        case 6:  return WHITE_KEY_WIDTH * 3.60f;  // F#
        case 8:  return WHITE_KEY_WIDTH * 4.55f;  // G#
        case 10: return WHITE_KEY_WIDTH * 5.55f;  // A#
        default: return 0.0f;
    }
}

// ═══════════════════════════════════════════════════════════════
// Rendering
// ═══════════════════════════════════════════════════════════════

void PianoKeyboard::render(SkiaCanvas* canvas, const Rect& bounds,
                            uint8_t key_low, uint8_t key_high,
                            const Rgba& theme)
{
    (void)canvas;
    (void)bounds;
    (void)key_low;
    (void)key_high;
    (void)theme;

    // TODO: Full implementation once SkiaCanvas is concrete.
    //
    // Algorithm:
    //   1. Compute key range (key_low → key_high).
    //   2. For each key:
    //      a. White keys: draw a rectangle covering the full width.
    //      b. Black keys: draw a narrower rectangle inset from the
    //         right edge of the corresponding white key.
    //   3. White keys use kKeyWhite (#FFFFFF).
    //   4. Black keys use kKeyBlack (#333333).
    //   5. The rightmost 1 px border uses the grid background colour
    //      to simulate a separator line.
}

// ═══════════════════════════════════════════════════════════════
// Hit testing
// ═══════════════════════════════════════════════════════════════

uint8_t PianoKeyboard::key_at_y(float y) const {
    // The keyboard is rendered with each semitone occupying
    // `pixels_per_semitone_y` pixels. Black keys share the same
    // vertical space as the preceding white key.
    int key = static_cast<int>(std::floor(y / WHITE_KEY_WIDTH));
    return static_cast<uint8_t>(std::clamp(key, 0, 127));
}

} // namespace aria
