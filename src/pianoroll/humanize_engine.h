#ifndef ARIA_PIANOROLL_HUMANIZE_ENGINE_H
#define ARIA_PIANOROLL_HUMANIZE_ENGINE_H

#include "note.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria {

// ─── HumanizeSettings ───────────────────────────────────────────

/// Settings controlling timing, velocity, and duration randomization.
struct HumanizeSettings {
    bool    randomize_start    = true;
    bool    randomize_duration = false;
    bool    randomize_velocity = true;

    double  timing_amount      = 30;   // PPQN standard deviation (0–120)
    double  velocity_amount    = 15;   // Velocity std deviation (0–63)
    double  duration_amount    = 10;   // Duration std deviation in PPQN (0–60)

    uint32_t seed              = 0;    // 0 = random seed
};

// ─── HumanizeEngine ─────────────────────────────────────────────

/// Adds humanisation (subtle random variation) to note timing, velocity,
/// and duration. Also supports groove templates extracted from real
/// performances.
class HumanizeEngine {
public:
    // ─── Groove template ──────────────────────────────────────

    /// A groove template extracted from a real performance.
    struct GrooveTemplate {
        std::string             name;
        std::vector<double>     timing_offsets;       // PPQN offsets per grid pos
        std::vector<uint8_t>    velocity_multipliers; // 0–255 multiplier per grid pos
    };

    // ─── Humanize ─────────────────────────────────────────────

    /// Randomize timing, velocity, and/or duration of notes in-place.
    void humanize(std::vector<Note*>& notes, const HumanizeSettings& settings);

    // ─── Groove ───────────────────────────────────────────────

    /// Apply a groove template to notes, aligning them to the template grid.
    void apply_groove(std::vector<Note*>& notes, const GrooveTemplate& groove);

    // ─── Built-in groove templates ────────────────────────────

    static GrooveTemplate shuffle_swing(double amount);
    static GrooveTemplate latin();
    static GrooveTemplate funk();
    static GrooveTemplate hiphop();
};

} // namespace aria

#endif // ARIA_PIANOROLL_HUMANIZE_ENGINE_H
