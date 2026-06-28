#include "humanize_engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Random helpers
// ═══════════════════════════════════════════════════════════════

/// A simple normal-distribution random generator (Box-Muller).
/// Wraps std::mt19937 for reproducibility.
static double gaussian_random(double mean, double stddev, std::mt19937& rng) {
    std::normal_distribution<double> dist(mean, stddev);
    return dist(rng);
}

/// Clamp a double to [min, max].
static double clamp_double(double v, double min, double max) {
    return std::max(min, std::min(max, v));
}

// ═══════════════════════════════════════════════════════════════
// Humanize
// ═══════════════════════════════════════════════════════════════

void HumanizeEngine::humanize(std::vector<Note*>& notes,
                               const HumanizeSettings& settings)
{
    if (notes.empty()) return;

    // Seed the RNG.
    uint32_t effective_seed = settings.seed;
    if (effective_seed == 0) {
        effective_seed = static_cast<uint32_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
    std::mt19937 rng(effective_seed);

    for (Note* n : notes) {
        if (!n) continue;

        // Randomize start timing.
        if (settings.randomize_start && settings.timing_amount > 0.0) {
            double offset = gaussian_random(0.0, settings.timing_amount, rng);
            int64_t new_start = static_cast<int64_t>(n->start_ppqn) +
                                static_cast<int64_t>(std::round(offset));
            n->start_ppqn = static_cast<uint64_t>(std::max<int64_t>(0, new_start));
        }

        // Randomize duration.
        if (settings.randomize_duration && settings.duration_amount > 0.0) {
            double offset = gaussian_random(0.0, settings.duration_amount, rng);
            int64_t new_dur = static_cast<int64_t>(n->duration_ppqn) +
                              static_cast<int64_t>(std::round(offset));
            n->duration_ppqn = static_cast<uint64_t>(std::max<int64_t>(1, new_dur));
        }

        // Randomize velocity.
        if (settings.randomize_velocity && settings.velocity_amount > 0.0) {
            double offset = gaussian_random(0.0, settings.velocity_amount, rng);
            int new_vel = static_cast<int>(std::round(
                static_cast<double>(n->velocity) + offset));
            n->velocity = static_cast<uint8_t>(std::clamp(new_vel, 0, 127));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Groove application
// ═══════════════════════════════════════════════════════════════

void HumanizeEngine::apply_groove(std::vector<Note*>& notes,
                                   const GrooveTemplate& groove)
{
    if (notes.empty() || groove.timing_offsets.empty()) return;

    for (Note* n : notes) {
        if (!n) continue;

        // Determine grid position index.
        uint64_t grid_size = 240;  // Assume 16th-note grid
        size_t grid_idx = static_cast<size_t>(n->start_ppqn / grid_size);

        // Apply timing offset if within template range.
        if (grid_idx < groove.timing_offsets.size()) {
            double offset = groove.timing_offsets[grid_idx];
            int64_t new_start = static_cast<int64_t>(n->start_ppqn) +
                                static_cast<int64_t>(std::round(offset));
            n->start_ppqn = static_cast<uint64_t>(std::max<int64_t>(0, new_start));
        }

        // Apply velocity multiplier if within template range.
        if (grid_idx < groove.velocity_multipliers.size()) {
            double mult = static_cast<double>(groove.velocity_multipliers[grid_idx]) / 128.0;
            int new_vel = static_cast<int>(std::round(
                static_cast<double>(n->velocity) * mult));
            n->velocity = static_cast<uint8_t>(std::clamp(new_vel, 0, 127));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Built-in groove templates
// ═══════════════════════════════════════════════════════════════

HumanizeEngine::GrooveTemplate HumanizeEngine::shuffle_swing(double amount) {
    GrooveTemplate gt;
    gt.name = "Shuffle/Swing";

    // 16 positions per bar (16th-note grid).
    int positions = 16;
    gt.timing_offsets.reserve(positions);
    gt.velocity_multipliers.reserve(positions);

    double swing_amount = std::clamp(amount, 0.0, 1.0);

    for (int i = 0; i < positions; ++i) {
        if (i % 2 == 1) {
            // Off-beat: delay by swing amount (up to ~40 PPQN at 16th = 240)
            gt.timing_offsets.push_back(swing_amount * 40.0);
        } else {
            gt.timing_offsets.push_back(0.0);
        }
        // Velocity: slightly quieter on upbeats.
        if (i % 2 == 1) {
            gt.velocity_multipliers.push_back(static_cast<uint8_t>(110 - static_cast<int>(swing_amount * 20)));
        } else {
            gt.velocity_multipliers.push_back(128);  // 1.0x
        }
    }

    return gt;
}

HumanizeEngine::GrooveTemplate HumanizeEngine::latin() {
    GrooveTemplate gt;
    gt.name = "Latin";

    // 16 positions — clave-inspired offsets.
    double offsets[16] = {
         0.0,   5.0,  -3.0,   8.0,
        -2.0,   0.0,   6.0,   0.0,
         3.0,  -4.0,   0.0,   7.0,
        -3.0,   2.0,   0.0,   5.0
    };
    uint8_t velocities[16] = {
        128, 120, 110, 125,
        115, 128, 118, 128,
        122, 108, 128, 120,
        112, 124, 128, 116
    };

    gt.timing_offsets.assign(offsets, offsets + 16);
    gt.velocity_multipliers.assign(velocities, velocities + 16);

    return gt;
}

HumanizeEngine::GrooveTemplate HumanizeEngine::funk() {
    GrooveTemplate gt;
    gt.name = "Funk";

    // 16 positions — syncopated funk offsets.
    double offsets[16] = {
         0.0,  12.0,  -5.0,  15.0,
        -8.0,   0.0,  10.0,   3.0,
         5.0,  -6.0,   0.0,  14.0,
        -4.0,   8.0,  -2.0,   0.0
    };
    uint8_t velocities[16] = {
        128, 130, 100, 135,
         90, 128, 120, 105,
        115,  95, 128, 125,
        102, 118, 110, 128
    };

    gt.timing_offsets.assign(offsets, offsets + 16);
    gt.velocity_multipliers.assign(velocities, velocities + 16);

    return gt;
}

HumanizeEngine::GrooveTemplate HumanizeEngine::hiphop() {
    GrooveTemplate gt;
    gt.name = "Hip-Hop";

    // 16 positions — laid-back hip-hop feel.
    double offsets[16] = {
         0.0,  18.0,   0.0,  22.0,
         5.0,   0.0,  15.0,   0.0,
         3.0,  20.0,   0.0,  25.0,
         8.0,   0.0,  12.0,   0.0
    };
    uint8_t velocities[16] = {
        128, 115, 128, 110,
        120, 128, 118, 128,
        125, 112, 128, 108,
        122, 128, 116, 128
    };

    gt.timing_offsets.assign(offsets, offsets + 16);
    gt.velocity_multipliers.assign(velocities, velocities + 16);

    return gt;
}

} // namespace aria
