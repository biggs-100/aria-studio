#include "midi_clock.h"
#include <cmath>
#include <algorithm>

namespace aria {

MidiClock::MidiClock() = default;

void MidiClock::set_role(Role role) {
    role_ = role;
    if (role_ == Role::Off) {
        running_ = false;
    }
}

void MidiClock::start() {
    if (role_ != Role::Off) {
        running_ = true;
        tick_counter_ = 0;
        accumulated_phase_ = 0.0;
        ppm_error_ = 0.0;
    }
}

void MidiClock::stop() {
    running_ = false;
}

void MidiClock::process(uint32_t frames, double bpm) {
    if (!running_ || role_ == Role::Off || bpm <= 0.0) return;

    static constexpr double kSampleRate = 48000.0;

    // Phase accumulator approach:
    //   ticks_per_second = (bpm / 60.0) * TICKS_PER_QN
    //   ticks_per_frame  = ticks_per_second / sample_rate
    double ticks_per_second = (bpm / 60.0) * static_cast<double>(TICKS_PER_QN);
    double ticks_per_frame  = ticks_per_second / kSampleRate;

    accumulated_phase_ += ticks_per_frame * static_cast<double>(frames);

    // Extract integer ticks from accumulator
    uint32_t expected_ticks = static_cast<uint32_t>(accumulated_phase_);
    accumulated_phase_ -= static_cast<double>(expected_ticks);

    // Fire callbacks for each tick
    for (uint32_t i = 0; i < expected_ticks; ++i) {
        ++tick_counter_;
        if (on_tick) on_tick();

        // Every TICKS_PER_QN ticks = 1 quarter note (beat)
        if (tick_counter_ % TICKS_PER_QN == 0) {
            if (on_beat) on_beat();

            // Every 4 beats = 1 bar
            if (tick_counter_ % (TICKS_PER_QN * 4) == 0) {
                if (on_bar) on_bar();
            }
        }
    }

    // Calculate PPM error
    double ideal_phase = ticks_per_frame * static_cast<double>(frames);
    if (ideal_phase > 0.0) {
        ppm_error_ = (accumulated_phase_ / ideal_phase) * 1'000'000.0;
    }
}

void MidiClock::reset() {
    tick_counter_ = 0;
    accumulated_phase_ = 0.0;
    ppm_error_ = 0.0;
    running_ = false;
}

} // namespace aria
