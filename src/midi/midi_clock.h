#ifndef ARIA_MIDI_CLOCK_H
#define ARIA_MIDI_CLOCK_H

#include <cstdint>
#include <functional>

namespace aria {

/// MIDI Clock generator and synchronizer.
///
/// Generates or follows MIDI clock signals (24 ticks per quarter note).
/// Can act as internal master or external slave. Uses a phase accumulator
/// for sample-accurate tick generation.
class MidiClock {
public:
    enum class Role {
        Internal,    ///< ARIA is the clock master
        Slave,       ///< ARIA follows external clock
        Off          ///< No clock sync
    };

    MidiClock();

    /// Set the clock role.
    void set_role(Role role);
    Role role() const { return role_; }

    /// Start/stop clock transmission.
    void start();
    void stop();

    /// Whether the clock is currently running.
    bool is_running() const { return running_; }

    /// Process one audio callback — generates ticks based on BPM.
    /// Called from the audio thread.
    void process(uint32_t frames, double bpm);

    /// Callbacks for clock events.
    std::function<void()> on_tick;
    std::function<void()> on_beat;
    std::function<void()> on_bar;

    /// Timing accuracy — parts per million deviation from ideal.
    double ppm_error() const { return ppm_error_; }

    /// Number of ticks generated since start.
    uint32_t tick_count() const { return tick_counter_; }

    /// Reset clock state.
    void reset();

private:
    /// MIDI clock standard: 24 ticks per quarter note.
    static constexpr uint32_t TICKS_PER_QN = 24;

    Role role_ = Role::Off;
    uint32_t tick_counter_ = 0;
    double accumulated_phase_ = 0.0;
    double ppm_error_ = 0.0;
    bool running_ = false;
};

} // namespace aria

#endif // ARIA_MIDI_CLOCK_H
