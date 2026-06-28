#ifndef ARIA_PIANOROLL_NOTE_H
#define ARIA_PIANOROLL_NOTE_H

#include "midi/midi_types.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aria {

// ─── Rgba ──────────────────────────────────────────────────────

/// Float-based RGBA colour (0.0–1.0), used by the GPU rendering layer.
struct Rgba {
    float r = 0.8f;
    float g = 0.8f;
    float b = 0.8f;
    float a = 1.0f;

    constexpr Rgba() = default;
    constexpr Rgba(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    /// Construct from 8-bit channels (0–255).
    static constexpr Rgba from_u8(uint8_t r8, uint8_t g8, uint8_t b8, uint8_t a8 = 0xFF) {
        return Rgba{r8 / 255.0f, g8 / 255.0f, b8 / 255.0f, a8 / 255.0f};
    }

    /// Construct from a 32-bit ARGB hex value (e.g. 0xFF4A9EFF).
    static constexpr Rgba from_argb(uint32_t argb) {
        return Rgba{
            ((argb >> 16) & 0xFF) / 255.0f,
            ((argb >> 8)  & 0xFF) / 255.0f,
            ((argb)       & 0xFF) / 255.0f,
            ((argb >> 24) & 0xFF) / 255.0f,
        };
    }
};

// ─── Rect (axis-aligned) ───────────────────────────────────────

/// A 2D axis-aligned rectangle — used for viewport culling and rendering bounds.
struct Rect {
    double x      = 0;
    double y      = 0;
    double width  = 0;
    double height = 0;

    double left()   const { return x; }
    double right()  const { return x + width; }
    double top()    const { return y; }
    double bottom() const { return y + height; }

    bool contains(double px, double py) const {
        return px >= left() && px <= right() && py >= top() && py <= bottom();
    }

    bool overlaps(const Rect& o) const {
        return left() < o.right() && right() > o.left()
            && top() < o.bottom() && bottom() > o.top();
    }

    static Rect from_ltrb(double l, double t, double r, double b) {
        return Rect{l, t, r - l, b - t};
    }
};

// ─── NoteID ────────────────────────────────────────────────────

/// Globally unique note identifier.
/// Format: 40 bits Unix timestamp (seconds) | 24 bits counter.
struct NoteID {
    uint64_t value = 0;

    constexpr bool operator==(const NoteID& o) const { return value == o.value; }
    constexpr bool operator!=(const NoteID& o) const { return value != o.value; }
    constexpr bool operator<(const NoteID& o)  const { return value <  o.value; }
};

// ─── NoteExpressionEvent ───────────────────────────────────────

/// A single expression event embedded within a note (CC, pitch bend, etc.).
struct NoteExpressionEvent {
    uint64_t offset_ppqn    = 0;  // Offset from note start (PPQN)
    uint8_t  type           = 0;  // CC, PitchBend, ChannelPressure, PolyPressure
    uint8_t  cc_number      = 0;  // If type == CC
    uint16_t value          = 0;  // 14-bit resolution (0–16383)
    uint8_t  interpolation  = 0;  // 0=Step, 1=Linear, 2=Bezier, 3=Smooth
};

// ─── Note ──────────────────────────────────────────────────────

/// A single note in the piano roll — rich object with position, velocity,
/// expression data, visual properties, and MPE.
struct Note {
    // ── Core ─────────────────────────────────────────────────
    NoteID  id;
    uint64_t start_ppqn     = 0;
    uint64_t duration_ppqn  = 0;
    uint8_t  key            = 60;   // MIDI note number 0–127
    uint8_t  velocity       = 100;  // 0–127
    uint8_t  release_velocity = 64; // 0–127
    uint8_t  channel        = 0;    // MIDI channel 0–15

    // ── Visual ───────────────────────────────────────────────
    Rgba   color;           // Per-note colour (inherits from track if default)
    float  opacity   = 1.0f;  // 0.0–1.0
    bool   muted     = false;
    bool   locked    = false;

    // ── Expression ───────────────────────────────────────────
    MPEExpression                    mpe;
    std::vector<NoteExpressionEvent> expressions;

    // ── Metadata ─────────────────────────────────────────────
    std::string label;
};

// ─── NoteColor (derived colour for rendering) ──────────────────

/// Pre-computed colour palette for a note, used by the renderer.
struct NoteColor {
    Rgba body;
    Rgba velocity;
    Rgba outline;
    Rgba shadow;
    Rgba muted_overlay;
    float opacity = 1.0f;

    static NoteColor from_note(const Note& note, const Rgba& track_color);
};

} // namespace aria

// ─── Hash specialisations ──────────────────────────────────────

template<>
struct std::hash<aria::NoteID> {
    size_t operator()(const aria::NoteID& id) const noexcept {
        return std::hash<uint64_t>{}(id.value);
    }
};

#endif // ARIA_PIANOROLL_NOTE_H
